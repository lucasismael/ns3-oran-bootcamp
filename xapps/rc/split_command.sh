#!/bin/bash

# Garante a execução a partir do diretório do script
cd "$(dirname "$0")"

# Envia comando de Split Ratio para a instância em execução do xApp RC via pipe local

SPLIT_RATIO="50"
DRB_ID="1"
UE_ID="00001"

usage() {
    echo "Uso: $0 [splitRatio] [drbId] [ueId]"
    echo ""
    echo "Exemplos:"
    echo "  $0 25             (Aplica 25% LTE / 75% NR)"
    echo "  $0 50             (Aplica 50% LTE / 50% NR)"
    echo "  $0 100            (Aplica 100% LTE / 0% NR)"
    echo "  $0 75 1 00001     (Formato completo com split, drb e ue)"
    exit 1
}

if [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
    usage
fi

if [ -n "$1" ]; then
    SPLIT_RATIO="$1"
fi
if [ -n "$2" ]; then
    DRB_ID="$2"
fi
if [ -n "$3" ]; then
    UE_ID="$3"
fi

POD_NAME=$(kubectl get pods -n ricxapp --no-headers -o custom-columns=":metadata.name" 2>/dev/null | grep -E "^ricxapp-rc-" | head -n 1)

if [ -z "$POD_NAME" ]; then
    echo "Erro: Pod do xApp RC não encontrado no namespace ricxapp."
    echo "Certifique-se de que o xApp foi iniciado com ./launch_app.sh"
    exit 1
fi

echo "=========================================================="
echo "Enviando comando para o xApp RC ($POD_NAME):"
echo "  UE ID       : $UE_ID"
echo "  DRB ID      : $DRB_ID"
echo "  Split Ratio : $SPLIT_RATIO% LTE / $((100 - SPLIT_RATIO))% NR"
echo "=========================================================="

# Envia o comando para o FIFO do xApp em execução no pod
kubectl exec -i -n ricxapp "$POD_NAME" -- /bin/bash -c "echo '$UE_ID $DRB_ID $SPLIT_RATIO' > /tmp/rc_cmd"

echo "Comando entregue ao xApp RC com sucesso!"
sleep 1
echo ""
echo "Últimos logs do xApp RC:"
kubectl logs -n ricxapp "$POD_NAME" --tail=8
echo ""
echo "Para acompanhar o stream contínuo de logs:"
echo "  kubectl logs -f -n ricxapp $POD_NAME"
