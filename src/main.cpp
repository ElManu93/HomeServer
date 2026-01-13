#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>

// I2C Pins
#define SDA_PIN 8
#define SCL_PIN 9

// Sensor Objekte
Adafruit_BMP280 bmp; // BMP280
Adafruit_AHTX0 aht;  // AHT20

// Optionale Korrekturen
float tempOffset = 0.0;      // °C
float humScale   = 1.0;      // Multiplikator für Luftfeuchtigkeit
float humOffset  = 0.0;      // %

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(1000);

  // BMP280 starten
  if (!bmp.begin(0x77)) {
    Serial.println("BMP280 nicht gefunden!");
    while (1);
  }

  // AHT20 starten
  if (!aht.begin()) {
    Serial.println("AHT20 nicht gefunden!");
    while (1);
  }

  Serial.println("Sensoren initialisiert!");
}

void loop() {
  // BMP280 Werte
  float temperatureBMP = bmp.readTemperature() + tempOffset;
  float pressure = bmp.readPressure() / 100.0F; // hPa

  // AHT20 Werte
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp); // temp = Temperatur, humidity = Luftfeuchtigkeit

  float temperatureAHT = temp.temperature + tempOffset;
  float humidityAHT = humidity.relative_humidity * humScale + humOffset;

  // Ausgabe
  Serial.print("BMP280 -> Temp: ");
  Serial.print(temperatureBMP);
  Serial.print(" °C, Druck: ");
  Serial.print(pressure);
  Serial.println(" hPa");

  Serial.print("AHT20 -> Temp: ");
  Serial.print(temperatureAHT);
  Serial.print(" °C, Luftfeuchtigkeit: ");
  Serial.print(humidityAHT);
  Serial.println(" %");

  delay(2000);
}
