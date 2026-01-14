#include <Arduino.h>
#include "secrets.h"
#include <SPIFFS.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define SDA_PIN 8
#define SCL_PIN 9

Adafruit_BMP280 bmp;
Adafruit_AHTX0 aht;

float tempOffset = 0.0;
float humScale   = 1.0;
float humOffset  = 0.0;

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

AsyncWebServer server(80);

float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;

void getSensorData() {
    sensors_event_t humEvent, tempEvent;
    aht.getEvent(&humEvent, &tempEvent);
    
    temperature = tempEvent.temperature + tempOffset;
    humidity = humEvent.relative_humidity * humScale + humOffset;
    pressure = bmp.readPressure() / 100.0F;

    Serial.print("Temp: "); Serial.print(temperature);
    Serial.print("°C | Hum: "); Serial.print(humidity);
    Serial.print("% | Pressure: "); Serial.print(pressure);
    Serial.println(" hPa");
}

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    delay(1000);

    // BMP280 initialisieren
    if (!bmp.begin(0x76)) {
        if (!bmp.begin(0x77)) {
            Serial.println("BMP280 nicht gefunden!");
            while (1) delay(10);
        }
    }
    Serial.println("✓ BMP280 initialisiert!");

    // AHT20 initialisieren
    if (!aht.begin()) {
        Serial.println("AHT20 nicht gefunden!");
        while (1) delay(10);
    }
    Serial.println("✓ AHT20 initialisiert!");

    // SPIFFS initialisieren
    if(!SPIFFS.begin(true)){
        Serial.println("SPIFFS Mount fehlgeschlagen!");
        return;
    }
    Serial.println("✓ SPIFFS gemountet");

    // WiFi Verbindung
    Serial.println("Verbinde mit WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\n✓ WiFi verbunden!");
    Serial.print("IP-Adresse: ");
    Serial.println(WiFi.localIP());

    // Route für die Hauptseite
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });

    // API-Endpunkt für Sensordaten
    server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
        getSensorData();
        
        JsonDocument doc;
        doc["temperature"] = temperature;
        doc["humidity"] = humidity;
        doc["pressure"] = pressure;
        doc["timestamp"] = millis();
        
        String response;
        serializeJson(doc, response);
        
        request->send(200, "application/json", response);
    });
    
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Nicht gefunden");
    });
    
    server.begin();
    Serial.println("✓ HTTP-Server gestartet");
    Serial.println("Öffne im Browser: http://" + WiFi.localIP().toString());
}

void loop() {
    delay(10);
}
