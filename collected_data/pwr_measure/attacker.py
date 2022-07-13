import socket
import time
import subprocess
import threading
import sys
import serial

# Attacking computer UDP server configs
class Attacker():
    def __init__(self, fname):
        # Service used for synchronizing Attacker and ESP32
        self.service = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.service.bind(('0.0.0.0', 6767))

        # Port used to collect experiment data - runs in another thread
        self.data_port = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.data_port.bind(('0.0.0.0', 6768))
        self.fname = fname
        self.header_written = False
        self.tasks = dict()

        self.experiment = threading.Thread(target=self._collect, args=(fname, ))
        self.experiment_running = False

    def _collect(self, fname):
        data_file = open("./data/" + fname, "a+")
        period = 0
        while self.experiment_running:
            try:
                data, _ = self.data_port.recvfrom(4096, socket.MSG_DONTWAIT)
            except IOError as e :
                print(f"[!] no data in collect - {e}")
            else:
                print("[+] collect received data")
                data = str(data.decode())
                for line in data.split("\r\n"):
                    name, metric = tuple(map(lambda x: x.strip(), line.split('\t')[:2]))
                    self.tasks[name] = metric

                exp_data = ",".join(self.tasks.values())
                experiment = f"{period},{exp_data}\n"
                if not self.header_written:
                    header = "period," + ",".join(self.tasks.keys()) + "\n"
                    data_file.write(header)
                    self.header_written = True

                data_file.write(experiment)
                period += 1

            time.sleep(1)

        data_file.close()

    def collect_experiment_data(self):
        print(f"[+] Start collect_experiment_data for file '{self.fname}'")
        self.experiment_running = True
        self.experiment = threading.Thread(target=self._collect, args=(self.fname, ))
        self.experiment.start()

    def stop_experiment(self):
        self.experiment_running = False
        self.experiment.join()
        self.header_written = False
        print(f"[-] Stopped collecting - closed file '{self.fname}'")


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
    t = threading.currentThread()
    serial_msg = ""
    print(">> Monitoring test case: ", file_name)
    with open(file_name, "w") as f:
        while True:
            character = ser.read().decode()
            if character == "\n":
                f.write(serial_msg+character)
                serial_msg = ""
            else:
                serial_msg += character

            if stop():
                break

    print (">> Stopping monitoring")

def main():
    port = "/dev/ttyACM0"
    baud_rate = 115200
    
    path = "./output/"

    trees = [b"n"] #[b"0", b"2", b"m", b"n"]
    for tree in trees:
        for pkt_count in ["8000000pps"]: #["16000000pps", "8000000pps"]:
            test_case = tree.decode()+'_'+str(pkt_count)+'_'+str("nonmap") # insane / nonmap
            test_case.replace('\'',"").replace('"',"")
            file_name = path + test_case + ".csv"

            attacker = Attacker("data_"+test_case+".csv")

            ser = serial.Serial(port, baud_rate)

            print("Going to tree", tree)

            esp32_addr = msg_esp(b"start", attacker, is_sync=True)

            print("[+] Starting experiment:")

            esp32_addr = msg_esp(b"assigned", attacker, esp32_addr, tree)

            print(f"[+] ESP32 assigned tree {tree}")

            # Thread for serial monitor in each test case
            stop_thread = False
            monitor = threading.Thread(target=serial_monitor, args=(file_name, ser, lambda: stop_thread, ))
            monitor.start()

            print("[>] Sending packets ...")
            attacker.collect_experiment_data()
            time.sleep(2)   # Wait for esp32 open iperf server
            iperf = subprocess.Popen(["iperf", "-c", esp32_addr[0], "-B", "0.0.0.0:5001", "-i", "1", "-t", "180", "-p", "5001", "-b", pkt_count], start_new_session=True)
            #nmap = subprocess.Popen(["nmap", "-sS", esp32_addr[0], "-p-", "-A", "-T", "insane"], start_new_session=True) #always insane
            #while iperf.poll() == None:
                #if nmap.poll() != None:
                    #nmap = subprocess.Popen(["nmap", "-sS", esp32_addr[0], "-p-", "-A", "-T", "insane"], start_new_session=True)
            #iperf.wait()
            #nmap.kill()
            print("[>] Finished sending packets")

            attacker.stop_experiment()

            # Receiving experiment results
            msg_esp(b"complete", attacker, esp32_addr, b"D")

            print("[+] ESP32 experiment complete, moving to next index")
            print("[+] Experiment data saved in file")

            # stop serial monitor thread and serial
            stop_thread = True
            monitor.join()
            ser.close()
            del attacker
            time.sleep(5)

if __name__ == "__main__":
    main()
