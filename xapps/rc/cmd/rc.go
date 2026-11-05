package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	"gerrit.o-ran-sc.org/r/ric-app/rc/control"
)

func main() {
	var autoMode bool = false
	var initialSplit int64 = -1

	// Parsing de argumentos:
	//   ./rc
	//   ./rc 25
	//   ./rc --auto
	for _, arg := range os.Args[1:] {
		if arg == "-auto" || arg == "--auto" {
			autoMode = true
		} else if val, err := strconv.ParseInt(arg, 10, 64); err == nil && val >= 0 && val <= 100 {
			initialSplit = val
		}
	}

	if autoMode {
		os.Setenv("RC_AUTO_MODE", "true")
	}

	// 1. Registro do xApp junto ao AppMgr do RIC (igual ao kpimon)
	deRegisterXApp()
	time.Sleep(2 * time.Second)
	registerXApp()
	time.Sleep(2 * time.Second)

	// Limpa os.Args para xapp.Run() não reclamar de flags
	os.Args = []string{os.Args[0]}

	c := control.NewControl()

	// Se passou um split inicial na linha de comando, dispara assim que estiver pronto
	if initialSplit >= 0 {
		go func() {
			time.Sleep(5 * time.Second) // Aguarda RMR estar pronto
			c.SendDrbSplitControl("00001", 1, initialSplit)
		}()
	}

	c.Run()
}

func registerXApp() ([]byte, error) {
	url := "http://service-ricplt-appmgr-http.ricplt:8080/ric/v1/register"
	payload, err := ioutil.ReadFile("/opt/ric/config/config-file.json")
	if err != nil {
		fmt.Printf("[RC-Reg] Error read config: %v\n", err)
	}

	hostname := os.Getenv("HOSTNAME")
	xappName := hostname
	xappVersion := "1.0.0"
	xappNamespace := "ricxapp"

	rmrOsKey := "SERVICE_" + strings.ToUpper(xappNamespace) + "_" + strings.ToUpper(xappName) + "_RMR_PORT"
	rmrEndpointEnv := os.Getenv(rmrOsKey)
	rmrEndpoint := ""
	if strings.Contains(rmrEndpointEnv, "//") {
		rmrEndpoint = strings.Split(rmrEndpointEnv, "//")[1]
	}

	request := map[string]interface{}{
		"appName":         hostname,
		"appVersion":      xappVersion,
		"configPath":      "",
		"appInstanceName": xappName,
		"httpEndpoint":    "",
		"rmrEndpoint":     rmrEndpoint,
		"config":          string(payload),
	}

	requestString, _ := json.Marshal(request)
	req, err := http.NewRequest("POST", url, bytes.NewBuffer(requestString))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/json")

	client := &http.Client{Timeout: 5 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, _ := ioutil.ReadAll(resp.Body)
	fmt.Printf("[RC-Reg] Registered to AppMgr: %s\n", string(body))
	return body, nil
}

func deRegisterXApp() ([]byte, error) {
	url := "http://service-ricplt-appmgr-http.ricplt:8080/ric/v1/deregister"
	hostname := os.Getenv("HOSTNAME")

	request := map[string]interface{}{
		"appName":         hostname,
		"appInstanceName": hostname,
	}
	requestString, _ := json.Marshal(request)
	req, err := http.NewRequest("POST", url, bytes.NewBuffer(requestString))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/json")

	client := &http.Client{Timeout: 5 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, _ := ioutil.ReadAll(resp.Body)
	return body, nil
}
