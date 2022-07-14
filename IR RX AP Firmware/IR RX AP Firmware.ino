/*
 Name:		IR_RX_AP_Firmware.ino
 Created:	5/10/2021 9:02:07 PM
 Author:	nicolas.obre
*/

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <map>


// An IR detector/demodulator is connected to GPIO pin 14(D5 on a NodeMCU
// board).
// Note: GPIO 16 won't work on the ESP8266 as it does not have interrupts.
const size_t nbOfReceivers = 2;
const uint16_t kRecvPins[nbOfReceivers] = {
    14, 2
}; //D5 - D4

const size_t bufferSize = 7;
unsigned long recvBuffers[nbOfReceivers][bufferSize];
bool lastStates[nbOfReceivers] = { LOW };
unsigned long lastMicros[nbOfReceivers] = { 0 };

const char* ssid = "MiniRC";
const char* password = "MiniRC12";
const uint16_t udpPort = 1212;

char buffer[50];

String idToIp[2] = { "", "" };
std::map<String, unsigned int> ipToId;
std::vector<unsigned long>idLastMillis;
unsigned long cooldownMs = 1000;

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

size_t currentIndex = 0;

struct IRParam {
    uint16_t delay;
    uint16_t pulsesCount;
    const uint16_t pulsesWidths[11];
};


const IRParam irParams[3] = {
    {
        17,
        bufferSize,
        {
            3000,   1000,
            3000,   1000,
            1000,   3000,
            1000,   0,
            0,      0,
            0
        }
    },
    {
        17,
        bufferSize,
        {
            4000,   2000,
            2000,   1000,
            3000,   2000,
            1000,   0,
            0,      0,
            0
        }
    },
    {
        17,
        bufferSize,
        {
            1000,   5000,
            1000,   2000,
            2000,   1000,
            2000,   0,
            0,      0,
            0
        }
    }
};
const uint16_t tolerance = 500;


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

            Serial.print("Number of waiting devices : ");
            Serial.println(lastMacs.size());
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
        });

    while (!Serial)  // Wait for the serial connection to be establised.
        delay(50);

    for (size_t i = 0; i < nbOfReceivers; i++) {
        pinMode(kRecvPins[i], INPUT);
    }
}


void loop() {
    if (lastMacs.size() > 0) {
        String cb;
        Serial.print("Remaing device to assign : ");
        Serial.println(lastMacs.size());
        if (deviceIP(lastMacs.back(), cb)) {
            lastMacs.erase(lastMacs.end());
            Serial.println("Ip address :");
            Serial.println(cb); //do something
            unsigned int indexToUse = nextAvailableIndex;
            auto foundId = ipToId.find(cb);
            if (foundId != ipToId.end()) {
                Serial.println("Ip already registered !");
                indexToUse = foundId->second;
            }
            Udp.beginPacket(cb.c_str(), 1212);
            Udp.write((char*)&irParams[indexToUse], sizeof(IRParam));
            Udp.endPacket();

            Serial.print("Device ID ");
            Serial.println(indexToUse);

            if (nextAvailableIndex == indexToUse) {
                idToIp[indexToUse] = cb;
                ipToId.emplace(cb, indexToUse);
                idLastMillis.push_back(0);
                nextAvailableIndex++;
            }
        }
        else {
            delay(100);
        }
        return;
    }

    for (size_t i = 0; i < nbOfReceivers; i++) {
        if (digitalRead(kRecvPins[i]) != lastStates[i]) {
            lastStates[i] = !lastStates[i];
            if (lastMicros[i] != 0) {
                unsigned long currentTime = micros() - lastMicros[i];
                if (currentTime < 500) return; //Skipping this one, too low
                if (currentTime < 4500) {
                    recvBuffers[i][currentIndex] = micros() - lastMicros[i];
                    currentIndex = ++currentIndex % bufferSize;
                    int foundIndex = compareSequence(i);
                    if (foundIndex != -1) {
                        if (millis() - idLastMillis[foundIndex] > 1000) {
                            sprintf(buffer, "[SF%02u]\n", foundIndex + 1);
                            Serial.print(buffer);
                        }
                        idLastMillis[foundIndex] = millis();
                    }
                }
            }
            lastMicros[i] = micros();
        }
    }

    if (Serial.available() > 0) {
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
    }
}
int compareSequence(size_t receiverIndex) {
    bool error = false;
    for (size_t j = 0; j < (sizeof(irParams) / sizeof(*irParams)); j++) {
        for (size_t k = 0; k < irParams[j].pulsesCount; k++) {
            for (size_t i = (currentIndex + k) % irParams[j].pulsesCount, step = 0; step < irParams[j].pulsesCount; step++) {
                if (irParams[j].pulsesWidths[step] < recvBuffers[receiverIndex][i] - tolerance || irParams[j].pulsesWidths[step] > recvBuffers[receiverIndex][i] + tolerance) {
                    error = true;
                    break;
                }
                i = (i + 1) % irParams[j].pulsesCount;
            }
            if (!error) return j; // if we escape from deeper loop without an error that's a match, so returning the index of the match
            error = false; //else trying next sequence
        }
    }
    return -1;
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

int FindIndex(const uint16_t a[], size_t size, uint16_t value)
{
    size_t index = 0;
    while (index < size && a[index] != value) ++index;

    return (index == size ? -1 : index);
}