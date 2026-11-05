package control

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"log"
	"os"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"

	"gerrit.o-ran-sc.org/r/ric-plt/xapp-frame/pkg/xapp"
)

var (
	gControlData *Control
)

func NewControl() Control {
	file, err := os.OpenFile("/opt/rc.log", os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666)
	if err == nil {
		log.SetOutput(io.MultiWriter(os.Stdout, file))
	} else {
		log.SetOutput(os.Stdout)
	}
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)
	xapp.Logger.SetLevel(4)

	// Carrega configurações de ambiente (com defaults compatíveis com o ns-3)
	ranName := os.Getenv("RC_RAN_NAME")
	if ranName == "" {
		ranName = "gnb_131_133_31000000" // LTE eNB onde o PDCP faz o split
	}
	plmnID := os.Getenv("RC_PLMN_ID")
	if plmnID == "" {
		plmnID = "313131"
	}
	enbID := os.Getenv("RC_ENB_ID")
	if enbID == "" {
		enbID = "36000000"
	}

	drbIDStr := os.Getenv("RC_DRB_ID")
	drbID := int64(1) // DRB 1 é o fluxo eMBB no ns-3
	if drbIDStr != "" {
		if val, err := strconv.ParseInt(drbIDStr, 10, 64); err == nil {
			drbID = val
		}
	}

	autoModeStr := strings.ToLower(strings.TrimSpace(os.Getenv("RC_AUTO_MODE")))
	autoMode := (autoModeStr == "true" || autoModeStr == "1")

	autoInterval := 5
	if val, err := strconv.Atoi(os.Getenv("RC_AUTO_INTERVAL")); err == nil && val > 0 {
		autoInterval = val
	}

	log.Printf("[RC-Init] Target RAN: %s | PLMN: %s | EnbID: %s | Default DRB: %d", ranName, plmnID, enbID, drbID)
	log.Printf("[RC-Init] Auto Mode: %v (Interval: %ds)", autoMode, autoInterval)

	c := Control{
		eventRicControlReqTimePeriod: 5,
		rcChan:                       make(chan *xapp.RMRParams, 1000),
		eventRicControlReqExpiredMap: make(map[int]bool),
		eventRicControlReqExpiredMu:  &sync.Mutex{},
		ricRequestInstanceID:         0,

		ranName: ranName,
		plmnID:  plmnID,
		enbID:   enbID,
		drbID:   drbID,

		autoMode:     autoMode,
		autoInterval: autoInterval,
		autoSplits:   []int64{0, 25, 50, 75, 100},
		autoIndex:    0,
	}

	gControlData = &c
	return c
}

func ReadyCB(i interface{}) {
	gControlData = i.(*Control)
	log.Printf("[RC] >>> RMR is Ready! Starting event loops...")

	go gControlData.controlLoop()

	if gControlData.autoMode {
		go gControlData.startAutoSplitLoop()
	}

	// Escuta comandos via FIFO (/tmp/rc_cmd) para o split_command.sh
	go gControlData.listenCommandPipe()

	// Mantém o CLI interativo no terminal (stdin)
	go gControlData.startInteractiveCLI()
}

func (c *Control) Run() {
	xapp.SetReadyCB(ReadyCB, c)
	xapp.Run(c)
}

func (c *Control) Consume(rp *xapp.RMRParams) (err error) {
	c.rcChan <- rp
	return nil
}

func (c *Control) rmrSend(params *xapp.RMRParams) (err error) {
	if !xapp.Rmr.Send(params, false) {
		err = errors.New("rmr.Send() failed")
		xapp.Logger.Error("Failed to rmrSend: %v", err)
		log.Printf("Failed to rmrSend: %v", err)
	}
	return
}

func (c *Control) controlLoop() {
	for {
		msg := <-c.rcChan
		switch msg.Mtype {
		case 12041: // RIC_CONTROL_ACK
			c.handleControlAck(msg)
		case 12042: // RIC_CONTROL_FAILURE
			c.handleControlFailure(msg)
		default:
			log.Printf("[RC-RMR] Ignored message type: %d", msg.Mtype)
		}
	}
}

func (c *Control) handleControlAck(params *xapp.RMRParams) {
	log.Printf("\033[1;32m[RIC_CONTROL_ACK] Recebido ACK do ns-3 com sucesso! (SubId=%d, RanName=%s)\033[0m",
		params.SubId, params.Meid.RanName)
}

func (c *Control) handleControlFailure(params *xapp.RMRParams) {
	log.Printf("\033[1;31m[RIC_CONTROL_FAILURE] Recebido erro de controle do ns-3 (SubId=%d, RanName=%s)\033[0m",
		params.SubId, params.Meid.RanName)
}

// SendDrbSplitControl monta a mensagem E2SM-RC e envia o RIC_CONTROL_REQ via RMR para o ns-3.
func (c *Control) SendDrbSplitControl(ueID string, drbID int64, splitRatio int64) error {
	ranFuncId := uint16(300)      // E2SM-RC RAN Function ID no ns-3
	ricRequestorID := uint16(1002) // ID do requisitante
	controlStyle := int64(1)       // Style 1: Radio Bearer Control
	controlActionId := int64(6)    // Action 6: DRB Split Ratio Control

	c.eventRicControlReqExpiredMu.Lock()
	c.ricRequestInstanceID++
	reqSN := uint16(c.ricRequestInstanceID)
	c.eventRicControlReqExpiredMu.Unlock()

	log.Printf("[E2SM-RC] Preparando comando -> UE: %s | DRB: %d | Split Ratio: %d%% (LTE: %d%%, NR: %d%%)",
		ueID, drbID, splitRatio, splitRatio, 100-splitRatio)

	e2sm := &E2sm{}
	headerBuf := make([]byte, 256)
	msgBuf := make([]byte, 1024)

	// 1. Codifica E2SM Control Header com o UEID
	encodedHeader, err := e2sm.SetRicControlHeader(headerBuf, []byte(ueID), controlStyle, controlActionId)
	if err != nil {
		log.Printf("[E2SM-RC] Erro ao codificar Header: %v", err)
		return err
	}

	// 2. Codifica E2SM Control Message com DRB ID e Split Ratio
	encodedMsg, err := e2sm.SetRicControlMessageDRB(msgBuf, drbID, splitRatio)
	if err != nil {
		log.Printf("[E2SM-RC] Erro ao codificar Message: %v", err)
		return err
	}

	// 3. Monta envelope E2AP (RIC Control Request)
	e2ap := &E2ap{}
	payloadBuf := make([]byte, 2048)
	payload, err := e2ap.SetRicControlRequestPayload(payloadBuf, ricRequestorID, reqSN, ranFuncId, encodedHeader, encodedMsg)
	if err != nil {
		log.Printf("[E2AP] Erro ao montar payload: %v", err)
		return err
	}

	// 4. Envia mensagem RMR (tipo 12040 = RIC_CONTROL_REQ)
	params := &xapp.RMRParams{
		Mtype:   12040,
		SubId:   -1,
		Payload: payload,
		Meid:    &xapp.RMRMeid{PlmnID: c.plmnID, EnbID: c.enbID, RanName: c.ranName},
	}

	if err := c.rmrSend(params); err != nil {
		log.Printf("[RMR] Falha ao enviar RIC_CONTROL_REQ: %v", err)
		return err
	}

	log.Printf("[E2AP] >>> RIC_CONTROL_REQ enviado com sucesso para %s (ReqID=%d, ReqSN=%d)",
		c.ranName, ricRequestorID, reqSN)
	return nil
}

// listenCommandPipe cria e escuta o FIFO /tmp/rc_cmd para comandos vindos do split_command.sh
func (c *Control) listenCommandPipe() {
	pipePath := "/tmp/rc_cmd"
	os.Remove(pipePath)
	if err := syscall.Mkfifo(pipePath, 0666); err != nil && !os.IsExist(err) {
		log.Printf("[RC-Pipe] Erro ao criar FIFO %s: %v", pipePath, err)
		return
	}
	os.Chmod(pipePath, 0666)

	log.Printf("[RC-Pipe] Escutando comandos em %s...", pipePath)
	for {
		file, err := os.OpenFile(pipePath, os.O_RDONLY, os.ModeNamedPipe)
		if err != nil {
			time.Sleep(1 * time.Second)
			continue
		}

		scanner := bufio.NewScanner(file)
		for scanner.Scan() {
			line := strings.TrimSpace(scanner.Text())
			if line == "" {
				continue
			}
			parts := strings.Fields(line)
			var ueID string = "00001"
			var drbID int64 = c.drbID
			var splitRatio int64 = 50

			if len(parts) == 1 {
				if val, err := strconv.ParseInt(parts[0], 10, 64); err == nil && val >= 0 && val <= 100 {
					splitRatio = val
				}
			} else if len(parts) >= 3 {
				ueID = parts[0]
				if val, err := strconv.ParseInt(parts[1], 10, 64); err == nil {
					drbID = val
				}
				if val, err := strconv.ParseInt(parts[2], 10, 64); err == nil && val >= 0 && val <= 100 {
					splitRatio = val
				}
			}

			log.Printf("[RC-Pipe] Comando recebido: UE=%s, DRB=%d, Split=%d%%", ueID, drbID, splitRatio)
			c.SendDrbSplitControl(ueID, drbID, splitRatio)
		}
		file.Close()
	}
}

// startAutoSplitLoop itera ciclicamente pelos valores de split ratio (0%, 25%, 50%, 75%, 100%)
func (c *Control) startAutoSplitLoop() {
	log.Printf("[RC-Auto] Aguardando 5s para inicializar loop de split automático...")
	time.Sleep(5 * time.Second)
	log.Printf("[RC-Auto] Iniciando loop de split automático a cada %d segundos...", c.autoInterval)

	ticker := time.NewTicker(time.Duration(c.autoInterval) * time.Second)
	defer ticker.Stop()

	defaultUE := "00001"
	for range ticker.C {
		split := c.autoSplits[c.autoIndex]
		c.autoIndex = (c.autoIndex + 1) % len(c.autoSplits)
		log.Printf("[RC-Auto] Disparando Split Ratio automático: %d%%", split)
		c.SendDrbSplitControl(defaultUE, c.drbID, split)
	}
}

// startInteractiveCLI permite que o aluno envie comandos diretamente pelo terminal do pod
func (c *Control) startInteractiveCLI() {
	time.Sleep(2 * time.Second)
	fmt.Println("\n=======================================================")
	fmt.Println("         xApp RAN CONTROL (RC) - Split Ratio           ")
	fmt.Println("=======================================================")
	fmt.Println(" Digite um comando para aplicar o Split Ratio no ns-3: ")
	fmt.Println("   Sintaxe : <ueID> <drbID> <splitRatio>")
	fmt.Println("   Exemplo : 00001 1 50   (50% LTE / 50% NR)")
	fmt.Println("   Exemplo : 00001 1 25   (25% LTE / 75% NR)")
	fmt.Println("   Exemplo : 00001 1 100  (100% LTE / 0% NR)")
	fmt.Println(" Ou apenas o valor do split (usa UE=00001 DRB=1):")
	fmt.Println("   Exemplo : 75")
	fmt.Println("=======================================================\n")

	scanner := bufio.NewScanner(os.Stdin)
	for {
		fmt.Print("rc-xapp> ")
		if !scanner.Scan() {
			break
		}
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}

		parts := strings.Fields(line)
		var ueID string = "00001"
		var drbID int64 = c.drbID
		var splitRatio int64 = 50

		if len(parts) == 1 {
			val, err := strconv.ParseInt(parts[0], 10, 64)
			if err != nil || val < 0 || val > 100 {
				fmt.Println("Erro: Split ratio deve ser um inteiro entre 0 e 100.")
				continue
			}
			splitRatio = val
		} else if len(parts) == 3 {
			ueID = parts[0]
			drbVal, err1 := strconv.ParseInt(parts[1], 10, 64)
			splitVal, err2 := strconv.ParseInt(parts[2], 10, 64)
			if err1 != nil || err2 != nil || splitVal < 0 || splitVal > 100 {
				fmt.Println("Erro: formato inválido. Use: <ueID> <drbID> <splitRatio (0-100)>")
				continue
			}
			drbID = drbVal
			splitRatio = splitVal
		} else {
			fmt.Println("Comando não reconhecido. Use: <ueID> <drbID> <splitRatio> ou apenas <splitRatio>")
			continue
		}

		c.SendDrbSplitControl(ueID, drbID, splitRatio)
	}
}
