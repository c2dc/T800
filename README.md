# T800: packet filtering and benchmark for IoT

## Requirements
Prior to run the benchmark with T800 with ESP32, the following softwares are required on the host computer:
- Python 3 _(tested with v3.8)_
- [ESP-IDF Framework ](https://github.com/espressif/esp-idf)
- *iperf v2* _(tested with 2.0.13)_
- *NMap* _(tested with 7.80)_

## How to Install

1. Install the `t800` component and its dependencies on the ESP-IDF, run `install.sh`:

```
$ source install.sh
```

2. It is required to configure the network. Enter the folder `iperf-benchmark` and run `idf.py menuconfig`. Change the values at the menu option `Example Connection Configuration`. Change the SSID and the Wi-Fi network password.

```
$ cd iperf-benchmark
$ idf.py menuconfig
```

3. Em seguida, para subir a aplicação para a ESP32, conecte-a por USB à sua maquina, então encontre a porta que ela está conectada e, por fim, execute `idf.py flash -p <PORTA_DA_ESP32>`. Execute 1 vez a aplicação, com `idf.py -p <YOUR_ESP32_PORT> monitor`, para que a ESP obtenha um IP na sua rede e finalize essa execução com `ctrl+[`.

4. Finalmente, para poder executar o benchmark, execute `idf.py menuconfig` e altere as configurações em `Benchmark Configuration` para incluir o IPv4 da máquina atacante e da ESP32. Então:
   * Rode `sudo python attacker.py` em uma janela de terminal
   * Rode `idf.py -p <YOUR_ESP32_PORT> flash monitor` em outra janela de terminal, em paralelo.
   * Depois de realizar o experimento, um arquivo `data.csv` será gerado com todos os dados coletados durante o experimento.

Para nossos experimentos, o dispositivo utilizado foi a ESP32. É válido ressaltar que as portas TCP `6767`, `6768` e `5001` ***não*** podem estar sendo utilizadas pelo Sistema Operacional durante a execução dos experimentos. Caso elas estejam sendo utilizadas, essas configurações podem ser alteradas utilizando `idf.py menuconfig`.


> Obs: Durante o experimento o pino D5 será 0 quando o experimento não estiver rodando e 1 durante sua execução.

## Cite
```
```

