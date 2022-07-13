# T800: ferramenta de firewall e benchmark para IoT

## Pré-requisitos
Antes de executar o benchmark com o T800 no sistema da ESP32, os seguintes softwares devem estar instalados na sua máquina:
- Python3
- [Framework ESP-IDF](https://github.com/espressif/esp-idf) feito pela empresa Espressif
- *iperf v2*
- *NMap*

## Como instalar
Para nossos experimentos, o dispositivo utilizado foi a ESP32. É válido ressaltar que as portas TCP `6767`, `6768` e `5001` ***não*** podem estar sendo utilizadas pelo Sistema Operacional durante a execução dos experimentos. Caso elas estejam sendo utilizadas, essas configurações podem ser alteradas utilizando `idf.py menuconfig`.

Para instalar o componente `t800` e suas dependências no ESP-IDF do seu sistema, execute o script `install.sh`.

Para configurar a rede em que o experimento vai ser executado, entre na pasta `iperf-benchmark` e execute `idf.py menuconfig`. Altere os valores no menu `Example Connection Configuration` para a SSID e a senha da sua Wi-Fi.

Em seguida, para subir a aplicação para a ESP32, conecte-a por USB à sua maquina, então encontre a porta que ela está conectada e, por fim, execute `idf.py flash -p <PORTA_DA_ESP32>`. Execute 1 vez a aplicação, com `idf.py -p <YOUR_ESP32_PORT> monitor`, para que a ESP obtenha um IP na sua rede e finalize essa execução com `ctrl+[`.

Finalmente, para poder executar o benchmark, execute `idf.py menuconfig` e altere as configurações em `Benchmark Configuration` para incluir o IPv4 da máquina atacante e da ESP32. Então:
1. Rode `sudo python attacker.py` em uma janela de terminal
2. Rode `idf.py -p <YOUR_ESP32_PORT> flash monitor` em outra janela de terminal, em paralelo.
3. Depois de realizar o experimento, um arquivo `data.csv` será gerado com todos os dados coletados durante o experimento.

> Obs: Durante o experimento o pino D5 será 0 quando o experimento não estiver rodando e 1 durante sua execução.