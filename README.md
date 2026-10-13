# Bootcamp: Integração ns-3 e O-RAN (OSC RIC Release E)

Bem-vindo ao repositório do Bootcamp de integração entre o simulador de redes **ns-3** e o **Near-RT RIC** (O-RAN Software Community - Release E) 

Este repositório fornece o ambiente, os roteiros e as configurações necessárias para provisionar o **Near-RT RIC**, conectar uma simulação do **ns-3** via interface E2 e executar um xApp de monitoramento.

---

## 📋 1. Pré-requisitos e Ambiente

Para reproduzir este laboratório, você precisará de um *host* (*Bare Metal* ou *Virtual Machine*) com as seguintes especificações mínimas:
* **Sistema Operacional:** Ubuntu 20.04
* **Memória RAM:** Mínimo de 4GB
* **Processador:** Dual-core (Recomendado: 4 Cores)
* **Armazenamento:** Pelo menos 50 GB de espaço em disco

### Arquitetura do Ambiente

Abaixo está o diagrama ilustrando a arquitetura abordada neste laboratório de forma simplificada:

```mermaid
%%{ init: { 'flowchart': { 'curve': 'linear' } } }%%
flowchart LR
    subgraph NS3["Ambiente ns-3"]
        LTE["LTE"] --- ORANI["oran-interface"]
        MMWAVE["mmWave"] --- ORANI
    end
    
    E2SIM["E2sim"]
    
    subgraph RIC["Near-RT RIC"]
        E2T["E2 Termination"]
        
        XAPP1["xApp 1"]
        XAPP2["xApp 2"]
        XAPP3["xApp 3"]
        
        E2T --- XAPP1
        E2T --- XAPP2
        E2T --- XAPP3
    end

    ORANI ---|E2| E2SIM
    E2SIM ---|E2| E2T
```

### Dependências de Software
Antes de iniciar, certifique-se de que o sistema está atualizado e instale as ferramentas essenciais:
```bash
sudo apt update -y && sudo apt upgrade -y
sudo apt install -y python3-pip git curl cmake tree vim net-tools build-essential nfs-common openssh-server libsctp-dev lksctp-tools autoconf automake libtool bison flex libboost-all-dev apt-utils
```
*Nota: Lembre-se de instalar os Virtual Box Guest Addons se estiver utilizando o VirtualBox*

---

## 🚀 2. Roteiro do Bootcamp

O bootcamp está dividido em etapas práticas. Acesse os roteiros de cada laboratório para consultar as instruções detalhadas, os comandos passo a passo e os códigos-fonte necessários:

| Laboratório | Descrição | Status |
| :--- | :--- | :---: |
| 🌐 [**Lab 1: Fundamentos da Arquitetura O-RAN e Deploy do Near-RT RIC**](./labs/lab01-setup-RIC.md) | Fundamentos da Arquitetura O-RAN, orquestração Cloud-Native (Kubernetes e Helm), configuração da RECIPE e deploy dos microsserviços do RIC | ✅ Disponível |
| ⚙️ [**Lab 2: Fundamentos do ns-3, Arquitetura ns-O-RAN e Preparação do Ambiente**](./labs/lab02-setup-ns3.md) | Fundamentos do ns-3 (DES e tempo real), pilha 5G mmWave/DC, arquitetura ns-O-RAN (E2sim, E2SM-KPM/RC) e compilação com Waf | ✅ Disponível |
| 🔄 [**Lab 3: Validação da Integração ns-3 e Near-RT RIC**](./labs/lab03-validacao-e2setup.md) | Protocolo E2AP, transporte SCTP, barramento RMR, execução do cenário DC e validação via REST API | ✅ Disponível |
| 📊 **Lab 4: Onboarding do KPIMon xApp e Simulação** | Implantação de xApp no RIC e coleta de telemetria da simulação em tempo real | ⏳ *Em breve* |

---

### [Lab 1: Fundamentos da Arquitetura O-RAN e Deploy do Near-RT RIC](./labs/lab01-setup-RIC.md)
Nesta etapa, exploramos a fundamentação teórica da Arquitetura O-RAN (desagregação da RAN, loops de controle temporal, SMO, RICs e interfaces abertas) e os princípios de infraestrutura Cloud-Native com Kubernetes e Helm. Em seguida, realizamos o provisionamento prático de um cluster Kubernetes *single-node*, a customização da receita de deploy (*RECIPE*) e a implantação completa dos microsserviços do Near-RT RIC da O-RAN SC.

### [Lab 2: Fundamentos do ns-3, Arquitetura ns-O-RAN e Preparação do Ambiente](./labs/lab02-setup-ns3.md)
Neste laboratório, aprofundamos a teoria do simulador ns-3 (versão 3.33), o funcionamento de simulações de eventos discretos (DES) versus tempo real com `RealtimeSimulatorImpl`, e a pilha de protocolos de ondas milimétricas do módulo `ns3-mmwave` com Conectividade Dupla (LTE + mmWave). Exploramos a arquitetura do arcabouço `ns-O-RAN`, detalhando a comunicação multi-threaded via `E2sim`, os modelos de serviço E2SM-KPM e E2SM-RC, e o processo prático de compilação da biblioteca ASN.1 e do simulador com o sistema Waf.

<!-- *(Conteúdo Extra)* O laboratório também discute a alternativa de deploy conteinerizado do ns-3 via Docker. -->

### [Lab 3: Validação da Integração ns-3 e Near-RT RIC (O Caminho do E2 Setup)](./labs/lab03-validacao-e2setup.md)
Neste laboratório, o foco é validar e dissecar a comunicação fim-a-fim entre o ns-3 e o Near-RT RIC através do procedimento de E2 Setup. O roteiro aprofunda a especificação do protocolo E2AP (Procedimentos Elementares e codificação ASN.1 APER), a camada de transporte SCTP (multi-streaming e RFC 4960), a mensageria interna RMR e o armazenamento na Shared Data Layer (SDL/Redis). Guiamos o aluno pela parametrização do cenário de Conectividade Dupla (`scenario-trainning.cc`), observabilidade em tempo real nos pods do `e2term` e homologação definitiva dos nós com status CONNECTED via API REST do E2 Manager.

### Lab 4: Onboarding do KPIMon xApp e Execução da Simulação *(Em breve)*
O último laboratório realiza o fechamento do loop da arquitetura (Simulador ↔ RIC ↔ xApp). O roteiro orientará sobre como obter o código de um xApp de monitoramento de métricas, configurar um registro local de imagens Docker, realizar o processo de onboarding e lançamento no cluster Kubernetes e visualizar as métricas sendo coletadas em tempo real.

---

<!-- ## 🛠 Considerações Finais
Em caso de falha em algum pod (ex: status `CrashLoopBackOff`), verifique os logs do Kubernetes utilizando `kubectl logs <nome-do-pod> -n ricplt`. 
Certifique-se de que a variável de política de imagem `image_pull_policy` no Helm está setada como `Always` durante o desenvolvimento de novos xApps para garantir que suas alterações sejam carregadas[cite: 4]. -->