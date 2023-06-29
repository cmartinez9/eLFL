#include <ArduinoJson.h>
#include <ArduinoJson.hpp>


/* electric Little Free Library - v0
  Author: Maitreyee Marathe
  Last modified: Apr 1, 2022
  Platform: Arduino Nano Every
  This program is used to record sensor readings from a monitoring system for 
  a solar+battery powered USB charging station and log it to an SD card (OpenLog)
  connected via the TX pin.
  The Arduino Nano Every has two serial ports:
  "Serial" - for computer via micro-USB port
  "Serial1" - for the TX/RX pins (pins 16/17)

  Sensor Inputs
  - Load Current
  - Battery Current (+ve when getting charged)
  - Temperature
  - Battery Voltage

  Outputs
  - Green LED ON, Red LED OFF- voltage above v_high
  - Green LED OFF, Red LED ON - voltage below v_low
  - Previous state - voltage between v_low and v_high (deadband)
*/

// Date and time functions using a DS1307 RTC connected via I2C and Wire lib
//#include <LowPower.h>
//#include <RadioHead.h>
//#include <RH_RF95.h>
#include "RTClib.h"
// #include <ArduinoJson.h>
// #include <ArduinoJson.hpp>

RTC_DS1307 rtc;

#define NODE_ID 23

#define TEMP_SENSE
#define LOAD_SENSE
#define CUR_SENSE
#define V_SENSE
//#define DOOR_SENSE
//#define LORA_ACTIVE
//#define OPEN_LOG_ACTIVE
#define RTC_ACTIVE

#define MODEM_CONFIG 21

// +2 to +20, 13 default
// #define TX_POWER 23
// #define SF       10
// #define BW       125000
// #define CR       7
#define TX_POWER 23
#define SF 11
#define BW 500000
#define CR 5
#define EN 4

#ifdef LORA_ACTIVE
RH_RF95 rf95(10, 3);
#endif

#define TEMP_PIN A0
#define LOAD_PIN A1
#define CUR_BATT_PIN A2
#define V_BATT_PIN A3
#define DOOR_PIN A6
#define RED_LED_PIN 2
#define GRN_LED_PIN 5

// Sensor parameters
int iLoad_value = 0;
int iBatt_value = 0;
int temperature_value = 0;
int vBatt_value = 0;
int door_value = 0;
// delay time for every 10 minutes
unsigned long minute = 10;
unsigned long readDelay = minute * 60000;

float vBatt = 0;
const float sensitivity = 2.5;  // current sensor sensitivity
const float Vref_Load = 2500;   // load current sensor output voltage with no current
const float Vref_Batt = 2420;   // battery current sensor output voltage with no current
const float v_low = 11.5;       // battery voltage limit to switch ON red LED
const float v_high = 12;        // battery voltage limit to swithc ON green LED
const float logTime = 10000;    // time duration (ms) between two data entries to the SD card
const int delayTime = 2;        // time duration (ms) between two sensor readings
const int avgSamples = 10;      // no. of sensor readings to compute average

StaticJsonDocument<200> doc;

DateTime now;

void setup() {

  Serial.begin(9600);
  Serial.println("Start Init");
  initSensors();
#ifdef LORA_ACTIVE
  initLoRa();
#endif
  Serial.println("Finished Init");
}


void loop() {
  doc["node"] = NODE_ID;

#ifdef RTC_ACTIVE
  checkTime();
#endif

  sampleAnalog();

  calcValues();

#ifdef DOOR_SENSE
  checkDoor();
#endif

  chngLedStatus();

  // delay(logTime - delayTime*avgSamples);

  serializeJsonPretty(doc, Serial);
  Serial.println();
#ifdef LORA_ACTIVE
  sendPacket();
#endif
  doc.clear();
  delay(readDelay);
  // delay(logTime - delayTime*avgSamples);
  //LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}

bool initSensors() {
#ifdef TEMP_SENSE
  pinMode(TEMP_PIN, INPUT);
#endif
#ifdef LOAD_SENSE
  pinMode(LOAD_PIN, INPUT);
#endif
#ifdef CUR_SENSE
  pinMode(CUR_BATT_PIN, INPUT);
#endif
#ifdef V_SENSE
  pinMode(V_BATT_PIN, INPUT);
#endif
#ifdef DOOR_SENSE
  pinMode(DOOR_PIN, INPUT);
#endif
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GRN_LED_PIN, OUTPUT);

#ifdef RTC_ACTIVE
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }


  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
 
#endif
}

void checkTime() {
  now = rtc.now();
  String month = String(now.month(), DEC);
  String day = String(now.day(), DEC);
  String year = String(now.year(), DEC);
  String hour = String(now.hour(), DEC);
  String min = String(now.minute(), DEC);
  String sec = String(now.second(), DEC);
  String date = month + "-" + day + "-" + year;
  String time = hour + ":" + min + ":" + sec;
  doc["date"] = date;
  doc["time"] = time;
  Serial.println("TIME CHECKED");
}

bool sampleAnalog() {
  // Collect samples every (delayTime) ms and compute average
  iLoad_value = 0;
  iBatt_value = 0;
  temperature_value = 0;
  vBatt_value = 0;

  for (int i = 0; i < avgSamples; i++) {
#ifdef LOAD_SENSE
    iLoad_value += analogRead(LOAD_PIN);
    if (i == avgSamples - 1)
      Serial.println("LOAD CHECKED");
#endif
#ifdef CUR_SENSE
    iBatt_value += analogRead(CUR_BATT_PIN);
    if (i == avgSamples - 1)
      Serial.println("CURRENT CHECKED");
#endif
#ifdef TEMP_SENSE
    temperature_value += analogRead(TEMP_PIN);
    if (i == avgSamples - 1)
      Serial.println("TEMP CHECKED");
#endif
#ifdef V_SENSE
    vBatt_value += analogRead(V_BATT_PIN);
    if (i == avgSamples - 1)
      Serial.println("VOLT CHECKED");
#endif
    delay(delayTime);
  }
}

bool calcValues() {

// Load Current (mA)
#ifdef LOAD_SENSE
  iLoad_value = iLoad_value / avgSamples;
  float voltage_temp = 4.88 * (iLoad_value);
  float iLoad = -(voltage_temp - Vref_Load) * sensitivity;  // negative due to wiring
  doc["load_cur"] = iLoad;
  Serial.println("LOAD CALCULATED");
#endif

// Battery Current (mA)
#ifdef CUR_SENSE
  iBatt_value = iBatt_value / avgSamples;
  float voltage_temp2 = 4.88 * (iBatt_value);
  float iBatt = (voltage_temp2 - Vref_Batt) * sensitivity;

  doc["bat_cur"] = iBatt;
  Serial.println("CURRENT CALCULATED");
#endif

// Battery Voltage (V)
#ifdef V_SENSE
  vBatt_value = vBatt_value / avgSamples;
  vBatt = 25 * vBatt_value;
  vBatt = vBatt / 1023;

  doc["bat_volt"] = vBatt;
  Serial.println("VOLT CALCULATED");
#endif

// Temperature (Celsius)
#ifdef TEMP_SENSE
  temperature_value = temperature_value / avgSamples;
  float voltage_temp3 = temperature_value * 5.0;
  voltage_temp3 /= 1024.0;
  //converting from 10 mv per degree with 500 mV offset to degrees
  float temperature = (voltage_temp3 - 0.5) * 100;

  doc["temp"] = temperature;
  Serial.println("TEMP CALCULATED");
#endif
}

bool checkDoor() {
  // Door sensor
  door_value = digitalRead(DOOR_PIN);  //= 0 if door closed, = 1 if door open

  doc["door"] = door_value;
  Serial.println("DOOR CHECKED");
}

bool chngLedStatus() {
  // Green LED ON when battery voltage more than v_high
  // Red LED ON when battery voltage less than v_low
  // Previous state when battery voltage between v_low and v_high (deadband)
  if (vBatt >= v_high) {
    digitalWrite(GRN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    Serial.println("LED CHANGED GREEN");
  } else if (vBatt <= v_low) {
    digitalWrite(GRN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    Serial.println("LED CHANGED RED");
  } else {
    Serial.println("NO LED CHANGE");
  }
}
#ifdef LORA_ACTIVE
bool initLoRa() {
  // pinMode(EN, OUTPUT);
  // digitalWrite(EN, HIGH);

  while (!Serial)
    ;  // Wait for serial port to be available
  if (!rf95.init())
    Serial.println("LORA INIT FAILED");

  rf95.waitPacketSent();
  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  rf95.setModemConfig(MODEM_CONFIG);  // set new modem configuration
  rf95.setFrequency(915.0);
  rf95.setTxPower(TX_POWER, false);  // set new transmit power configuration
  rf95.setSpreadingFactor(SF);       // set new spreading factor 2^10
  rf95.setSignalBandwidth(BW);       // set new bandwidth
  rf95.setCodingRate4(CR);           // set new coding rate
  rf95.setPayloadCRC(true);
}

bool sendPacket() {
  char packet[200];
  serializeJson(doc, packet);

  bool sent = rf95.send(packet, sizeof(packet));  // Added bool sent in front to check status - true if correctly queues for transmit

  rf95.waitPacketSent();
  Serial.println("PACKET SENT");

  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  if (rf95.waitAvailableTimeout(2000)) {
    if (rf95.recv(buf, &len)) {
      Serial.print("RECIEVED: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);
    } else {
      Serial.println("RECIEVED FAILED");
    }
    delay(1000);
  } else {
    Serial.println("");
  }
}
#endif

