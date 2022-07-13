#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219;

int16_t curr_array[16];
int i;
String comma = ",";
int digital_pin_5 = 0;
int signalFromESP = 5; // Digital Input #5, discrete signal from ESP to Nano BLE

void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(115200);
  while (!Serial) {
    delay(1);
  }
  
  if (!ina219.begin()){
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }

  pinMode(signalFromESP, INPUT);
  
  Serial.println("Measuring voltage and current with INA219...");
}


void loop() {
  // put your main code here, to run repeatedly:
  float shuntvoltage = 0;
  float busvoltage = 0;
  float current_mA = 0;
  float loadvoltage = 0;
  float power_mW = 0;

  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  power_mW = ina219.getPower_mW();
  loadvoltage = busvoltage + (shuntvoltage / 1000);

  digital_pin_5 = digitalRead(signalFromESP);
  
  Serial.println(busvoltage+comma+shuntvoltage+comma+loadvoltage+comma+current_mA+comma+power_mW+comma+digital_pin_5);
  //delay(10);

}
