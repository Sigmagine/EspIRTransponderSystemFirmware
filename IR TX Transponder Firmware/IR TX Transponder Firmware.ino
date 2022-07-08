/*
 Name:		IR_TX_Transponder_Firmware.ino
 Created:	2021-05-10 23:14:38
 Author:	SamuelTranchet
*/



#include <IRsend.h>
#include <IRremoteESP8266.h>
//#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>




#define WIFI_SSID "MiniRC"
#define WIFI_PASS "MiniRC12"

#define UDP_PORT 1212

#define NUM_LEDS 2
#define IR_LED_PIN 2
#define NEOPIXEL_PIN 0

//CRGB leds[NUM_LEDS];
uint16_t transponderCode = 0;
uint32_t transponderCodeNec = 0;
WiFiUDP UDP;
unsigned char packet[255];
IRsend ir(IR_LED_PIN,false,true);

struct IRParams {
    uint16_t delay = 20;
    uint16_t pulsesCount = 0;
    uint16_t pulsesWidths[11] = {
    0,0,0,0,0,0,0,0,0,0,0
    };
};

IRParams irParams;

const uint16_t rawData[] = {
    3000, 1000,
    3000, 1000,
    1000, 3000,
    1000
};

 /*
const uint16_t rawData[] = {
    4000, 2000,
    2000, 1000,
    3000, 2000,
    1000
};*/


// the setup function runs once when you press reset or power the board
void setup() {

    //FastLED.addLeds<NEOPIXEL, NEOPIXEL_PIN>(leds, NUM_LEDS);  // GRB ordering is assumed

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
            transponderCode = (((uint16_t)packet[1]) << 8) | ((uint16_t)packet[0]);
            unsigned char inv0 = ~packet[0];
            unsigned char inv1 = ~packet[1];
            transponderCodeNec = ((uint32_t)packet[1] << 24) | ((uint32_t)inv1 << 16) | ((uint32_t)packet[0] << 8) | (uint32_t)inv0;

            Serial.print("Code received : 0x");
            Serial.print(packet[1], HEX);
            Serial.print(", 0x");
            Serial.print(packet[0], HEX);
            Serial.print(" -> 0x");
            Serial.print(transponderCode, HEX);
            Serial.print(", 0x");
            Serial.print(transponderCodeNec, HEX);
            Serial.println();

            for (unsigned int i = 0; i < 3; i++) {
                //FastLED.showColor(CRGB::White);
                digitalWrite(IR_LED_PIN, HIGH);
                delay(20);
                //FastLED.showColor(CRGB::Black);
                digitalWrite(IR_LED_PIN, LOW);
                delay(100);
            }
        }else if (len == 3) {
            
            Serial.println("Color received");
            //FastLED.showColor(CRGB(packet[0], packet[1], packet[2]));
        }else if (len == sizeof(irParams)) {

            memcpy(&irParams, packet, sizeof(irParams));

            if (irParams.pulsesCount > sizeof(irParams.pulsesWidths) / sizeof(irParams.pulsesWidths[0]))irParams.pulsesCount = sizeof(irParams.pulsesWidths) / sizeof(irParams.pulsesWidths[0]);

            Serial.print("Ir params received - delay : ");
            Serial.print(irParams.delay);
            Serial.print(" pulses : ");
            for (unsigned int i = 0; i < irParams.pulsesCount; i++) {
                Serial.print(irParams.pulsesWidths[i]);
                Serial.print(", ");
            }
            Serial.println();

            for (unsigned int i = 0; i < 3; i++) {
                //FastLED.showColor(CRGB::White);
                digitalWrite(IR_LED_PIN, HIGH);
                delay(20);
                //FastLED.showColor(CRGB::Black);
                digitalWrite(IR_LED_PIN, LOW);
                delay(100);
            }
           
        } else {
            Serial.print("Received ");
            Serial.print(len);
            Serial.println(" bytes packet");
        }
        
    }

    if (irParams.pulsesCount>0) {
        //ir.sendSony38(transponderCode,kSony12Bits,0);
        //delay(80);
        //ir.sendNEC(transponderCodeNec, 32, 0);
        ir.sendRaw(irParams.pulsesWidths, irParams.pulsesCount, 38); // Note the approach used to automatically calculate the size of the array.
        delay(irParams.delay);

    }


    
}
