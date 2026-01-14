#include <Arduino.h>
#include "secrets.h"
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

// ============================================
// INLINE HTML - Im Flash gespeichert
// ============================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Sensor Dashboard</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #041978041978 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: rgba(255, 255, 255, 0.95);
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.1);
            max-width: 800px;
            width: 100%;
        }
        h1 { color: #333; text-align: center; margin-bottom: 10px; font-size: 2em; }
        .status { text-align: center; margin-bottom: 30px; color: #666; font-size: 0.9em; }
        .status-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background-color: #4CAF50;
            margin-right: 5px;
            animation: pulse 2s infinite;
        }
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
        .sensor-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }
        .sensor-card {
            background: linear-gradient(135deg, #041978 0%, #764ba2 100%);
            border-radius: 15px;
            padding: 25px;
            color: white;
            box-shadow: 0 4px 15px rgba(243, 242, 242, 0.1);
            transition: transform 0.3s ease;
        }
        .sensor-card:hover { transform: translateY(-5px); }
        .sensor-icon { font-size: 2.5em; margin-bottom: 10px; }
        .sensor-name { font-size: 0.9em; opacity: 0.9; margin-bottom: 5px; }
        .sensor-value { font-size: 2.2em; font-weight: bold; margin: 10px 0; }
        .sensor-unit { font-size: 0.8em; opacity: 0.8; }
        .last-update { text-align: center; margin-top: 20px; color: #666; font-size: 0.85em; }
        @media (max-width: 600px) { .sensor-grid { grid-template-columns: 1fr; } }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Sensor Dashboard</h1>
        <div class="status">
            <span class="status-indicator"></span>
            <span id="status-text">Verbunden</span>
        </div>
        <div class="sensor-grid">
            <div class="sensor-card">
                <div class="sensor-name">Temperatur</div>
                <div class="sensor-value" id="temperature">--</div>
                <div class="sensor-unit">C</div>
            </div>
            <div class="sensor-card">
                <div class="sensor-name">Luftfeuchtigkeit</div>
                <div class="sensor-value" id="humidity">--</div>
                <div class="sensor-unit">%</div>
            </div>
            <div class="sensor-card">
                <div class="sensor-name">Luftdruck</div>
                <div class="sensor-value" id="pressure">--</div>
                <div class="sensor-unit">hPa</div>
            </div>
        </div>
        <div class="last-update">
            Letzte Aktualisierung: <span id="last-update">--</span>
        </div>
    </div>
    <script>
        function updateSensorData() {
            fetch('/sensors')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('temperature').textContent = data.temperature.toFixed(1);
                    document.getElementById('humidity').textContent = data.humidity.toFixed(1);
                    document.getElementById('pressure').textContent = data.pressure.toFixed(1);
                    const now = new Date();
                    document.getElementById('last-update').textContent = now.toLocaleTimeString('de-DE');
                    document.getElementById('status-text').textContent = 'Verbunden';
                })
                .catch(error => {
                    console.error('Fehler:', error);
                    document.getElementById('status-text').textContent = 'Verbindungsfehler';
                });
        }
        updateSensorData();
        setInterval(updateSensorData, 5000);
    </script>
</body>
</html>
)rawliteral";

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

    // Route für die Hauptseite - INLINE HTML
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", index_html);
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
    Serial.println("HTTP-Server gestartet");
    Serial.println("Oeffne im Browser: http://" + WiFi.localIP().toString());
}

void loop() {
    delay(10);
}
