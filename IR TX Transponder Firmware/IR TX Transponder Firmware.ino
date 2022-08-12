/*
 Name:		IR_TX_Transponder_Firmware.ino
 Created:	2021-05-10 23:14:38
 Author:	SamuelTranchet
*/

#include <IRsend.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <WiFiUdp.h>

#define UDP_PORT 1212
#define HTTP_PORT 80
#define NUM_LEDS 2
#define IR_LED_PIN 2
#define NEOPIXEL_PIN 0

struct IRParams {
    uint32_t color;
    uint16_t delay = 20;
    uint16_t pulsesCount = 0;
    uint16_t pulsesWidths[11] = {
    0,0,0,0,0,0,0,0,0,0,0
    };
};

struct EEPromConfig {
    char ssid[32];
    char pass[96];
};


IRParams irParams;
EEPromConfig config;
uint16_t transponderCode = 0;
uint32_t transponderCodeNec = 0;
WiFiUDP UDP;
unsigned char packet[255];
IRsend ir(IR_LED_PIN, false, true);
String st;
ESP8266WebServer server(HTTP_PORT);


String getUid() {

    char buffer[40];
    sprintf(buffer, "%08X", ESP.getChipId());

    return String(buffer);
}

String getSSID() {

    char buffer[40];
    sprintf(buffer, "%08X", ESP.getChipId());

    return String("IR Transponder ") + getUid();
}


bool connectWiFi(void) {

    WiFi.begin(config.ssid, config.pass);

    Serial.println("Waiting for WiFi to connect");

    for (int c = 0; c < 2000; c++) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Connected");
            Serial.print("Local IP: ");
            Serial.println(WiFi.localIP());
            return true;
        }
        delay(10);
    }

    Serial.println("Connect timed out");

    return false;
}

void scanWiFi() {

    Serial.println("Scanning WiFi networks");

    WiFi.disconnect();
    WiFi.mode(WIFI_STA);

    delay(100);

    int n = WiFi.scanNetworks();
    
    st = "<ol>";

    if (n == 0) {

        Serial.println("No networks found");

    } else {

        Serial.print(n);
        Serial.println(" networks found");

        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
            delay(10);

            // Print SSID and RSSI for each network found
            st += "<li>";
            st += WiFi.SSID(i);
            st += " (";
            st += WiFi.RSSI(i);

            st += ")";
            st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
            st += "</li>";
        }
    }

    Serial.println("");

    st += "</ol>";

    delay(100);
}

void launchWeb() {

    server.on("/", []() {

        IPAddress ip = WiFi.softAPIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
        String content;

        content = "<!DOCTYPE HTML>\r\n<html>";
        content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
        content += "<p>IP : ";
        content += ipStr;
        content += "<br/>UID : ";
        content += getUid();
        content += "</p><p>";
        content += st;
        content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
        content += "</html>";
        server.send(200, "text/html", content);

    });

    server.on("/scan", []() {

        //setupAP();
        IPAddress ip = WiFi.softAPIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

        String content;

        content = "<!DOCTYPE HTML>\r\n<html>go back";
        server.send(200, "text/html", content);

    });

    server.on("/setting", []() {

        String ssid = server.arg("ssid");
        String pass = server.arg("pass");

        String content;
        int statusCode;

        if (ssid.length() > 0) {

            memset(config.ssid, 0, sizeof(config.ssid));
            memset(config.pass, 0, sizeof(config.pass));

            strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid));
            strncpy(config.pass, pass.c_str(), sizeof(config.pass));

            Serial.println("Writing config to EEprom");
            EEPROM.put(0, config);

            EEPROM.commit();

            content = "{\"Success\":\"saved to eeprom... reset to boot into new WiFi\"}";
            statusCode = 200;
            ESP.reset();

        } else {
            content = "{\"Error\":\"404 not found\"}";
            statusCode = 404;
            Serial.println("Sending 404");
        }
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(statusCode, "application/json", content);

    });

    server.begin();
    Serial.println("Server started");
}


void setupAP() {

    scanWiFi();

    Serial.println("Starting acces point");

    
    WiFi.softAP(getSSID().c_str(), "");

    Serial.print("SoftAP IP: ");
    Serial.println(WiFi.softAPIP());

}

void setup(){

    Serial.begin(115200); //Initialising if(DEBUG)Serial Monitor

    EEPROM.begin(512); //Initialasing EEPROM
    
    ir.begin();

    Serial.println();
    Serial.println();
    Serial.println("Startup");
    
    Serial.println("Reading EEPROM config");

    EEPROM.get(0, config);

    config.ssid[sizeof(config.ssid) - 1] = 0;
    config.pass[sizeof(config.pass) - 1] = 0;

    Serial.println("WiFi SSID: ");
    Serial.println(config.ssid);

    Serial.println("WiFi Pass: ");
    Serial.println(config.pass);

    
    if (connectWiFi()){

        UDP.begin(UDP_PORT);

    } else {

        setupAP();
        launchWeb();

        Serial.println();
        Serial.println("Please connect to AP's WiFi and browse home page ...");

        while ((WiFi.status() != WL_CONNECTED)){
            delay(100);
            server.handleClient();
        }

    }

    pinMode(LED_BUILTIN, OUTPUT);

}


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
            } else if (len == 3) {

                Serial.println("Color received");
                //FastLED.showColor(CRGB(packet[0], packet[1], packet[2]));

            } else if (len == sizeof(irParams)) {

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

        if (irParams.pulsesCount > 0) {
            //ir.sendSony38(transponderCode,kSony12Bits,0);
            //delay(80);
            //ir.sendNEC(transponderCodeNec, 32, 0);
            ir.sendRaw(irParams.pulsesWidths, irParams.pulsesCount, 38); // Note the approach used to automatically calculate the size of the array.
            delay(irParams.delay);

        }
}











