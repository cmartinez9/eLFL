#include <ArduinoJson.h>

// rf95_client.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messageing client
// with the RH_RF95 class. RH_RF95 class does not provide for addressing or
// reliability, so you should only use RH_RF95 if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example rf95_server
// Tested with Anarduino MiniWirelessLoRa, Rocket Scream Mini Ultra Pro with
// the RFM95W, Adafruit Feather M0 with RFM95

#include <SPI.h>
#include <RH_RF95.h>


// 0 => BW = 125, Cr = 4/5, Sf = 128, CRC On, Medium Range, Default
// 1 => BW = 500, Cr = 4/5, Sf = 128, CRC On, Fast + Short Range
// 2 => BW = 32.25, Cr = 4/8, Sf = 512, CRC On, Slow + Long Range
// 3 => BW = 125, Cr = 4/8, Sf = 4096, CRC On, Low Data rate, CRC On, Slow + Long Range
// 4 => BW = 125, Cr = 4/5 Sf = 2048, CRC On, Slow + Long Range
#define MODEM_CONFIG 2

// +2 to +20, 13 default
#define TX_POWER 23
#define SF       10
#define BW       125000
#define CR       7

// Node Name
#define NODE 1

#define EN 4

RH_RF95 rf95(10, 2); 

void setup() 
{
  // Set pin mode for enable 
  pinMode(EN, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(EN, HIGH);
  Serial.begin(9600);
  while (!Serial) ; // Wait for serial port to be available
  if (!rf95.init())
    Serial.println("init failed");
   // pinMode(3, OUTPUT);
    
  rf95.waitPacketSent();
// Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
  
  rf95.setModemConfig(MODEM_CONFIG); // set new modem configuration
  rf95.setFrequency(915.0);
  rf95.setTxPower(TX_POWER,false); // set new transmit power configuration
  rf95.setSpreadingFactor(SF); // set new spreading factor 2^10
  rf95.setSignalBandwidth(BW); // set new bandwidth
  rf95.setCodingRate4(CR); // set new coding rate
  rf95.setPayloadCRC(true);
    

  
  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  //  driver.setTxPower(23, false);

}

void loop()
{
  while(!rf95.waitCAD());
  StaticJsonDocument<256> doc;
  Serial.println("Sending to rf95_server");
  // Send a message to rf95_server
  //digitalWrite(3, LOW);
  doc["id"] = NODE;
  doc["voltage"] = 5;
  doc["current"] = 1;
  char packet[220];
  serializeJson(doc, packet);
  //uint8_t data[sizeof(packet)] = packet;
  digitalWrite(5, HIGH);
  rf95.send(packet, sizeof(packet));

  rf95.waitPacketSent();
  Serial.println("Packet Sent");
  digitalWrite(5, LOW);
  //Serial.println(sizeof(data));
  //digitalWrite(3, HIGH);

  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  if (rf95.waitAvailableTimeout(2000))
  //  if (rf95.waitAvailableTimeout(1000))
  { 
    // Should be a reply message for us now   
    if (rf95.recv(buf, &len))
    {
      Serial.print("got reply: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);    
    }
    else
    {
      Serial.println("recv failed");
    }
    delay(1000);
  }
  else
  {
    Serial.println("No reply, is rf95_server running?");
  }

 
  delay(4000);
}
