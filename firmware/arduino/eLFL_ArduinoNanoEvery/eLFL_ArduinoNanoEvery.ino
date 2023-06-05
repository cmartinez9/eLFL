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
#include "RTClib.h"
RTC_DS1307 rtc;

// Define pins
const int temperature_pin = A0;
const int iLoad_pin = A1;
const int iBatt_pin = A2;
const int vBatt_pin = A3;
const int door_pin = A6;
const int red_pin = 2;
const int green_pin = 3;

// Sensor parameters
int iLoad_value = 0;
int iBatt_value = 0;
int temperature_value = 0;
int vBatt_value = 0;
int door_value = 0;
const float sensitivity = 2.5; // current sensor sensitivity
const float Vref_Load = 2500; // load current sensor output voltage with no current
const float Vref_Batt = 2420; // battery current sensor output voltage with no current
const float v_low = 11.5; // battery voltage limit to switch ON red LED
const float v_high = 12; // battery voltage limit to swithc ON green LED
const float logTime = 10000; // time duration (ms) between two data entries to the SD card
const int delayTime = 2; // time duration (ms) between two sensor readings
const int avgSamples = 10; // no. of sensor readings to compute average

void setup() {
  Serial1.begin(57600); 
  Serial.begin(57600);
  pinMode(temperature_pin, INPUT);
  pinMode(iLoad_pin, INPUT);
  pinMode(iBatt_pin, INPUT);
  pinMode(vBatt_pin, INPUT);
  pinMode(door_pin, INPUT);
  pinMode(red_pin,OUTPUT);
  pinMode(green_pin, OUTPUT);


  #ifndef ESP8266
    while (!Serial); // wait for serial port to connect. Needed for native USB
  #endif
  
  if (! rtc.begin()) {
      Serial.println("Couldn't find RTC");
      Serial.flush();
      while (1) delay(10);
  }
  

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  // When time needs to be re-set on a previously configured device, the
  // following line sets the RTC to the date & time this sketch was compiled
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // This line sets the RTC with an explicit date & time, for example to set
  // January 21, 2014 at 3am you would call:
  // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));  
  
}


void loop() {
    DateTime now = rtc.now();
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.print(", ");
    
    Serial1.print(now.year(), DEC);
    Serial1.print('/');
    Serial1.print(now.month(), DEC);
    Serial1.print('/');
    Serial1.print(now.day(), DEC);
    Serial1.print(" ");
    Serial1.print(now.hour(), DEC);
    Serial1.print(':');
    Serial1.print(now.minute(), DEC);
    Serial1.print(':');
    Serial1.print(now.second(), DEC);
    Serial1.print(", ");
  
  
  // Collect samples every (delayTime) ms and compute average
  iLoad_value = 0;
  iBatt_value = 0;
  temperature_value = 0;
  vBatt_value = 0;
  for (int i = 0; i < avgSamples; i++)
  {
    iLoad_value += analogRead(iLoad_pin);
    iBatt_value += analogRead(iBatt_pin);    
    temperature_value += analogRead(temperature_pin);  
    vBatt_value += analogRead(vBatt_pin);       
    delay(delayTime);
  }
  
  // Load and Battery Current (mA)
  iLoad_value = iLoad_value / avgSamples;
  iBatt_value = iBatt_value / avgSamples;

  temperature_value = temperature_value / avgSamples;
  vBatt_value = vBatt_value / avgSamples;  
  float voltage_temp = 4.88 * (iLoad_value);
  float iLoad = -(voltage_temp - Vref_Load) * sensitivity; // negative due to wiring
  voltage_temp = 4.88 * (iBatt_value);
  float iBatt = (voltage_temp - Vref_Batt) * sensitivity;
  

  Serial1.print(String(iLoad));
  Serial1.print(", ");
  Serial1.print(String(iBatt));
  Serial1.print(", ");

  Serial.print(String(iLoad));
  Serial.print(", ");
  Serial.print(String(iBatt));
  Serial.print(", ");  
  
  // Temperature (Celsius)
  voltage_temp = temperature_value * 5.0;
  voltage_temp /= 1024.0; 
  //converting from 10 mv per degree with 500 mV offset to degrees
  float temperature = (voltage_temp - 0.5) * 100 ;
  Serial1.print(String(temperature));
  Serial1.print(", ");  
  Serial.print(String(temperature));
  Serial.print(", ");    
  
  // Battery Voltage (V)
  float vBatt = 25*vBatt_value;
  vBatt = vBatt/1023;
  Serial1.print(String(vBatt));  
  Serial.print(String(vBatt)); 
  Serial1.print(", ");  
  Serial.print(", ");    
  
  // Door sensor
  door_value = digitalRead(door_pin); //= 0 if door closed, = 1 if door open
  Serial1.print(String(door_value));  
  Serial.print(String(door_value)); 
  Serial1.print("\n");    
  Serial.print("\n");     
  
  // Green LED ON when battery voltage more than v_high
  // Red LED ON when battery voltage less than v_low
  // Previous state when battery voltage between v_low and v_high (deadband)
  if (vBatt >= v_high) {
    digitalWrite(green_pin, HIGH);
    digitalWrite(red_pin, LOW);
  } else if (vBatt <= v_low) {
    digitalWrite(green_pin, LOW);
    digitalWrite(red_pin, HIGH);
  }

  delay(logTime - delayTime*avgSamples);
  
}
