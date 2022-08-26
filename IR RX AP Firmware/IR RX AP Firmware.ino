/*
 Name:		IR_RX_AP_Firmware.ino
 Created:	5/10/2021 9:02:07 PM
 Authors:	nicolas.obre, samuel.tranchet
*/

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <map>
#include <Adafruit_NeoPixel.h>


const size_t numLeds = 16;

const char* ssid = "MiniRC";
const char* password = "MiniRC12";
const uint16_t udpPort = 1212;

const unsigned long cooldownMs = 1000;

const size_t bufferSize = 5;

const uint16_t tolerance = 200;


const uint8_t pinLed = 0;
Adafruit_NeoPixel leds = Adafruit_NeoPixel(numLeds, pinLed, NEO_GRB + NEO_KHZ800);
bool ledsChanged = false;

// An IR detector/demodulator is connected to GPIO pin 14(D5 on a NodeMCU
// board).
// Note: GPIO 16 won't work on the ESP8266 as it does not have interrupts.






struct IRParams {
    uint32_t color;
    uint16_t delay;
    uint16_t pulsesCount;
    const uint16_t pulsesWidths[11];
};

struct Racer {


    Racer(IRParams irParams) : irParams(irParams) {


    };

    IRParams irParams;
    unsigned long idLastMillis = 0;
    char strToSend[8] = { 0 };

};


Racer racers[8] = {
    IRParams{
        leds.Color(0, 0, 255),
        10,
        bufferSize,
        {
            1613, 1613, 1613, 1613, 1613, 0, 0, 0, 0, 0, 0
        }
    },
    IRParams{
         leds.Color(0, 255, 255),
        10,
        bufferSize,
        {
            863,1238,1613,1613,863, 0, 0, 0, 0, 0, 0
        }
    },
    IRParams{
        leds.Color(0, 255, 0),
        10,
        bufferSize,
        {
            863,863,1238,863,863, 0, 0, 0, 0, 0, 0
        }
    },
    IRParams{
        leds.Color(255, 255, 0),
        10,
        bufferSize,
        {
            488,1613,488,1613,488, 0, 0, 0, 0, 0, 0
        }
    },
    IRParams{
        leds.Color(255, 127, 0),
        10,
        bufferSize,
        {
            488,1238,1613,488,1238, 0, 0, 0, 0, 0, 0
        }
    },
    IRParams{
        leds.Color(255, 0, 0),
        10,
        bufferSize,
        {
            488,863,1613,1238,488, 0, 0, 0, 0, 0, 0
        }
    },
    IRParams{
        leds.Color(128, 0, 255),
        10,
        bufferSize,
        {
            1238,488,1613,488,1613, 0, 0, 0, 0, 0, 0
        }
    },
    IRParams{
        leds.Color(255, 0, 255),
        10,
        bufferSize,
        {
            488,488,863,488,488, 0, 0, 0, 0, 0, 0
        }
    }

};



struct IRDetector {

    IRDetector(uint8_t pin, uint8_t ledIndex) : pin(pin), ledIndex(ledIndex) {

    }

    Racer* compareSequence() {
        bool error = false;
        for (size_t j = 0; j < (sizeof(racers) / sizeof(Racer)); j++) {
            Racer* racer = &racers[j];
            const IRParams* irParams = &racer->irParams;
            for (size_t k = 0; k < irParams->pulsesCount; k++) {
                for (size_t i = (currentIndex + k) % irParams->pulsesCount, step = 0; step < irParams->pulsesCount; step++) {
                    if (irParams->pulsesWidths[step] < this->buffer[i] - tolerance ||
                        irParams->pulsesWidths[step] > this->buffer[i] + tolerance ||
                        this->states[i] == (step % 2)) {

                        error = true;
                        break;
                    }
                    i = (i + 1) % irParams->pulsesCount;
                }
                if (!error) return racer; // if we escape from deeper loop without an error that's a match, so returning the index of the match
                error = false; //else trying next sequence
            }
        }
        return NULL;
    }

    const uint8_t pin;
    const uint8_t ledIndex;
    size_t currentIndex = 0;
    unsigned long buffer[bufferSize];
    int states[bufferSize];
    int lastState = HIGH;
    unsigned long lastMicros = 0;
    unsigned long lastLedMillis = 0;
    Racer* lastRacer = NULL;
};

IRDetector irDetectors[2] = {
    IRDetector(2,0),  //D4
    IRDetector(14,8) //D5
};





std::vector<String> idToIp;
std::map<String, unsigned int> ipToId;

unsigned int nextAvailableIndex = 0;

String serialReadString;

//Is this really needed ?
WiFiEventHandler clientConnected;
WiFiEventHandler clientDisconnected;

struct macAddress {
    uint8 value[6];
};
std::vector<macAddress> lastMacs;

WiFiUDP Udp;


void setup() {

    Serial.begin(115200);
    Serial.println();

    Serial.print("Setting soft-AP ... ");
    Serial.println(WiFi.softAP(ssid, password) ? "Ready" : "Failed!");

    clientConnected = WiFi.onSoftAPModeStationConnected([](const WiFiEventSoftAPModeStationConnected& event){
            Serial.println("Station connected");
            char lastMac[18];
            sprintf(lastMac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(event.mac));
            Serial.printf("Mac adress: %s\n", lastMac);
            macAddress currentMacAddress;
            memcpy(currentMacAddress.value, event.mac, 6);
            bool found = false;
            for (size_t i = 0; i < lastMacs.size(); i++) {
                if (std::equal(std::begin(lastMacs[i].value), std::end(lastMacs[i].value), std::begin(event.mac))) {
                    found = true;
                    break;
                }
            }
            if (!found)
                lastMacs.push_back(currentMacAddress);
            leds.fill(leds.Color(255, 0, 0), 0, numLeds);
            leds.show();
            

            for (size_t i = 0; i < sizeof(irDetectors) / sizeof(IRDetector); i++) {
                irDetectors[i].lastLedMillis = 0;
            }

        });

    clientDisconnected = WiFi.onSoftAPModeStationDisconnected([](const WiFiEventSoftAPModeStationDisconnected& event) {
            Serial.println("Station disconnected");
            char lastMac[18];
            sprintf(lastMac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(event.mac));
            Serial.printf("Mac adress: %s\n", lastMac);
            macAddress currentMacAddress;
            memcpy(currentMacAddress.value, event.mac, 6);
            for (size_t i = 0; i < lastMacs.size(); i++) {
                if (std::equal(std::begin(lastMacs[i].value), std::end(lastMacs[i].value), std::begin(event.mac))) {
                    lastMacs.erase(lastMacs.begin() + i);
                    break;
                }
            }
            if (lastMacs.size() == 0) {
                leds.clear();
                leds.show();
                delay(250);
                leds.fill(leds.Color(255, 128, 0), 0, numLeds);
                leds.show();
                delay(250);
                leds.clear();
                leds.show();
                delay(250);
                leds.fill(leds.Color(255, 128, 0), 0, numLeds);
                leds.show();
                delay(250);
                leds.clear();
                leds.show();
                delay(250);
            }
        });

    while (!Serial) delay(50);  // Wait for the serial connection to be establised.
        


    leds.begin();
    leds.show();
    leds.setBrightness(255);

    unsigned long us = micros();

    for (size_t i = 0; i < sizeof(irDetectors)/sizeof(IRDetector); i++) {
        irDetectors[i].lastMicros = us;
        pinMode(irDetectors[i].pin, INPUT);
    }

    for (size_t j = 0; j < (sizeof(racers) / sizeof(Racer)); j++) {
        Racer* racer = &racers[j];
        sprintf(racer->strToSend, "[SF%02u]\n", j + 1);
    }
}


void loop() {

    
    unsigned long ms = millis();

    


    if (lastMacs.size() > 0) {
        String cb;
        Serial.print("Remaing device to assign : ");
        Serial.println(lastMacs.size());
        if (deviceIP(lastMacs.back(), cb)) {
            lastMacs.erase(lastMacs.end());
            if (lastMacs.size() == 0) {
                leds.fill(leds.Color(255, 255, 255), 0, numLeds);
                ledsChanged = true;
            }
            Serial.println("Ip address :");
            Serial.println(cb); //do something
            unsigned int indexToUse = nextAvailableIndex;
            auto foundId = ipToId.find(cb);
            if (foundId != ipToId.end()) {
                Serial.println("Ip already registered !");
                indexToUse = foundId->second;
            }
            Udp.beginPacket(cb.c_str(), 1212);
            Udp.write((char*)&racers[indexToUse].irParams, sizeof(IRParams));
            Udp.endPacket();

            Serial.print("Device ID ");
            Serial.println(indexToUse);

            if (nextAvailableIndex == indexToUse) {
                idToIp.push_back(cb);
                ipToId.emplace(cb, indexToUse);
                nextAvailableIndex++;
            }

        } else {
            delay(100);
        }

        return;
    }

    

    for (size_t i = 0; i < sizeof(irDetectors) / sizeof(IRDetector); i++) {

        IRDetector* irDetector = &irDetectors[i];

        if (irDetector->lastLedMillis != 0 && ms > irDetector->lastLedMillis) {
            leds.fill(leds.Color(0, 0, 0), irDetector->ledIndex, 8);
            ledsChanged = true;
            irDetector->lastLedMillis = 0;
            irDetector->lastRacer = NULL;
        }

        bool state = digitalRead(irDetector->pin);
        unsigned long us = micros();

        if (state != irDetector->lastState) {

            unsigned long currentTime = us - irDetector->lastMicros;

           /* if (currentTime < tolerance) {
                //Ignoring this value
                irDetector->lastMicros = us - currentTime;
                continue; //Skipping this one, too low
            }*/

            irDetector->lastState = state;

            if (currentTime < 2500) {

                irDetector->buffer[irDetector->currentIndex] = currentTime;
                irDetector->states[irDetector->currentIndex] = state;
                irDetector->currentIndex = ++irDetector->currentIndex % bufferSize;

                Racer* racer = irDetector->compareSequence();

                if (racer != NULL) {

                    if (ms > irDetector->lastLedMillis || irDetector->lastRacer!=racer){
                        irDetector->lastRacer = racer;
                        leds.fill(racer->irParams.color, irDetector->ledIndex, 8);
                        ledsChanged = true;
                    }
                    irDetector->lastLedMillis = ms + cooldownMs;
                    
                    if (ms > racer->idLastMillis) {
                        Serial.print(racer->strToSend);
                    }
                    racer->idLastMillis = ms + cooldownMs; //the detected emmiter must leave the area
                }
            }

            irDetector->lastMicros = us;
        }
    }


    if (ledsChanged) {
        leds.show();
        ledsChanged = false;
    }//avoid sending full data several times and do it after all processing is done

   /* for (size_t i = 0; i < nbOfReceivers; i++) {

        int state = digitalRead(kRecvPins[i]);
        unsigned long us = micros();

        if (state != lastStates[i]) {

            unsigned long currentTime = us - lastMicros[i];

            if (currentTime > 4000) {
                recvIndexes[i] = 0;

            }

            if (currentTime < tolerance) {
                //Ignoring this value
                lastMicros[i] = us - currentTime;
                continue; //Skipping this one, too low
            }

            lastStates[i] = state;

            if (currentTime < 2500) {

                recvBuffers[i][currentIndex] = currentTime;
                recvValues[i][currentIndex] = state;
                currentIndex = ++currentIndex % bufferSize;

                int foundIndex = compareSequence(i);

                if (foundIndex != -1) {
                    if (ms - idLastMillis[foundIndex] > cooldownMs) {

                        leds.fill(irParams[foundIndex].color, 0, numLeds);
                        leds.show();
                        lastLedMillis = ms;

                        Serial.print(sf[foundIndex]);
                    }
                    idLastMillis[foundIndex] = ms;
                }
            }

            lastMicros[i] = us;
        }
    }*/

    /*if (Serial.available() > 0) {
        serialReadString = Serial.readStringUntil(']');
        if (serialReadString.charAt(1) == 'C' && serialReadString.charAt(2) == 'F') {
            int carNumber = serialReadString.substring(3, 5).toInt();
            Serial.print("Get pos for car ");
            Serial.println(carNumber);
            int curPos = serialReadString.substring(11, 13).toInt();
            Serial.print("Pos : ");
            Serial.println(curPos);
            String ip = idToIp[carNumber - 1];
            Serial.print("Ip : ");
            Serial.println(ip);

            if (ip != "") {
                switch (curPos) {
                case 1:
                    Serial.print("Sending Green to");
                    Serial.println(ip);
                    sendColor(ip, 0, 255, 0);
                    break;
                case 2:
                    Serial.print("Sending Orange to");
                    Serial.println(ip);
                    sendColor(ip, 255, 69, 0);
                    break;
                case 3:
                    Serial.print("Sending Red to");
                    Serial.println(ip);
                    sendColor(ip, 255, 0, 0);
                    break;
                default:
                    Serial.print("Sending shutdown to");
                    Serial.println(ip);
                    sendColor(ip, 0, 0, 0);
                    break;
                }
            }
        }
    }*/
}


void sendColor(String ip, int r, int g, int b) {
    uint8_t color[3];
    color[0] = r;
    color[1] = g;
    color[2] = b;
    Udp.beginPacket(ip.c_str(), 1212);
    Udp.write(color, 3);
    Udp.endPacket();
}

boolean deviceIP(macAddress mac_address, String& cb) {

    struct station_info* station_list = wifi_softap_get_station_info();
    
    while (station_list != NULL) {
        char station_mac[18] = { 0 }; sprintf(station_mac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(station_list->bssid));
        String station_ip = IPAddress((&station_list->ip)->addr).toString();
        if (std::equal(std::begin(mac_address.value), std::end(mac_address.value), std::begin(station_list->bssid))) {
            cb = station_ip;
            return true;
        }

        station_list = STAILQ_NEXT(station_list, next);
    }

    wifi_softap_free_station_info();
    cb = "DHCP not ready or bad MAC address";
    return false;
}

/*int FindIndex(const uint16_t a[], size_t size, uint16_t value)
{
    size_t index = 0;
    while (index < size && a[index] != value) ++index;

    return (index == size ? -1 : index);
}*/