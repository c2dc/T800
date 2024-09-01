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

3. To upload the application to the ESP32, connect it to your machine via USB, then find the port it's connected to and finally run `idf.py flash -p <YOUR_ESP32_PORT>`. Run the application once with `idf.py -p <YOUR_ESP32_PORT> monitor`, so that the ESP can obtain an IP on your network, and end this execution with `ctrl+[`.

4. Finally, to be able to run the benchmark, run `idf.py menuconfig` and change the settings in `Benchmark Configuration` to include the IPv4 of the attacking machine and the ESP32. Then:
   * Run `sudo python attacker.py` in a terminal window.
   * Run `idf.py -p <YOUR_ESP32_PORT> flash monitor` in another terminal window, in parallel.
   * After conducting the experiment, a file `data.csv` will be generated with all the data collected during the experiment.

For our experiments, the device used was the ESP32. It is worth noting that TCP ports `6767`, `6768`, and `5001` ***cannot*** be in use by the Operating System during the experiment. If they are being used, these settings can be changed using `idf.py menuconfig`.

> Note: During the experiment, pin D5 will be 0 when the experiment is not running and 1 during its execution.

## Reproducibility

More details on how to reproduce this work is available inside the `manual` folder in both [English](https://github.com/c2dc/T800/blob/main/manual/manual_en.md) and [pt-BR](https://github.com/c2dc/T800/blob/main/manual/manual_pt-br.md).

## Cite
```
@article{10.5753/jisa.2024.3835,
  title={Design and implementation of intelligent packet filtering in IoT microcontroller-based devices},
  volume={15},
  url={https://journals-sol.sbc.org.br/index.php/jisa/article/view/3835},
  DOI={10.5753/jisa.2024.3835},
  number={1},
  journal={Journal of Internet Services and Applications},
  author={Bertoli, Gustavo de Carvalho and Fernandes, Gabriel Victor C. and Monici, Pedro H. Borges and Guibo, César H. de Araujo and Santos, Aldri Luiz dos and Pereira Júnior, Lourenço Alvez},
  year={2024},
  month={Aug.},
  pages={289–301}
}
```

