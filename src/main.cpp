#include <Arduino.h>

// Web libaries
#include "secrets.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "webserver.h"

// Sensor libraries
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>

// SD Card
#include "FS.h"
#include <SD.h>
#include <SPI.h>


// Definitions for Sensors
#define SDA_PIN 8
#define SCL_PIN 9

Adafruit_BMP280 bmp;
Adafruit_AHTX0 aht;

float tempOffset = 0.0;
float humScale   = 1.0;
float humOffset  = 0.0;

float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;


// WiFi credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

AsyncWebServer server(80);

// SD Card
#define SD_CS 10
#define SD_SCK 12
#define SD_MISO 13
#define SD_MOSI 11

// Weather API
String weatherDescription = "Lädt...";
float weatherTemp = 0.0;
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 600000; // 10 Minuten

// Wetterdaten vom ESP32 abrufen
void getWeatherData() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        
        String url = "http://api.openweathermap.org/data/2.5/weather?q=Mannheim&appid=";
        url += WEATHER_API_KEY;
        url += "&units=metric&lang=de";
        
        Serial.println("Hole Wetterdaten...");
        http.begin(url);
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            String payload = http.getString();
            
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                weatherTemp = doc["main"]["temp"];
                const char* description = doc["weather"][0]["description"];
                weatherDescription = String((int)round(weatherTemp)) + "°C, " + String(description);
                
                Serial.print("Wetter aktualisiert: ");
                Serial.println(weatherDescription);
            } else {
                Serial.println("JSON Parse Fehler");
                weatherDescription = "Fehler beim Parsen";
            }
        } else {
            Serial.print("HTTP Fehler: ");
            Serial.println(httpCode);
            weatherDescription = "Nicht verfügbar";
        }
        
        http.end();
    } else {
        weatherDescription = "Keine WiFi-Verbindung";
    }
}

void logToSD() {
    if (!SD.begin(SD_CS)) {
        Serial.println("SD-Karte konnte nicht initialisiert werden!");
        return;
    }

    File logFile = SD.open("/sensor_log.csv", FILE_APPEND);
    if (logFile) {
        String dataString = String(millis()) + ";" + String(temperature) + ";" + String(humidity) + ";" + String(pressure) + "\n";
        logFile.print(dataString);
        logFile.close();
        Serial.println("Daten in SD-Karte geloggt");
    } 
    else {
        Serial.println("Fehler beim Öffnen der Log-Datei!");
    }
}

void getSensorData() {
    sensors_event_t humEvent, tempEvent;
    aht.getEvent(&humEvent, &tempEvent);
    
    temperature = tempEvent.temperature + tempOffset;
    humidity = humEvent.relative_humidity * humScale + humOffset;
    pressure = bmp.readPressure() / 100.0F;

    Serial.print("Temp: "); Serial.print(temperature);
    Serial.print("C | Hum: "); Serial.print(humidity);
    Serial.print("% | Pressure: "); Serial.print(pressure);
    Serial.println(" hPa");

    logToSD();
}

void setupSD() {
    Serial.println("Initialising SD Card...");
    
    // Open SPI bus
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("Couldn't mount SD Card!");
        return;
    }
    
    Serial.println("SD Card Ready!");
    
    // 2. Check data log file
    String fileName = "/sensor_log.csv";
    if (SD.exists(fileName.c_str())) {
        Serial.println("Log-file already exists.");
    } else {
        File logFile = SD.open(fileName.c_str(), FILE_WRITE);
        if (logFile) {
            logFile.println("Timestamp;Temperature;Humidity;Pressure");
            logFile.close();
            Serial.println("Created log-file.");
        } else {
            Serial.println("Error creating log-file!");
        }
    }
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
    Serial.println("BMP280 initialisiert!");

    // AHT20 initialisieren
    if (!aht.begin()) {
        Serial.println("AHT20 nicht gefunden!");
        while (1) delay(10);
    }
    Serial.println("AHT20 initialisiert!");

    // WiFi Verbindung
    Serial.println("Verbinde mit WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi verbunden!");
    Serial.print("IP-Adresse: ");
    Serial.println(WiFi.localIP());

    // SD-Karte initialisieren
    setupSD();

    // Erste Wetterdaten abrufen
    getWeatherData();

    // Route für die Hauptseite - INLINE HTML
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", index_html);
    });

    // API-Endpunkt für Sensordaten
    server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
        getSensorData();
        
        // Wetter aktualisieren wenn nötig
        if (millis() - lastWeatherUpdate > weatherUpdateInterval) {
            getWeatherData();
            lastWeatherUpdate = millis();
        }
        
        JsonDocument doc;
        doc["temperature"] = temperature;
        doc["humidity"] = humidity;
        doc["pressure"] = pressure;
        doc["weather"] = weatherDescription;
        doc["timestamp"] = millis();
        
        String response;
        serializeJson(doc, response);
        
        request->send(200, "application/json", response);
    });
    
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Nicht gefunden");
    });
    
    server.on("/sd-data", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!SD.cardType()) {
            request->send(500, "application/json", "{\"error\":\"SD nicht verfügbar\"}");
            return;
        }

        // Letzte 100 Zeilen lesen (≈48h bei 30s Intervall)
        String csvData = "";
        File file = SD.open("/sensor_log.csv", FILE_READ);
        int lineCount = 0;

        if (file) {
            while (file.available() && lineCount < 100) {
                String line = file.readStringUntil('\n');
                if (line.startsWith("Timestamp") || line.length() < 10) continue;
                csvData += line + "\n";
                lineCount++;
            }
            file.close();
        }

        JsonDocument doc;
        doc["status"] = "ok";
        doc["lines"] = lineCount;
        doc["data"] = csvData;  // Roh-CSV für Chart

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.begin();
    Serial.println("HTTP-Server gestartet");
    Serial.println("Oeffne im Browser: http://" + WiFi.localIP().toString());
}

void loop() {
    delay(10);
}
