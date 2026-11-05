package control

import (
	"sync"

	"gerrit.o-ran-sc.org/r/ric-plt/xapp-frame/pkg/xapp"
)

type Control struct {
	eventRicControlReqTimePeriod int32
	rcChan                       chan *xapp.RMRParams
	eventRicControlReqExpiredMap map[int]bool
	eventRicControlReqExpiredMu  *sync.Mutex
	ricRequestInstanceID         int

	// Configuração do alvo de controle E2
	ranName string
	plmnID  string
	enbID   string
	drbID   int64

	// Modo Automático (envia split periodicamente)
	autoMode     bool
	autoInterval int // segundos
	autoSplits   []int64
	autoIndex    int
}
