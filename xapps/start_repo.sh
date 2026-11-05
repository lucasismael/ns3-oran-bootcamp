#!/bin/bash
# ===================================================================================
# start_repo.sh: Inicializa e/ou reativa o Docker Registry (:5000) e ChartMuseum (:8090)
# ===================================================================================

set -e

# Garante a execução a partir do diretório raiz de xapps
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CHARTS_DIR="$SCRIPT_DIR/charts"
mkdir -p "$CHARTS_DIR"

start_or_resume_registry() {
    # 1. Verifica se a porta 5000 já está respondendo
    if curl -s -f http://127.0.0.1:5000/v2/ >/dev/null 2>&1; then
        echo "✅ Docker Registry já está ativo e respondendo na porta 5000."
        return 0
    fi

    # 2. Verifica se o container 'registry' existe no Docker (mesmo parado pós-reboot)
    if docker ps -a --format '{{.Names}}' | grep -Eq "^registry$"; then
        echo "🔄 Reativando container Registry existente..."
        docker start registry >/dev/null 2>&1 || {
            echo "Aviso: Falha ao iniciar registry existente. Recriando container..."
            docker rm -f registry 2>/dev/null || true
            docker run -d -p 5000:5000 --name registry --restart=unless-stopped registry:2
        }
    else
        echo "🚀 Criando e iniciando novo container Registry com auto-restart..."
        docker run -d -p 5000:5000 --name registry --restart=unless-stopped registry:2
    fi
}

start_or_resume_chartmuseum() {
    # 1. Verifica se o ChartMuseum já está respondendo na porta 8090
    if curl -s -f http://127.0.0.1:8090/health >/dev/null 2>&1; then
        echo "✅ ChartMuseum já está ativo e respondendo na porta 8090."
        return 0
    fi

    # 2. Verifica se o container 'chartmuseum' existe no Docker (mesmo parado pós-reboot)
    if docker ps -a --format '{{.Names}}' | grep -Eq "^chartmuseum$"; then
        echo "🔄 Reativando container ChartMuseum existente..."
        docker start chartmuseum >/dev/null 2>&1 || {
            echo "Aviso: Falha ao iniciar chartmuseum existente. Recriando container..."
            docker rm -f chartmuseum 2>/dev/null || true
            docker run -d -u 0 -p 8090:8080 --name chartmuseum --restart=unless-stopped \
                -e DEBUG=1 -e STORAGE=local -e STORAGE_LOCAL_ROOTDIR=/charts \
                -v "$CHARTS_DIR":/charts \
                chartmuseum/chartmuseum:latest
        }
    else
        echo "🚀 Criando e iniciando novo container ChartMuseum na porta 8090 com auto-restart..."
        docker run -d -u 0 -p 8090:8080 --name chartmuseum --restart=unless-stopped \
            -e DEBUG=1 -e STORAGE=local -e STORAGE_LOCAL_ROOTDIR=/charts \
            -v "$CHARTS_DIR":/charts \
            chartmuseum/chartmuseum:latest
    fi
}

echo "=========================================================="
echo "Verificando Infraestrutura Local (Registry & ChartMuseum)..."
echo "=========================================================="

start_or_resume_registry
start_or_resume_chartmuseum

export CHART_REPO_URL=http://0.0.0.0:8090

# Aguarda 1 segundo para estabilização das portas
sleep 1

REGISTRY_OK=false
CHARTMUSEUM_OK=false

if curl -s -f http://127.0.0.1:5000/v2/ >/dev/null 2>&1; then
    REGISTRY_OK=true
fi

if curl -s -f http://127.0.0.1:8090/health >/dev/null 2>&1; then
    CHARTMUSEUM_OK=true
fi

if [ "$REGISTRY_OK" = true ] && [ "$CHARTMUSEUM_OK" = true ]; then
    echo "=========================================================="
    echo "✅ Toda a infraestrutura local está pronta e operante!"
    echo "   Docker Registry : http://127.0.0.1:5000  (Status: OK)"
    echo "   ChartMuseum     : http://127.0.0.1:8090  (Status: OK)"
    echo "   Política        : --restart=unless-stopped (ativa no reboot)"
    echo "=========================================================="
else
    echo "=========================================================="
    echo "❌ Erro: Falha ao validar a saúde dos serviços locais!"
    echo "   Registry (porta 5000)   : $REGISTRY_OK"
    echo "   ChartMuseum (porta 8090): $CHARTMUSEUM_OK"
    echo "=========================================================="
    echo "Dica: Verifique se as portas 5000 e 8090 não estão em uso"
    echo "      ou audite os logs com: docker logs registry / docker logs chartmuseum"
    exit 1
fi
