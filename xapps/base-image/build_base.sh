#!/bin/bash
# ===================================================================================
# build_base.sh: Compila e publica as Imagens Base dos xApps no Registry Local
# ===================================================================================

set -e

# Garante a execução a partir do diretório xapps/ (contexto de build)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XAPPS_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

REGISTRY="127.0.0.1:5000"
BUILDER_TAG="${REGISTRY}/oran-xapp-base:1.0.0"
RUNTIME_TAG="${REGISTRY}/oran-xapp-runtime:1.0.0"

echo "=========================================================="
echo "Iniciando compilação das Imagens Base O-RAN para xApps..."
echo "Contexto de Build: $XAPPS_ROOT"
echo "Builder Target   : $BUILDER_TAG"
echo "Runtime Target   : $RUNTIME_TAG"
echo "=========================================================="

cd "$XAPPS_ROOT"

# Pré-vôo: Garante que Registry (:5000) e ChartMuseum (:8090) estão ativos
"$XAPPS_ROOT/start_repo.sh"

# 1. Compila o Builder Base (Stage 1)
echo ">>> [1/4] Compilando Stage 1 (Builder Base)..."
docker build \
    --network=host \
    -f base-image/Dockerfile.base \
    --target xapp-base-builder \
    -t "$BUILDER_TAG" .

# 2. Publica o Builder Base no registro local
echo ">>> [2/4] Enviando $BUILDER_TAG para o Registry..."
docker push "$BUILDER_TAG"

# 3. Compila o Runtime Base (Stage 2)
echo ">>> [3/4] Compilando Stage 2 (Runtime Base)..."
docker build \
    --network=host \
    -f base-image/Dockerfile.base \
    --target xapp-runtime-base \
    -t "$RUNTIME_TAG" .

# 4. Publica o Runtime Base no registro local
echo ">>> [4/4] Enviando $RUNTIME_TAG para o Registry..."
docker push "$RUNTIME_TAG"

echo ""
echo "=========================================================="
echo "✅ Imagens Base compiladas e publicadas com sucesso!"
echo "   Builder : $BUILDER_TAG"
echo "   Runtime : $RUNTIME_TAG"
echo ""
echo "Agora os xApps podem utilizar 'Dockerfile.fast' e compilar"
echo "em segundos, sem recompilar E2AP, E2SM ou instalar pacotes!"
echo "=========================================================="
