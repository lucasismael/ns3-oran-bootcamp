# Laboratório 2: Instalação do ns-3 e ns-O-RAN

Este guia detalha o processo de preparação do ambiente de simulação de redes. Aqui, vamos compilar o simulador ns-3, a biblioteca E2sim e o módulo ns-O-RAN localmente, preparado para se conectar ao Near-RT RIC.

## 1. Instalação de Dependências

O ns-3 o E2Sim exigem compiladores C++, bibliotecas de comunicação (como SCTP para a interface E2) e ferramentas de automação (CMake).

* Atualize a lista de pacotes e instale as dependências necessárias:

    ```bash
    sudo apt-get update
    sudo apt-get install -y build-essential git cmake libsctp-dev autoconf automake libtool bison flex libboost-all-dev
    sudo apt-get install -y g++ python3 qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools
    ```

## 2. Compilação e Instalação da Biblioteca E2Sim

A biblioteca E2Sim é responsável por simular o comportamento de um nó E2 e lidar com a codificação e decodificação das mensagens E2AP e E2SM.

Nessa etapa são necessárias permissões de sudo
```bash
sudo -i
```

* Clone o repositório da biblioteca:
    ```bash
    git clone -b develop https://github.com/wineslab/ns-o-ran-e2-sim ~/e2sim
    ```

* Crie o diretório de *build* dentro do projeto e navegue até ele:
    ```bash
    mkdir -b ~/e2sim/e2sim/build
    cd ~/e2sim/e2sim/build
    ```

* Configure o projeto com *cmake*. Aqui é possível definir o nível de log desejado com ```-DLOG_LEVEL```:
  * ```LOG_LEVEL_UNCOND```  =  0
  * ```LOG_LEVEL_ERROR```   =  1
  * ```LOG_LEVEL_INFO```    =  2
  * ```LOG_LEVEL_DEBUG```   =  3
  ```bash
  cmake .. -DDEV_PKG=1 -DLOG_LEVEL=3
  ```

* Empacote a compilação utilizando o ```make```:
    ```bash
    make package
    ```  

* Instale o pacote .deb gerado no sistema operacional e atualize o cache de bibliotecas dinâmicas:
    ```bash
    dpkg --install ./e2sim-dev_1.0.0_amd64.deb
    ldconfig
    ```

Com isso, as bibliotecas compartilhadas do E2sim ficam registradas no sistema, fornecendo as dependências necessárias para que o simulador ns-3 consiga realizar a codificação e decodificação das mensagens E2AP e E2SM.

## 3. Configuração do ns-3 e Integração do módulo ns-O-RAN

O ecossistema ns-O-RAN depende do simulador ns-3 com o módulo mmWave. 

*Nessa etapa não são necessárias permissões de administrador.*

* Clone o repositório do simulador ns-3 modificado:
  ```bash
  git clone -b release https://github.com/wineslab/ns-o-ran-ns3-mmwave ~/ns3-mmwave-oran
  ```
  * Essa versão do repositório carrega o código do ns-3.33 em conjunto com o módulo mmWave

* Clone o módulo oficial da interface O-RAN (sim-ns3-o-ran-e2) diretamente para o diretório contrib/oran-interface do simulador:
    ```bash
    git clone -b master https://github.com/o-ran-sc/sim-ns3-o-ran-e2 ~/ns3-mmwave-oran/contrib/oran-interface
    ```

* Com o repositório completo e os módulos integrados, o próximo passo é configurar o sistema de compilação do simulador utilizando os parâmetros adequados.

    * As flags de configuração:

      * ```--enable-examples```: Habilita a compilação dos scripts de exemplo embutidos no ns-3 e nos seus módulos. É fundamental para que possamos validar a instalação rodando cenários de testes já prontos.

      * ```--enable-tests```: Constrói a suíte de testes unitários e de integração do ns-3. Permite executar rotinas de verificação nativas para garantir que o núcleo do simulador e as bibliotecas não apresentam falhas lógicas após a compilação.

      * ```--disable-werror```: Instrui o sistema a não tratar avisos do compilador (*warnings*) como erros críticos (*errors*). Como estamos compilando módulos externos e bibliotecas complexas, é comum que surjam avisos de sintaxe ou depreciação; esta flag impede que a compilação seja interrompida abruptamente por detalhes não impeditivos.

      * ```--build-profile```: Define o perfil de desempenho e otimização da compilação. Os perfis mais utilizados são:

          * ```debug```: Ideal para desenvolvimento. Mantém os símbolos de depuração (debugging symbols), logs detalhados e asserts ativados, **porém torna a simulação mais lenta**.

          * ```optimized```: Focado em performance. Aplica agressivas otimizações de código e desativa logs/asserts internos do ns-3, **reduzindo drasticamente o tempo de execução de simulações pesadas**.
      
      * ```--out```: Define o diretório de saída da compilação. Permite manter múltiplas *builds* em pastas separadas. Isso possibilita chavear entre diferentes perfis ou configurações sem precisar recompilar todo o código do zero, processando apenas as diferenças. 
  
  ```bash
  cd ns3-mmwave-oran
  # Para compilar uma build otimizada
  ./waf configure --enable-examples --enable-tests --disable-werror --build-profile=optimized --out=build/optimized
  ```
* Por fim, já é possível compilar o código completo do simulador:
    ```bash
    ./waf build
    ```
*Esse processo pode demorar um pouco, de acordo com os recursos da sua máquina!*

<div align="center">
  <img src="./figures/lab02/ns3_build_success.png" alt="ns-3 Build Success!">
</div>

A conclusão da build com sucesso marca o fim deste laboratório. Com as bibliotecas E2sim e os módulos de rádio devidamente compilados no ns-3, a sua máquina está oficialmente pronta para iniciar as simulações e conectar-se à plataforma O-RAN.

##