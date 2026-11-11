#include <SPI.h>
#include <EthernetENC.h>

const uint8_t CS_PIN = 10;   // change to test another chip select pin
byte mac[] = { 0x02, 0x12, 0x34, 0x56, 0x78, 0x9A };
IPAddress ip(10, 0, 0, 55);

void setup() {
   Serial.begin(115200);
   Ethernet.init(CS_PIN);
   Ethernet.begin(mac, ip);

   Serial.print(F("Hardware: "));
   Serial.println(Ethernet.hardwareStatus() == EthernetNoHardware ? F("NOT FOUND") : F("found"));
   Serial.print(F("IP: "));
   Serial.println(Ethernet.localIP());
   delay(100);
}

void loop() {
   Ethernet.maintain();   // keeps the stack running, so it can answer ping
   delay(10);
}