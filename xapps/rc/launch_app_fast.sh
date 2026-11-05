#!/bin/bash
#set -x 

# Garante a execução a partir do diretório do script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Pré-vôo: Garante que Registry (:5000) e ChartMuseum (:8090) estão ativos
"$SCRIPT_DIR/../start_repo.sh"

export CHART_REPO_URL=http://0.0.0.0:8090

echo ">>> Desinstalando rc prévio (se houver)..."
dms_cli uninstall rc ricxapp || true

echo ">>> Executando build otimizado com Dockerfile.fast (usando imagem base)..."
docker build --network=host . -f Dockerfile.fast -t 127.0.0.1:5000/rc_dev:1.0.0

echo ">>> Enviando imagem para o registry local..."
docker push 127.0.0.1:5000/rc_dev:1.0.0

echo ">>> Realizando onboarding no DMS..."
dms_cli onboard ./xapp-descriptor/config_dev.json ./xapp-descriptor/schema.json

echo ">>> Instalando rc via Helm no namespace ricxapp..."
dms_cli install rc 1.0.0 ricxapp

echo "Wait for 10 seconds"
sleep 10

# Filtra especificamente o pod do xApp rc
pod_name=$(kubectl get pods -n ricxapp --no-headers -o custom-columns=":metadata.name" | grep -E "^ricxapp-rc-" | head -n 1)

echo "=========================================================="
echo "xApp RC iniciado com sucesso no pod: $pod_name"
echo ""
echo "O serviço RC já está em execução de fundo no pod."
echo ""
echo "Para acompanhar os logs em tempo real:"
echo "  kubectl logs -f -n ricxapp $pod_name"
echo ""
echo "Para aplicar um Split Ratio (ex: 50% LTE / 50% NR):"
echo "  ./split_command.sh 50"
echo "=========================================================="
