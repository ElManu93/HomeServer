#include <Arduino.h>

// Web libaries
#include "secrets.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "webserver.h"
#include <time.h>

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

// Defintions for second sensor
float picoTemperature = 0.0;
float picoHumidity = 0.0;
float picoPressure = 0.0;

char response[256];
unsigned long lastMeasurement = 0;
const unsigned long measurementInterval = 60000; // 60 Sekunden

// WiFi credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

AsyncWebServer server(80);

// SD Card
#define SD_CS 10
#define SD_SCK 12
#define SD_MISO 13
#define SD_MOSI 11
char dataString[128];

char csvData[12000] = "";
char line[128];

char csvResponse[12000];

// Weather API
char weatherDescription[64] = "Loading...";
float weatherTemp = 0.0;
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 6000000; // 100 Minuten

// Time
char currentTime[32] = "Loading...";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 0;

// Wetterdaten vom ESP32 abrufen
void getWeatherData() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        
        char url[256];
        snprintf(
            url,
            sizeof(url),
            "http://api.openweathermap.org/data/2.5/weather?q=Mannheim&appid=%s&units=metric&lang=de",
            WEATHER_API_KEY
        );
        
        Serial.println("Hole Wetterdaten...");
        http.begin(url);
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            WiFiClient *stream = http.getStreamPtr();
            
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, *stream);
            
            if (!error) {
                weatherTemp = doc["main"]["temp"];
                const char* description = doc["weather"][0]["description"];
                snprintf(
                    weatherDescription,
                    sizeof(weatherDescription),
                    "%dC, %s",
                    (int)round(weatherTemp),
                    description
                );
                
                Serial.print("Wetter aktualisiert: ");
                Serial.println(weatherDescription);
            } else {
                Serial.println("JSON Parse Fehler");
                strncpy(weatherDescription, "Fehler beim Parsen", sizeof(weatherDescription));
            }
        } else {
            Serial.print("HTTP Fehler: ");
            Serial.println(httpCode);
            strncpy(weatherDescription, "Nicht verfuegbar", sizeof(weatherDescription));
        }
        
        http.end();
    } else {
        strncpy(weatherDescription, "Keine WiFi-Verbindung", sizeof(weatherDescription));
    }
}

void getDateTime() {
    struct tm timeinfo;
    
    if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
    }

    char formattedTime[40];  // Buffer to store the formatted string
    strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    strncpy(currentTime, formattedTime, sizeof(currentTime));
}

void logToSD() {
    File logFile = SD.open("/sensor_log.csv", FILE_APPEND);

    if (logFile) {
        snprintf(
            dataString,
            sizeof(dataString),
            "%s;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f\n",
            currentTime,
            temperature,
            humidity,
            pressure,
            picoTemperature,
            picoHumidity,
            picoPressure
        );

        logFile.print(dataString);
        logFile.close();
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

    getDateTime();

    Serial.print("Time: "); Serial.print(currentTime);
    Serial.print(" | Temp: "); Serial.print(temperature);
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
    const char* fileName = "/sensor_log.csv";

    if (SD.exists(fileName)) {
        Serial.println("Log-file already exists.");
    } else {
        File logFile = SD.open(fileName, FILE_WRITE);
        if (logFile) {
            logFile.println("Timestamp;Temperature;Humidity;Pressure;PicoTemperature;PicoHumidity;PicoPressure");
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
    
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    
    Serial.println("\nWiFi verbunden!");
    Serial.print("IP-Adresse: ");
    Serial.println(WiFi.localIP());

    // Zeit des Starts holen
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    getDateTime();

    // SD-Karte initialisieren
    setupSD();

    // Erste Wetterdaten abrufen
    getWeatherData();

    // Erste Sesordaten abrufen
    getSensorData();

    // Route für die Hauptseite - INLINE HTML
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", index_html);
    });

    // API-Endpunkt für Sensordaten
    server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
        //getSensorData();
        
        // Wetter aktualisieren wenn nötig
        if (millis() - lastWeatherUpdate > weatherUpdateInterval) {
            getWeatherData();
            lastWeatherUpdate = millis();
        }
        
        JsonDocument doc;
        doc["temperature"] = temperature;
        doc["humidity"] = humidity;
        doc["pressure"] = pressure;
        doc["pico_temperature"] = picoTemperature;
        doc["pico_humidity"] = picoHumidity;
        doc["pico_pressure"] = picoPressure;
        doc["weather"] = weatherDescription;
        doc["timestamp"] = currentTime;
        
        serializeJson(doc, response);
        
        request->send(200, "application/json", response);
    });
    
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Nicht gefunden");
    });
    
    server.on("/sd-data", HTTP_GET, [](AsyncWebServerRequest *request) {
        csvData[0] = '\0';   // Buffer reset
        File file = SD.open("/sensor_log.csv", FILE_READ);

        if (!file) {
            request->send(500, "application/json", "{\"error\":\"Log-Datei nicht lesbar\"}");
            return;
        }

        // Letzte 100 Zeilen lesen
        int lineCount = 0;

        while (file.available() && lineCount < 100) {

            size_t len = file.readBytesUntil('\n', line, sizeof(line)-1);
            line[len] = '\0';

            if (strncmp(line, "Timestamp", 9) == 0 || strlen(line) < 10) continue;

            strncat(csvData, line, sizeof(csvData) - strlen(csvData) - 2);
            strncat(csvData, "\n", sizeof(csvData) - strlen(csvData) - 1);

            lineCount++;
        }

        JsonDocument doc;
        doc["status"] = "ok";
        doc["lines"] = lineCount;
        doc["data"] = csvData;

        serializeJson(doc, csvResponse);
        request->send(200, "application/json", csvResponse);
    });

    // Pico W Daten empfangen (HTTP POST JSON)
    server.on("/api/pico", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)   {
            // JSON-String bauen
            Serial.print("Pico POST empfangen: ");
            Serial.write(data, len);
            Serial.println();

            // ArduinoJson parsen
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (!error) {
                picoTemperature = doc["temperature"] | 0.0;
                picoHumidity = doc["humidity"] | 0.0;
                picoPressure = doc["pressure"] | 0.0;

                Serial.printf("Pico: T=%.1f°C H=%.1f%% P=%.1f hPa\n", 
                             picoTemperature, picoHumidity, picoPressure);
            } else {
                Serial.println("JSON Parse Fehler vom Pico");
            }
        }
    );

    server.begin();
    Serial.println("HTTP-Server gestartet");
}

void loop() {

    if (millis() - lastMeasurement > measurementInterval) {
        lastMeasurement = millis();
        getSensorData();
    }

}
