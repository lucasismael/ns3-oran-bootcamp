# xApps do Bootcamp (Near-RT RIC)

Este diretório contém os códigos-fonte, descritores de empacotamento O-RAN e scripts de automação dos **xApps** utilizados ao longo do bootcamp:

* **[`kpimon/`](./kpimon/):** xApp de monitoramento de métricas de rádio (*Key Performance Metrics*) via serviço **E2SM-KPM v1.0**. Subscreve nós do ns-3/E2sim e coleta telemetria periódica (Throughput, Latência, PRBs, SINR) por fatia e por UE.
* **[`rc/`](./rc/):** xApp de controle de rádio (*RAN Control*) via serviço **E2SM-RC v1.0**. Permite enviar ações de controle para nós de rádio gNodeB/eNodeB (ex: ajuste de *Split Ratio* de tráfego entre LTE e mmWave).
* **[`base-image/`](./base-image/):** Construtor da **Imagem Base Docker** (`oran-xapp-base` e `oran-xapp-runtime`), contendo RMR, ASN.1, CGO e bibliotecas pré-compiladas para acelerar o build dos xApps de minutos para segundos.

---

> [!IMPORTANT]
> ### ⚠️ Privilégios de Execução e Superusuário (`sudo -i`)
> 
> As etapas do ciclo de vida dos xApps — que envolvem **compilação de containers Docker**, publicação no **Docker Registry local (`:5000`)**, publicação de Helm Charts no **ChartMuseum (`:8090`)** e chamadas ao utilitário **`dms_cli`** / **`kubectl`** — exigem acesso irrestrito ao socket do Docker e aos binários de orquestração do cluster.
> 
> Além disso, em ambientes onde os diretórios originais dos xApps são sincronizados ou montados a partir do usuário `root` (como via `bindfs` ou volume compartilhado), **é fundamental executar os comandos de build e launch como superusuário**:
> 
> ```bash
> # Acesso ao shell de superusuário
> sudo -i
> 
> # Navegação até o repositório do bootcamp
> cd /home/ns3/ns3-oran-bootcamp/xapps/kpimon
> 
> # Inicializar a infraestrutura local (Registry + ChartMuseum) se ainda não estiver ativa
> ./start_repo.sh
> 
> # Compilar a imagem Docker, fazer onboarding no DMS e instalar no namespace ricxapp
> ./launch_app.sh
> ```

---

## 🛠️ Estrutura de Cada xApp

Cada subdiretório de xApp é estruturado segundo as convenções da **O-RAN Software Community (OSC)**:

```text
xapps/<nome-do-xapp>/
├── cmd/                  # Ponto de entrada da aplicação (main.go)
├── control/              # Lógica de controle, handlers RMR e ASN.1 decoders
├── xapp-descriptor/      # Metadados O-RAN:
│   ├── config.json       # Descritor formal (portas RMR, imagem Docker, parâmetros)
│   └── schema.json       # Esquema JSON de validação
├── Dockerfile            # Multi-stage build para gerar a imagem de runtime
├── launch_app.sh         # Script unificado: uninstall -> build -> push -> onboard -> install
└── routes.txt            # Tabela de roteamento RMR estática para testes e desenvolvimento
```

---

## 🚀 Métodos de Deploy Disponíveis

Você pode testar e rodar os xApps de duas formas distintas:

### Opção A: Método Otimizado via Imagem Base (⚡ Recomendado: build em segundos)

Neste modo, as bibliotecas E2AP, E2SM, RMR e ferramentas CGO já vêm pré-compiladas na imagem base, dispensando a compilação de código C repetitiva.

```bash
sudo -i

# 1. Gerar e publicar a imagem base no registry local (executar apenas uma vez):
cd /home/ns3/ns3-oran-bootcamp/xapps/base-image
./build_base.sh

# 2. Deploy ultra-rápido do KPIMON xApp:
cd /home/ns3/ns3-oran-bootcamp/xapps/kpimon
./launch_app_fast.sh

# 3. Deploy ultra-rápido do RC xApp:
cd /home/ns3/ns3-oran-bootcamp/xapps/rc
./launch_app_fast.sh
./split_command.sh 50
```

---

### Opção B: Método Tradicional (Standalone: compila do zero)

Neste modo (original), cada xApp constrói sua imagem independentemente a partir do sistema base, clonando bibliotecas e compilando os arquivos C com `gcc`.

```bash
sudo -i

# 1. Inicializar Registry e ChartMuseum (apenas uma vez):
cd /home/ns3/ns3-oran-bootcamp/xapps/kpimon
./start_repo.sh

# 2. Deploy do KPIMON xApp:
./launch_app.sh

# 3. Deploy do RC xApp:
cd /home/ns3/ns3-oran-bootcamp/xapps/rc
./launch_app.sh
./split_command.sh 50
```
