# T800: IoT Firewall and Benchmarking Tool
---

An intelligent packet filter for embedded devices using the LwIP TCP/IPv4 stack.

## Prerequisites
Before running the benchmark with the T800 on the ESP32 system, the following software must be installed on your machine:
- Python3
- [ESP-IDF Framework](https://github.com/espressif/esp-idf) by Espressif
- *iperf v2*
- *NMap*

## Installation
For our experiments, the device used was the ESP32. It is worth noting that TCP ports `6767`, `6768`, and `5001` ***cannot*** be in use by the operating system during the experiment. If they are in use, these settings can be changed using `idf.py menuconfig`.

To install the `t800` component and its dependencies on your ESP-IDF system, run the `install.sh` script.

To configure the network on which the experiment will be run, go to the `iperf-benchmark` folder and run `idf.py menuconfig`. Change the values in the `Example Connection Configuration` menu to the SSID and password of your Wi-Fi.

Next, to upload the application to the ESP32, connect it to your machine via USB, then find the port it is connected to, and finally run `idf.py flash -p <YOUR_ESP32_PORT>`. Run the application once with `idf.py -p <YOUR_ESP32_PORT> monitor` so that the ESP obtains an IP on your network, and end this execution with `ctrl+[`.

Finally, to run the benchmark, run `idf.py menuconfig` and change the settings under `Benchmark Configuration` to include the IP address of the attacking machine and the ESP32. Then:
1. Run `sudo python attacker.py` in one terminal window
2. Run `idf.py -p <YOUR_ESP32_PORT> flash monitor` in another terminal window, in parallel.
3. After running the experiment, a `data.csv` file will be generated with all the data collected during the experiment.

> Note: During the experiment, pin D5 will be 0 when the experiment is not running and 1 during its execution.


## How does the T800 work?
The T800 needs to be previously configured in the application in which it is to be used. To do this, it is necessary to allocate a `t800_config_t` struct with the filter usage mode and packet classification function. The usage mode can be of type `SELECTED` or `UNINITIALIZED`. The classification function is given by some Tensorflow model, converted to Tensorflow Lite and previously loaded onto the ESP32. It is possible to use even a simpler model with conditional structures.

Next, it is only necessary to call the `t800_init()` function, passing the configuration struct as an argument. This function is responsible for definitively initializing the T800 system.

Thus, as the T800 communicates directly with the TCP/IPv4 stack of LwIP, from the moment packets start arriving at the device (ESP32), the `t800_run()` function is invoked. This function receives as parameters the IP and TCP headers of the packet that arrived on the board. This function is intended to verify the mode of operation of the T800 at that moment and invoke the previously selected classification function.

## How to obtain computational metrics?

For our experiments, a UDP server made in python that receives data from the ESP32 from time to time (in our experiments, a time of 1 second was established) was used.

It is worth noting that data collection is performed in a separate thread from the execution of experiments, thus ensuring the parallelization of the experiment and data collection.

## How to generate network traffic on ESP32?

### Benign traffic

For this purpose, the T800 has an independently implemented *iperf client* that runs on the user's computer system and receives experiments on the *iperf server*. This ensures that the data collected by the *iperf client* is reliable and accurately represents what was caused by the experiment.

To start the *iperf client* on ESP32, you must call the `iperf_tcp_server()` function with only one parameter: the UDP socket. Within this function, the *iperf client* will be configured and packets from the *iperf server* will be received.

To configure the *iperf client*, you only need to configure the TCP socket that will establish the connection with the *iperf server*. This is done by calling the `iperf_setup_tcp_server()` function, passing a `sockaddr_in` struct as a parameter that will serve the purpose of configuring the *iperf client* socket.

It is worth noting that while the *iperf client* is running, another thread is responsible for collecting data on the computational resources of ESP32.

### Malicious traffic

For controlling malicious traffic, the python script that contains all the logic for receiving data and communicating with the ESP32 *iperf client* socket also runs the *NMap* command.

*NMap* runs in conjunction with the *iperf server* on the attacker's side (the user who wants to receive metrics from the board), and with the help of some flags, it is possible to control its traffic, such as the `-T` flag.

## The data.csv file

This file is the result of the metrics collected in the experiment. It is important to note that the most relevant metrics are:

- Timestamp: uniquely identifies the data points every second.
- Mbps: identifies network speed at a given moment.
- wifi: identifies the absolute time spent processing packets.
- Stack: identifies the maximum stack usage in a particular task.

This file is generated with each experiment run and may have numerous replicas of a given experiment (automated).

## How to obtain energy metrics?

To obtain energy metrics, a NodeMCU (referred to in the work generically as ESP32) based on the [ESP32-WROOM-32](https://www.espressif.com/en/products/devkits/esp32-devkitc) microcontroller was used as the board to be measured. The current sensor for measuring power consumption was the [INA219](https://github.com/adafruit/Adafruit_INA219), and for reading this sensor, the available I2C communication on this integrated circuit was used for reading by a second microcontrolled board ([Nano BLE Sense 33](https://store.arduino.cc/products/arduino-nano-33-ble-sense)). This reading board transmits the obtained data via I2C by serial communication to the host computer through a USB cable.

The source code for reading the INA219 sensor via I2C and serial transmission is available in [this repository](https://github.com/c2dc/t800-sbrc2022/blob/main/collected_data/pwr_measure/measure_power_nano_ble/measure_power_nano_ble.ino). The board responsible for reading the INA219 sensor reads the following parameters:

- Voltage (V)
- Shunt Voltage (mV)
- Load Voltage (V)
- Current (mA)
- Power (mW)

In addition to the 5 parameters provided via I2C by the current sensor INA219, the digital pin D5 of the ESP32 board is also read. This digital pin is used as part of the experiment to indicate when an experiment starts (going to high level) and when it ends (returning to low level). This facilitates the analysis of the data generated by the experiments, since only readings while the discrete signal is at high level are considered.

Thus, the 6 parameters read by the Nano BLE Sense 33 board at maximum frequency (infinite loop without delay) are transmitted via serial communication to the host computer with a baud rate of 115200 bps and the message in the comma-separated values format. The reading of the serial communication by the host computer is part of the script "attacker.py" (as in the following [code snippet](https://github.com/c2dc/T800/blob/main/collected_data/pwr_measure/attacker.py#L81-L97)).

A schematic of the measurement system setup is available [here](./extra/sketch.fzz) and should be viewed with the Fritzing tool. And an image of the experimental setup for obtaining energy metrics:
<p align="center">
  <img src="./extra/setup.png" alt="Experimental Setup for Power Measurement" width="70%"/>
</p>

