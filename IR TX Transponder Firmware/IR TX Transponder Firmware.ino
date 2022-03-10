/*
 Name:		IR_TX_Transponder_Firmware.ino
 Created:	2021-05-10 23:14:38
 Author:	SamuelTranchet
*/



#include <IRsend.h>
#include <IRremoteESP8266.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>




#define WIFI_SSID "MiniRC"
#define WIFI_PASS "MiniRC12"

#define UDP_PORT 1212

#define NUM_LEDS 2
#define IR_LED_PIN 2
#define NEOPIXEL_PIN 0

CRGB leds[NUM_LEDS];
uint16_t transponderCode = 0;
WiFiUDP UDP;
char packet[255];
IRsend ir(IR_LED_PIN);

// the setup function runs once when you press reset or power the board
void setup() {

    FastLED.addLeds<NEOPIXEL, NEOPIXEL_PIN>(leds, NUM_LEDS);  // GRB ordering is assumed

    Serial.begin(115200);
    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    UDP.begin(UDP_PORT);

    ir.begin();
    
}

// the loop function runs over and over again until power down or reset
void loop() {

    int packetSize = UDP.parsePacket();
    if (packetSize) {
        
        int len = UDP.read(packet, 255);
        if (len == 2) {
            transponderCode = (((uint16_t)packet[1]) << 8) + ((uint16_t)packet[0]);
            Serial.print("Code received : 0x");
            Serial.print(packet[0], HEX);
            Serial.print(", 0x");
            Serial.print(packet[1], HEX);
            Serial.print(" -> 0x");
            Serial.print(transponderCode, HEX);
            Serial.println();

            for (unsigned int i = 0; i < 3; i++) {
                FastLED.showColor(CRGB::White);
                digitalWrite(IR_LED_PIN, HIGH);
                delay(20);
                FastLED.showColor(CRGB::Black);
                digitalWrite(IR_LED_PIN, LOW);
                delay(100);
            }
        }if (len == 3) {
            
            Serial.println("Color received");
            FastLED.showColor(CRGB(packet[0], packet[1], packet[2]));
        }
        
    }

    if (transponderCode) {
        ir.sendSony38(transponderCode,kSony12Bits,0);
        delay(40);
        //ir.sendNEC(0x00FF4AB5, 32, 0);
    }


    
}
