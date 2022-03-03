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


// An IR detector/demodulator is connected to GPIO pin 14(D5 on a NodeMCU
// board).
// Note: GPIO 16 won't work on the ESP8266 as it does not have interrupts.
const uint16_t kRecv1Pin = 14; //D5
const char* ssid = "MiniRC";

const char* password = "MiniRC12";
const uint16_t udpPort = 1212;
const uint16_t availableIds[] = { 0x06FE, 0x03A4, 0x1212, 0xACAB };
int foundIndex = -1;
char buffer[50];

String idToIp[2];
unsigned int nextAvailableIndex = 0;

String serialReadString;

WiFiEventHandler clientConnected;
bool waitingDHCP = false;
char lastMac[18];
WiFiUDP Udp;

IRrecv irrecv1(kRecv1Pin);
decode_results results;


void setup() {
    Serial.begin(115200);
    Serial.println();

    Serial.print("Setting soft-AP ... ");
    Serial.println(WiFi.softAP(ssid, password) ? "Ready" : "Failed!");

    clientConnected = WiFi.onSoftAPModeStationConnected([](const WiFiEventSoftAPModeStationConnected& event)
        {
            Serial.println("Station connected");
            sprintf(lastMac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(event.mac));
            Serial.printf("Mac adress: %s\n", lastMac);
            waitingDHCP = true;
        });

    while (!Serial)  // Wait for the serial connection to be establised.
        delay(50);

    irrecv1.enableIRIn();  // Start the receiver1
}


void loop() {
    if (waitingDHCP) {
        String cb;
        if (deviceIP(lastMac, cb)) {
            Serial.println("Ip address :");
            Serial.println(cb); //do something
            Udp.beginPacket(cb.c_str(), 1212);
            uint8_t id[2];
            id[0] = (availableIds[nextAvailableIndex] & 0xFF);
            id[1] = (availableIds[nextAvailableIndex] >> 8) & 0xFF;
            Udp.write(id, 2);
            Udp.endPacket();
            idToIp[nextAvailableIndex] = cb;
            Serial.println("Device ID");
            Serial.print(id[0], HEX);
            Serial.print(id[1], HEX);
            nextAvailableIndex++;
        }
    }
    if (irrecv1.decode(&results)) {
        Serial.println("Got signal");
        foundIndex = FindIndex(availableIds, 4, results.value) ;
        if (foundIndex != -1) {
            sprintf(buffer, "[SF%02u]\n", foundIndex + 1);
            Serial.print(buffer);
        }
        foundIndex = -1;
        irrecv1.resume();  // Receive the next value
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
    //delay(10);
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

boolean deviceIP(char* mac_device, String& cb) {

    struct station_info* station_list = wifi_softap_get_station_info();

    while (station_list != NULL) {
        char station_mac[18] = { 0 }; sprintf(station_mac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(station_list->bssid));
        String station_ip = IPAddress((&station_list->ip)->addr).toString();

        if (strcmp(mac_device, station_mac) == 0) {
            waitingDHCP = false;
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