# Laboratório 1: Deploy do Near-RT RIC

Este guia detalha o processo passo-a-passo para instalar o cluster Kubernetes, preparar as ferramentas de orquestração (Helm e Docker) e realizar o deploy completo do Near-RT RIC da O-RAN SC.

É importante realizar todo esse procedimento como usuário sudo
   ```bash   
   sudo -i
   ```

## 1. Clonando o Repositório

O primeiro passo consiste em obter o código-fonte da plataforma.

Nele já estão contidos os scripts, Helm charts, e outros arquivos utilizados para realizar o deploy do Near-RT RIC.

Clone o repositório **ric-plt/ric-dep** do OSC RIC:
   ```bash
   git clone "https://gerrit.o-ran-sc.org/r/ric-plt/ric-dep"
   ```
   
Entre no seguinte diretório (Ajuste o caminho do diretório caso necessário):
   ```bash
   cd ~/ric-dep/bin 
   ```

## 2. Provisionamento do cluster Kubernetes

Utilize o script ```install_k8s_and_helm.sh```

* Desenvolvido para preparar um sistema operacional Ubuntu (16.04, 18.04 ou 20.04) e provisionar um cluster Kubernetes *single-node*, configurando o ambiente base necessário para rodar a plataforma Near-RT RIC da O-RAN.
```bash
./install_k8s_and_helm.sh
```

* Ao fim da execução do script é possível checar o status do cluster:

```bash
kubectl get pods -n kube-system
```

<div align="center">
  <img src="./figures/lab01/kube-system.png" alt="Output do comando para checar o status dos pods do cluster kubernetes">
</div>
<!-- ![Output do comando para checar o status dos pods do cluster kubernetes](./figures/lab01/kube-system.png) -->

## 3. Deploy do Near-RT RIC

Agora, partimos para o deploy dos componentes do RIC. Para isso é utilizado o script ```install```.
   * Executa comandos do Helm em lote para implantar todos os microserviços da plataforma (e.g. E2Term, E2Mgr, AppMgr) no cluster Kubernetes. Em vez de o usuário instalar manualmente cada componente, o script lê os *charts* do repositório, aplica a ordem correta de dependências e sobe a infraestrutura completa utilizando as configurações fornecidas por um arquivo ```RECIPE```.
   * A ```RECIPE``` é um arquivo YAML que funciona como o mapa de variáveis para o script de instalação. Ele centraliza todas as configurações personalizáveis do deploy, definindo parâmetros cruciais como os endereços IP de comunicação da plataforma, mapeamento de portas, e as versões exatas das imagens Docker que devem ser baixadas para cada componente do RIC.


### 3.1 Editando a RECIPE

A O-RAN Software Community já disponibiliza exemplos de ```RECIPE``` para as diferentes releases do Near-RT RIC. Todas elas podem ser encontradas no diretório ```ric-dep/RECIPE_EXAMPLE```. 

É crucial definir os endereços de IP da plataforma. Aqui vamos utilizar o ```example_recipe_oran_e_release.yaml```.

Consulte o endereço de IP da sua máquina com o comando:
```bash
hostname -I | awk '{print $1}'
```

Edite o arquivo ```~/ric-dep/RECIPE_EXAMPLE/example_recipe_oran_e_release.yaml``` alterando os campos ```ricip``` e ```auxip``` (Por padrão vem com o valor 10.0.0.1) para o IP encontrado no passo anterior.


<div align="center">
  <img src="./figures/lab01/recipe_ips_old.png" alt="Antes da alteração" width="35%" align="middle">
  &nbsp; &#10140; &nbsp;
  <img src="./figures/lab01/recipe_ips_new.png" alt="Depois da alteração" width="40%" align="middle">
</div>

### 3.2 Preparação de Dependências do Helm
No ecossistema O-RAN, os microserviços individuais não são completamente independentes nas suas configurações; eles herdam parâmetros padronizados, macros de formatação e definições de rede.

Antes do deploy é necessário executar o script ```install_common_templates_to_helm.sh```

Ele garante que, quando o orquestrador começar a levantar a infraestrutura, o Helm tenha acesso a essa biblioteca de código compartilhado. Se este passo for ignorado, o script principal falhará imediatamente, pois os charts de cada componente não conseguirão resolver as suas dependências cruzadas nem gerar os *manifests* do Kubernetes corretamente.

<div align="center">
  <img src="./figures/lab01/install_helm_templates.png" alt="Output do comando install helm">
</div>

### 3.3 Deploy do Near-RT RIC

Agora, o script de instalação pode ser executado:

```bash
./install -f ../RECIPE_EXAMPLE/example_recipe_oran_e_release.yaml
```

Este script constrói a infraestrutura do Near-RT RIC no Kubernetes. Em vez de lançar todos os componentes de forma simultânea, ele segue uma sequência de inicialização orquestrada para garantir que as dependências arquiteturais da plataforma sejam respeitadas.

Ao fim da execução, todos os status dos *pods* podem ser observados com o comando:

```bash
kubectl get pods -n ricplt
```
*Você pode adicionar um ```watch``` no começo do comando para acompanhar os status sendo atualizados.*

Os *pods* dos componentes podem demorar alguns minutos para ficarem prontos. Ao fim do processo o namespace ```ric-plt``` deve ser:

<div align="center">
  <img src="./figures/lab01/pods_status_final.png" alt="namespace ric-plt">
</div>

### 3.4 Undeploy do Near-RT RIC

Caso necessário, é possível desinstalar toda a plataforma com:

```bash
./uninstall #presente no diretório ~/ric-dep/bin
```
### 3.5 Troubleshooting

#### A1 Mediator "Running" mas não está "Ready"
Por padrão, a Release E do OSC RIC possui um bug de configuração no componente **A1 Mediator**. Se não aplicarmos este patch, o pod iniciará (status `Running`), mas o Kubernetes nunca o considerará pronto (`Ready 0/1`).

Isto ocorre devido a dois fatores principais que o patch corrige silenciosamente:
1. **Healthchecks Desatualizados:** As sondas do Kubernetes (*Liveness* e *Readiness probes*) estavam a tentar verificar a saúde do componente através do URL legada `/A1-P/v2/healthcheck`. O patch atualiza o caminho para `/a1-p/healthcheck`.
2. **Erro de Renderização do Helm:** O ficheiro `config.yaml` possuía um erro de escopo de variável (procurava `.Values.loglevel` em vez de `.Values.a1mediator.loglevel`).

**SOLUÇÃO:**
1. Copie o arquivo de *patch* ```fix_a1mediator.patch```
```diff
diff -ruN ./templates/config.yaml /root/ric-plt-ric-dep/helm/a1mediator/templates/config.yaml
--- ./templates/config.yaml	2024-07-06 15:04:40.941990000 -0300
+++ /root/ric-plt-ric-dep/helm/a1mediator/templates/config.yaml	2024-07-06 16:30:01.342419312 -0300
@@ -33,4 +33,4 @@
     rte|20012|{{ include "common.servicename.a1mediator.rmr" . }}.{{ include "common.namespace.platform" . }}:{{ include "common.serviceport.a1mediator.rmr.data" . }}
     newrt|end
   loglevel.txt: |
-    log-level: {{ .Values.loglevel }}
+    log-level: {{ .Values.a1mediator.loglevel }}
diff -ruN ./templates/deployment.yaml /root/ric-plt-ric-dep/helm/a1mediator/templates/deployment.yaml
--- ./templates/deployment.yaml	2024-07-06 15:04:40.941990000 -0300
+++ /root/ric-plt-ric-dep/helm/a1mediator/templates/deployment.yaml	2024-07-06 16:30:01.342419312 -0300
@@ -71,11 +71,11 @@
               protocol: TCP
           livenessProbe:
             httpGet:
-              path: /A1-P/v2/healthcheck
+              path: /a1-p/healthcheck
               port: http
           readinessProbe:
             httpGet:
-              path: /A1-P/v2/healthcheck
+              path: /a1-p/healthcheck
               port: http
       volumes:
         - name: "a1conf"
diff -ruN ./templates/ingress-a1mediator.yaml /root/ric-plt-ric-dep/helm/a1mediator/templates/ingress-a1mediator.yaml
--- ./templates/ingress-a1mediator.yaml	2024-07-06 15:04:40.941990000 -0300
+++ /root/ric-plt-ric-dep/helm/a1mediator/templates/ingress-a1mediator.yaml	2024-07-06 16:30:01.342419312 -0300
@@ -6,8 +6,8 @@
   rules:
   - http:
       paths:
-      - path: {{ include "common.kongpath.ric.a1mediator" . }}
-        pathType: Prefix
+      - pathType: Prefix
+        path: {{ include "common.kongpath.ric.a1mediator" . }}
         backend:
           service:
             name: {{ include "common.servicename.a1mediator.http" . }}
diff -ruN ./values.yaml /root/ric-plt-ric-dep/helm/a1mediator/values.yaml
--- ./values.yaml	2024-07-06 15:04:40.941990000 -0300
+++ /root/ric-plt-ric-dep/helm/a1mediator/values.yaml	2024-07-06 16:30:01.342419312 -0300
@@ -38,4 +38,4 @@
     a1_rcv_retry_times: 20
     ins_del_no_resp_ttl: 5
     ins_del_resp_ttl: 10
-  loglevel: "INFO"
+  loglevel: "DEBUG"
```
2. Salve em `~/ric-dep/helm/a1mediator` 
3. Aplique o *patch* com: `patch -p1 < fix_a1mediator.patch`
4. Reinstale o RIC
#