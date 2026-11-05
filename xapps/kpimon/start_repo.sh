#!/bin/bash
# Redireciona para o inicializador unificado de infraestrutura dos xApps
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/../start_repo.sh" "$@"
