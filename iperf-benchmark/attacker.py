import socket
import time
import subprocess
import threading

import serial


# Attacking computer UDP server configs
class Attacker():
    def __init__(self):
        # Service used for synchronizing Attacker and ESP32
        self.service = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.service.bind(('0.0.0.0', 6767))

        # Port used to collect experiment data - runs in another thread
        self.data_port = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.data_port.bind(('0.0.0.0', 6768))

        self.experiment = None
        self.experiment_running = False
        self.logs = []

    def _collect(self):
        while self.experiment_running:
            try:
                data, _ = self.data_port.recvfrom(4096, socket.MSG_DONTWAIT)
                # print(f"\n========\nreceived data: {data}\n========\n")
            except IOError as e:
                print(f"[!] no data in collect - {e}")
            else:
                print("[+] collect received data")
                data = str(data.decode())
                # print(data)
                log = {}
                for line in data.split("\r\n"):
                    metric, value = tuple(map(lambda x: x.strip(), line.split('\t')[:2]))
                    # print(f"metric: {metric}\tvalue: {value}")
                    if metric != "main":
                        log[metric] = value

                self.logs.append(log)

            time.sleep(1)

    def collect_experiment_data(self):
        print("[+] Start collect_experiment_data")
        self.experiment_running = True
        self.experiment = threading.Thread(target=self._collect)
        self.experiment.start()

    def stop_experiment(self, fname):
        self.experiment_running = False
        self.experiment.join()

        # append the experiment log to CSV file
        data_file = open(fname, "a+")

        # sort all the dictionaries to guarantee the order
        for i in range(len(self.logs)):
            self.logs[i] = dict(sorted(self.logs[i].items()))

        header = ",".join(self.logs[1].keys()) + "\n" # all logs send the same data, so choose any
        data_file.write(header)

        # print(f"[+] collected {len(self.logs)} data_points")
        # print(f"logs: {self.logs}")
        for log in self.logs:
            # since all logs have the same keys, always inserted in the same order,
            # iteration order of log.values() should be consistent
            formatted_values = ",".join(log.values()) + "\n" # format to CSV
            data_file.write(formatted_values)

        data_file.close()
        print(f"[-] Saved log into file '{fname}'")

        self.logs.clear()


def msg_esp(expected, attacker, esp32_addr=None, msg=None, is_sync=False):
    esp32_signal = b""
    while esp32_signal.strip() != expected:
        if msg:
            attacker.service.sendto(msg, esp32_addr)

        esp32_signal, esp32_addr = attacker.service.recvfrom(4096)

    # warn ESP that we received the expected message
    if not is_sync:
        attacker.service.sendto(b"x", esp32_addr)

    return esp32_addr


def serial_monitor(file_name, ser, stop):
    serial_msg = ""
    print(">> Monitoring test case: ", file_name)
    with open(file_name, "w") as f:
        while True:
            character = ser.read().decode()
            #print(f'c = {character}')
            if character == "\n":
                f.write(serial_msg+character)
                serial_msg = ""
            else:
                serial_msg += character

            if stop():
                print('here')
                break

    print(">> Stopping monitoring")


def main():
    port = "/dev/ttyACM0"
    baud_rate = 115200

    attacker = Attacker()

    models = [b"0"]
    for tree in models:
        for i in range(30):
            print(f"\n\n============ I = {i} ============\n\n")

            ser = serial.Serial(port, baud_rate)

            print("Going to tree", tree)

            esp32_addr = msg_esp(b"start", attacker, is_sync=True)

            print("[+] Starting experiment:")
            print(f"esp32_addr: {esp32_addr} tree: {tree}")

            esp32_addr = msg_esp(b"assigned", attacker, esp32_addr, tree)

            print(f"[+] ESP32 assigned tree {tree}")

            stop_thread = False
            monitor = threading.Thread(target=serial_monitor, args=(f'./output_pwr/I1M0A0_{i}_newrun.csv', ser, lambda: stop_thread, ))
            monitor.start()

            print("[>] Sending packets ...")
            attacker.collect_experiment_data()
            time.sleep(2)   # Wait for esp32 open iperf server

            iperf = subprocess.Popen(["iperf", "-c", esp32_addr[0], "-B", "0.0.0.0:5001", "-i", "1", "-t", "360", "-p", "5001", "-b", "16000000pps"], start_new_session=True)
            #nmap = subprocess.Popen(["nmap", "-sS", esp32_addr[0], "-p-", "-A", "-T", "insane"], start_new_session=True)
            iperf.wait()
            #nmap.kill()
            print("[>] Finished sending packets")

            attacker.stop_experiment("./output_data/I1M0A0_newrun.csv")

            # Receiving experiment results
            msg_esp(b"complete", attacker, esp32_addr, b"D")

            print("[+] ESP32 experiment complete, moving to next index")
            print("[+] Experiment data saved in file")

            # stop serial monitor thread and serial
            stop_thread = True
            monitor.join()
            ser.close()

            time.sleep(5)


if __name__ == "__main__":
    main()
