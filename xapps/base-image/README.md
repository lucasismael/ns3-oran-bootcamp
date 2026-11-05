# Imagem Base O-RAN para xApps (`oran-xapp-base`)

Este diretório contém o gerador da **Imagem Base Docker** para os xApps do Bootcamp.

---

## 🎯 Por que usar a Imagem Base?

No modo tradicional (`launch_app.sh`), cada ciclo de build do xApp gasta entre **2 a 5 minutos** repetindo tarefas estáticas:
1. `apt update` e download de pacotes de desenvolvimento (`cmake`, `build-essential`).
2. Download e instalação dos pacotes `.deb` do RMR 4.7.4.
3. Download e compilação da biblioteca `libe2proto` (no KPIMON).
4. Compilação com `gcc` de mais de **600 arquivos C** das pastas `e2ap/` e `e2sm/` (no RC).
5. Download e resolução de dependências do framework `xapp-frame`.

Ao compilar a **Imagem Base** uma única vez, todas essas bibliotecas e arquivos gerados ficam pré-instalados em `/usr/local/lib` e `/usr/local/include`. 

Com isso, o build do xApp (`launch_app_fast.sh`) precisa apenas compilar o código Go da aplicação, reduzindo o tempo de build para **menos de 5 segundos**!

---

## 🛠️ Como Gerar a Imagem Base (Apenas uma vez)

Como essa etapa gerencia imagens no daemon Docker e publica no registro local `:5000`, execute como superusuário:

```bash
sudo -i
cd /home/ns3/ns3-oran-bootcamp/xapps/base-image
./build_base.sh
```

O script criará e publicará no Docker Registry local duas imagens:
* **`127.0.0.1:5000/oran-xapp-base:1.0.0`**: Ambiente completo de compilação (Go 1.14 + CGO + RMR 4.7.4 + ASN.1 + libe2proto + Wrappers E2AP/E2SM).
* **`127.0.0.1:5000/oran-xapp-runtime:1.0.0`**: Imagem enxuta de execução com as bibliotecas `.so` prontas.

---

## 🧪 Comparando os Dois Métodos de Deploy

Você pode testar e comparar o funcionamento de ambos os métodos lado a lado:

### 1. Método Tradicional (Standalone - Recompila tudo do zero):
```bash
sudo -i
cd /home/ns3/ns3-oran-bootcamp/xapps/kpimon
./launch_app.sh   # Utiliza o Dockerfile padrão
```

### 2. Método Otimizado (Via Imagem Base - Build em segundos):
```bash
sudo -i
# (Certifique-se de ter gerado a base com ./build_base.sh previamente)
cd /home/ns3/ns3-oran-bootcamp/xapps/kpimon
./launch_app_fast.sh   # Utiliza Dockerfile.fast
```
