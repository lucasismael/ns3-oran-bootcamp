package control

import (
	"errors"
	"io"
	"log"
	"os"
	"strings"
	"sync"
	"time"

	"gerrit.o-ran-sc.org/r/ric-plt/xapp-frame/pkg/xapp"
)

// Control concentra o estado de execução do xApp KPIMon.
type Control struct {
	ranList               []string
	eventCreateExpired    int32
	eventDeleteExpired    int32
	rcChan                chan *xapp.RMRParams
	eventCreateExpiredMap map[string]bool
	eventDeleteExpiredMap map[string]bool
	eventCreateExpiredMu  *sync.Mutex
	eventDeleteExpiredMu  *sync.Mutex
	subRespChan           chan string
	subscribedRanList     map[string]bool
	subscribedRanListMu   *sync.Mutex
}

func init() {
	file, err := os.OpenFile("/opt/kpimon.log", os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666)
	if err != nil {
		xapp.Logger.Error("Cannot open /opt/kpimon.log: %v", err)
		return
	}
	// Escreve simultaneamente no stdout (kubectl logs) e em /opt/kpimon.log
	log.SetOutput(io.MultiWriter(os.Stdout, file))
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)
	xapp.Logger.SetLevel(4)
}

// NewControl inicializa a estrutura de controle do xApp e carrega a lista de nós E2 alvo.
func NewControl() Control {
	return Control{
		ranList:               getRanList(),
		eventCreateExpired:    5,
		eventDeleteExpired:    5,
		rcChan:                make(chan *xapp.RMRParams),
		eventCreateExpiredMap: make(map[string]bool),
		eventDeleteExpiredMap: make(map[string]bool),
		eventCreateExpiredMu:  &sync.Mutex{},
		eventDeleteExpiredMu:  &sync.Mutex{},
		subRespChan:           make(chan string, 10),
		subscribedRanList:     make(map[string]bool),
		subscribedRanListMu:   &sync.Mutex{},
	}
}

// getRanList obtém a lista de nós E2 (eNB/gNB) a subscrever (via variável de ambiente ranList ou padrão do cenário ns-3).
func getRanList() []string {
	str := strings.TrimSpace(os.Getenv("ranList"))
	if str == "" {
		// Nós E2 padrão criados pelo cenário scenario-trainning.cc (eNB CellId 1 e gNB CellId 2)
		str = "gnb_131_133_31000000,gnb_131_133_32000000"
	}
	log.Printf("Configured E2 Nodes (ranList): %s", str)
	return strings.Split(str, ",")
}

// ReadyCB é acionado pelo xapp-frame assim que o barramento RMR está pronto.
func ReadyCB(i interface{}) {
	c := i.(*Control)
	c.startTimerSubReq()
	go c.controlLoop()
}

// Run inicia o ciclo de vida do xApp no xapp-frame.
func (c *Control) Run() {
	log.Printf("Starting KPIMon xApp with %d target E2 Node(s): %v", len(c.ranList), c.ranList)
	if len(c.ranList) > 0 {
		xapp.SetReadyCB(ReadyCB, c)
		xapp.Run(c)
	} else {
		xapp.Logger.Error("gNodeB not set for subscription")
		log.Printf("gNodeB not set for subscription")
	}
}

// startTimerSubReq gerencia o envio periódico de RIC Subscription Request para nós ainda não subscritos.
func (c *Control) startTimerSubReq() {
	timerSR := time.NewTimer(5 * time.Second)
	count := 0

	go func(t *time.Timer) {
		defer timerSR.Stop()
		for {
			<-t.C
			count++

			c.subscribedRanListMu.Lock()
			var unsubscribedList []string
			for _, ranName := range c.ranList {
				if !c.subscribedRanList[ranName] {
					unsubscribedList = append(unsubscribedList, ranName)
				}
			}
			c.subscribedRanListMu.Unlock()

			if len(unsubscribedList) == 0 {
				xapp.Logger.Info("All target E2 Nodes are subscribed. Stopping subscription timer.")
				log.Printf("All target E2 Nodes are subscribed. Stopping subscription timer.")
				break
			}

			log.Printf("Sending RIC_SUB_REQ to unsubscribed E2 Nodes (attempt %d/%d): %v",
				count, MAX_SUBSCRIPTION_ATTEMPTS, unsubscribedList)
			if err := c.sendRicSubRequest(1001, 0, 200, unsubscribedList); err != nil {
				xapp.Logger.Error("Failed to send subscription request: %v", err)
				log.Printf("Failed to send subscription request: %v", err)
			}

			if count >= MAX_SUBSCRIPTION_ATTEMPTS {
				xapp.Logger.Error("Max subscription attempts (%d) reached. Stopping timer.", MAX_SUBSCRIPTION_ATTEMPTS)
				log.Printf("Max subscription attempts (%d) reached. Stopping timer.", MAX_SUBSCRIPTION_ATTEMPTS)
				break
			}
			t.Reset(5 * time.Second)
		}
	}(timerSR)
}

// Consume recebe todas as mensagens RMR entregues ao xApp e as enfileira no canal interno.
func (c *Control) Consume(rp *xapp.RMRParams) (err error) {
	c.rcChan <- rp
	return
}

// rmrSend envia uma mensagem via RMR.
func (c *Control) rmrSend(params *xapp.RMRParams) (err error) {
	if !xapp.Rmr.Send(params, false) {
		err = errors.New("rmr.Send() failed")
		xapp.Logger.Error("Failed to rmrSend: %v", err)
		log.Printf("Failed to rmrSend: %v", err)
	}
	return
}

// controlLoop despacha as mensagens RMR recebidas de acordo com o Message Type (Mtype).
func (c *Control) controlLoop() {
	for {
		msg := <-c.rcChan
		xapp.Logger.Debug("Received message type: %d", msg.Mtype)

		switch msg.Mtype {
		case 12050: // RIC_INDICATION
			c.handleIndication(msg)
		case 12011: // RIC_SUB_RESP
			c.handleSubscriptionResponse(msg)
		case 12012: // RIC_SUB_FAILURE
			c.handleSubscriptionFailure(msg)
		case 12021: // RIC_SUB_DEL_RESP
			c.handleSubscriptionDeleteResponse(msg)
		case 12022: // RIC_SUB_DEL_FAILURE
			c.handleSubscriptionDeleteFailure(msg)
		default:
			xapp.Logger.Error("Unknown RMR message type %d discarded", msg.Mtype)
			log.Printf("Unknown RMR message type %d discarded", msg.Mtype)
		}
	}
}

// ============================================================================
// Decodificação e Coleta de Métricas E2SM-KPM (RIC Indication)
// ============================================================================

// handleIndication processa mensagens RIC_INDICATION (Mtype 12050) enviadas pelos nós E2 (ns-3).
func (c *Control) handleIndication(params *xapp.RMRParams) (err error) {
	var e2ap *E2ap
	var e2sm *E2sm

	// 1. Decodifica o envelope E2AP da mensagem RIC Indication
	indicationMsg, err := e2ap.GetIndicationMessage(params.Payload)
	if err != nil {
		xapp.Logger.Error("Failed to decode RIC Indication message: %v", err)
		log.Printf("Failed to decode RIC Indication message: %v", err)
		return
	}

	// 2. Decodifica o Corpo da Indicação (E2SM-KPM Indication Message) e exibe apenas as métricas por UE
	if err = c.logIndicationMessage(e2sm, params.Meid.RanName, indicationMsg.IndMessage); err != nil {
		return err
	}

	return nil
}

// logIndicationMessage decodifica e exibe apenas as métricas por UE (ListOfMatchedUes).
func (c *Control) logIndicationMessage(e2sm *E2sm, ranName string, msgBytes []byte) error {
	indMsg, err := e2sm.GetIndicationMessage(msgBytes)
	if err != nil {
		xapp.Logger.Error("Failed to decode RIC Indication Message: %v", err)
		log.Printf("Failed to decode RIC Indication Message: %v", err)
		return err
	}

	if indMsg.IndMsgType != 1 {
		return nil
	}

	indMsgFormat1 := indMsg.IndMsg.(*IndicationMessageFormat1)

	// Exibe estritamente o relatório de métricas de Throughput/Delay por UE (CU-UP com 6 métricas)
	if indMsgFormat1.ListOfMatchedUesCount > 0 && indMsgFormat1.ListOfMatchedUes[0].ListOfPMInfoCount == 6 {
		c.logMatchedUEs(ranName, indMsgFormat1)
	}

	return nil
}

// logMatchedUEs exibe as métricas por UE (ListOfMatchedUes) reportadas pelo ns-3.
func (c *Control) logMatchedUEs(ranName string, indMsgFormat1 *IndicationMessageFormat1) {
	log.Printf("[Per-UE Metrics (%s)] Total UEs in report: %d", ranName, indMsgFormat1.ListOfMatchedUesCount)

	for i := 0; i < indMsgFormat1.ListOfMatchedUesCount; i++ {
		ueItem := indMsgFormat1.ListOfMatchedUes[i]
		ueID := string(ueItem.UeID.Buf)

		log.Printf("    UE[%d] ID (IMSI): %s | Metrics Count: %d", i, ueID, ueItem.ListOfPMInfoCount)
		for j := 0; j < ueItem.ListOfPMInfoCount; j++ {
			pm := ueItem.ListOfPMInfo[j]
			metricName := string(pm.PMType.Buf)
			log.Printf("      -> %-38s : %v", metricName, pm.PMVal)
		}
	}
}

// ============================================================================
// Gerenciamento de Subscrição E2AP (RIC Subscription)
// ============================================================================

func (c *Control) handleSubscriptionResponse(params *xapp.RMRParams) (err error) {
	ranName := params.Meid.RanName
	log.Printf("Received RIC_SUB_RESP from {%s} (SubId=%d)", ranName, params.SubId)

	c.eventCreateExpiredMu.Lock()
	_, ok := c.eventCreateExpiredMap[ranName]
	if !ok {
		c.eventCreateExpiredMu.Unlock()
		log.Printf("RIC_SUB_REQ timer for {%s} had already expired", ranName)
		return nil
	}
	c.eventCreateExpiredMap[ranName] = true
	c.eventCreateExpiredMu.Unlock()
	c.subRespChan <- ranName

	var e2ap *E2ap
	subscriptionResp, err := e2ap.GetSubscriptionResponseMessage(params.Payload)
	if err != nil {
		xapp.Logger.Error("Failed to decode RIC Subscription Response message: %v", err)
		log.Printf("Failed to decode RIC Subscription Response message: %v", err)
		return
	}

	if subscriptionResp.ActionAdmittedList.Count > 0 {
		c.subscribedRanListMu.Lock()
		c.subscribedRanList[ranName] = true
		c.subscribedRanListMu.Unlock()
	}

	log.Printf("[RIC Subscription Response] Node=%s | SubID=%d | ReqID=%d | ReqSN=%d | FuncID=%d | AdmittedActions=%d",
		ranName,
		params.SubId,
		subscriptionResp.RequestID,
		subscriptionResp.RequestSequenceNumber,
		subscriptionResp.FuncID,
		subscriptionResp.ActionAdmittedList.Count,
	)

	return nil
}

func (c *Control) handleSubscriptionFailure(params *xapp.RMRParams) (err error) {
	ranName := params.Meid.RanName
	log.Printf("Received RIC_SUB_FAILURE from {%s} (SubId=%d)", ranName, params.SubId)

	c.eventCreateExpiredMu.Lock()
	if _, ok := c.eventCreateExpiredMap[ranName]; ok {
		c.eventCreateExpiredMap[ranName] = true
		c.subRespChan <- ranName
	}
	c.eventCreateExpiredMu.Unlock()
	return nil
}

func (c *Control) handleSubscriptionDeleteResponse(params *xapp.RMRParams) (err error) {
	ranName := params.Meid.RanName
	log.Printf("Received RIC_SUB_DEL_RESP from {%s} (SubId=%d)", ranName, params.SubId)

	c.eventDeleteExpiredMu.Lock()
	if _, ok := c.eventDeleteExpiredMap[ranName]; ok {
		c.eventDeleteExpiredMap[ranName] = true
	}
	c.eventDeleteExpiredMu.Unlock()
	return nil
}

func (c *Control) handleSubscriptionDeleteFailure(params *xapp.RMRParams) (err error) {
	ranName := params.Meid.RanName
	log.Printf("Received RIC_SUB_DEL_FAILURE from {%s} (SubId=%d)", ranName, params.SubId)

	c.eventDeleteExpiredMu.Lock()
	if _, ok := c.eventDeleteExpiredMap[ranName]; ok {
		c.eventDeleteExpiredMap[ranName] = true
	}
	c.eventDeleteExpiredMu.Unlock()
	return nil
}

func (c *Control) setEventCreateExpiredTimer(ranName string) {
	c.eventCreateExpiredMu.Lock()
	c.eventCreateExpiredMap[ranName] = false
	c.eventCreateExpiredMu.Unlock()

	timer := time.NewTimer(time.Duration(c.eventCreateExpired) * time.Second)
	go func(t *time.Timer) {
		defer t.Stop()
		for {
			select {
			case <-t.C:
				c.eventCreateExpiredMu.Lock()
				isResponded := c.eventCreateExpiredMap[ranName]
				delete(c.eventCreateExpiredMap, ranName)
				c.eventCreateExpiredMu.Unlock()
				if !isResponded {
					log.Printf("RIC_SUB_REQ[%s]: Event Create Timer expired", ranName)
				}
				return
			default:
				c.eventCreateExpiredMu.Lock()
				if c.eventCreateExpiredMap[ranName] {
					delete(c.eventCreateExpiredMap, ranName)
					c.eventCreateExpiredMu.Unlock()
					return
				}
				c.eventCreateExpiredMu.Unlock()
			}
			time.Sleep(100 * time.Millisecond)
		}
	}(timer)
}

func (c *Control) sendRicSubRequest(subID int, requestSN int, funcID int, ranList []string) (err error) {
	var e2ap *E2ap
	var e2sm *E2sm

	eventTriggerCount := 1
	periods := []int64{13}
	eventTriggerDefinition := make([]byte, 8)
	if _, err = e2sm.SetEventTriggerDefinition(eventTriggerDefinition, eventTriggerCount, periods); err != nil {
		xapp.Logger.Error("Failed to encode EventTriggerDefinition: %v", err)
		log.Printf("Failed to encode EventTriggerDefinition: %v", err)
		return err
	}

	actionCount := 1
	ricStyleType := []int64{0}
	actionIds := []int64{0}
	actionTypes := []int64{0}
	actionDefinitions := make([]ActionDefinition, actionCount)
	subsequentActions := []SubsequentAction{{0, 0, 0}}

	for index := 0; index < actionCount; index++ {
		if ricStyleType[index] == 0 {
			actionDefinitions[index].Buf = nil
			actionDefinitions[index].Size = 0
		} else {
			actionDefinitions[index].Buf = make([]byte, 8)
			if _, err = e2sm.SetActionDefinition(actionDefinitions[index].Buf, ricStyleType[index]); err != nil {
				xapp.Logger.Error("Failed to encode ActionDefinition: %v", err)
				log.Printf("Failed to encode ActionDefinition: %v", err)
				return err
			}
			actionDefinitions[index].Size = len(actionDefinitions[index].Buf)
		}
	}

	for _, ranName := range ranList {
		go func(ranName string) {
			params := &xapp.RMRParams{
				Mtype:   12010, // RIC_SUB_REQ
				SubId:   subID,
				Payload: make([]byte, 1024),
				Meid:    &xapp.RMRMeid{PlmnID: "313131", EnbID: "::", RanName: ranName},
			}

			var payloadErr error
			params.Payload, payloadErr = e2ap.SetSubscriptionRequestPayload(
				params.Payload,
				1001,
				uint16(requestSN),
				uint16(funcID),
				eventTriggerDefinition,
				len(eventTriggerDefinition),
				actionCount,
				actionIds,
				actionTypes,
				actionDefinitions,
				subsequentActions,
			)
			if payloadErr != nil {
				xapp.Logger.Error("Failed to build RIC_SUB_REQ payload for %s: %v", ranName, payloadErr)
				log.Printf("Failed to build RIC_SUB_REQ payload for %s: %v", ranName, payloadErr)
				return
			}

			log.Printf("Sending RIC_SUB_REQ to %s (FuncID=%d, SubId=%d)", ranName, funcID, subID)
			if sendErr := c.rmrSend(params); sendErr != nil {
				xapp.Logger.Error("Failed to send RIC_SUB_REQ to %s: %v", ranName, sendErr)
				log.Printf("Failed to send RIC_SUB_REQ to %s: %v", ranName, sendErr)
				return
			}
			c.setEventCreateExpiredTimer(ranName)
		}(ranName)
	}

	for i := 0; i < len(ranList); i++ {
		select {
		case resp := <-c.subRespChan:
			xapp.Logger.Info("Subscription response confirmed for: %s", resp)
		case <-time.After(time.Duration(c.eventCreateExpired) * time.Second):
			xapp.Logger.Error("Timeout waiting for subscription response")
		}
	}

	return nil
}
