/* -------------------------------------------------------------
   ESP32 + VL53L0X + DHT11
   ------------------------------------------------------------- */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "Adafruit_VL53L0X.h"
#include "DHT.h"

// -------------------------------------------------------------
// WIFI
// -------------------------------------------------------------
const bool WIFI_ENABLED = true;

const char* WIFI_SSID = "wifi-nome";
const char* WIFI_PASS = "wifi-senha";

// -------------------------------------------------------------
// SUPABASE
// -------------------------------------------------------------
const char* SUPABASE_URL =
  "X";

const char* SUPABASE_SENSOR_URL =
  "X";

// Use the Service Role key for inserts (requires admin privileges). Generate it in Supabase dashboard.
const char* SUPABASE_SERVICE_KEY = "X";
const char* SUPABASE_KEY = "X";


const char* ESTABLISHMENT_ID =
  "est-aquarium";

// -------------------------------------------------------------
// I2C - VL53L0X
// -------------------------------------------------------------
#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// -------------------------------------------------------------
// DHT11
// -------------------------------------------------------------
#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// -------------------------------------------------------------
// PROXIMIDADE
// -------------------------------------------------------------
const int DIST_THRESHOLD = 800;
const uint32_t TRIGGER_MS = 1500;
const uint32_t COOLDOWN_MS = 5000;

unsigned long objectDetectedAt = 0;
bool detecting = false;
unsigned long lastTrigger = 0;

// -------------------------------------------------------------
// ENVIO PERIÓDICO DHT11
// -------------------------------------------------------------
const uint32_t SENSOR_SEND_INTERVAL = 30000; // 30 segundos
unsigned long lastSensorSend = 0;

// -------------------------------------------------------------
void connectWiFi() {

  if (!WIFI_ENABLED) return;

  Serial.print("Conectando ao WiFi ");

  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 10000
  ) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("\nWiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println("\nFalha no WiFi.");
  }
}

// -------------------------------------------------------------
void sendProximityEvent(
  int distance,
  float temperature,
  float humidity
) {

  if (!WIFI_ENABLED || WiFi.status() != WL_CONNECTED) {

    Serial.println("[INFO] WiFi offline.");
    return;
  }

  HTTPClient http;

  http.begin(SUPABASE_URL);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);

  http.addHeader(
    "Authorization",
    String("Bearer ") + SUPABASE_KEY
  );

  StaticJsonDocument<256> doc;

  doc["establishment_id"] = ESTABLISHMENT_ID;
  doc["event_type"] = "proximity";

  JsonObject payload =
    doc.createNestedObject("payload");

  payload["distance_mm"] = distance;
  payload["temperature"] = temperature;
  payload["humidity"] = humidity;

  String body;

  serializeJson(doc, body);

  Serial.println("[INFO] Enviando evento...");

  int code = http.POST(body);

  if (code > 0) {

    Serial.print("HTTP ");
    Serial.println(code);

    Serial.println(http.getString());

  } else {

    Serial.print("Erro HTTP: ");
    Serial.println(code);
  }

  http.end();
}

// -------------------------------------------------------------
void sendSensorReading(
  float temperature,
  float humidity
) {

  if (!WIFI_ENABLED || WiFi.status() != WL_CONNECTED) {
    Serial.println("[SENSOR] WiFi offline.");
    return;
  }

  if (isnan(temperature) || isnan(humidity) || temperature < 0) {
    Serial.println("[SENSOR] Leitura inválida, não enviando.");
    return;
  }

  HTTPClient http;

  http.begin(SUPABASE_SENSOR_URL);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader(
    "Authorization",
    String("Bearer ") + SUPABASE_KEY
  );

  StaticJsonDocument<200> doc;

  doc["establishment_id"] = ESTABLISHMENT_ID;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;

  String body;
  serializeJson(doc, body);

  Serial.println("[SENSOR] Enviando leitura DHT11...");

  int code = http.POST(body);

  if (code > 0) {
    Serial.print("[SENSOR] HTTP ");
    Serial.println(code);
  } else {
    Serial.print("[SENSOR] Erro HTTP: ");
    Serial.println(code);
  }

  http.end();
}

// -------------------------------------------------------------
void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println("\n--- ESP32 + VL53L0X + DHT11 ---");

  // -----------------------------------------------------------
  // I2C
  // -----------------------------------------------------------
  Wire.begin(SDA_PIN, SCL_PIN);

  // -----------------------------------------------------------
  // VL53L0X
  // -----------------------------------------------------------
  if (!lox.begin()) {

    Serial.println("Falha ao iniciar VL53L0X");

    while (true) {
      delay(10);
    }
  }

  Serial.println("VL53L0X iniciado!");

  // -----------------------------------------------------------
  // DHT11
  // -----------------------------------------------------------
  dht.begin();

  Serial.println("DHT11 iniciado!");

  // -----------------------------------------------------------
  // WIFI
  // -----------------------------------------------------------
  connectWiFi();
}

// -------------------------------------------------------------
void loop() {

  // -----------------------------------------------------------
  // Reconecta WiFi automaticamente
  // -----------------------------------------------------------
  if (
    WIFI_ENABLED &&
    WiFi.status() != WL_CONNECTED
  ) {
    connectWiFi();
  }
  unsigned long now = millis();

  // -----------------------------------------------------------
  // Leitura DHT11
  // -----------------------------------------------------------
  float humidity = dht.readHumidity();

  float temperature =
    dht.readTemperature();

  if (
    isnan(humidity) ||
    isnan(temperature)
  ) {

    Serial.println(
      "Erro ao ler DHT11"
    );

    humidity = -1;
    temperature = -1;
  }

  // -----------------------------------------------------------
  // Envio periódico DHT11 (a cada 30s)
  // -----------------------------------------------------------
  if (now - lastSensorSend >= SENSOR_SEND_INTERVAL) {
    sendSensorReading(temperature, humidity);
    lastSensorSend = now;
  }

  // -----------------------------------------------------------
  // Leitura VL53L0X
  // -----------------------------------------------------------
  VL53L0X_RangingMeasurementData_t measure;

  lox.rangingTest(&measure, false);

  // duplicate now removed



  if (measure.RangeStatus != 4) {

    int distance =
      measure.RangeMilliMeter;

    Serial.println("----------------");

    Serial.print("Distância: ");
    Serial.print(distance);
    Serial.println(" mm");

    Serial.print("Temperatura: ");
    Serial.print(temperature);
    Serial.println(" C");

    Serial.print("Umidade: ");
    Serial.print(humidity);
    Serial.println(" %");

    // ---------------------------------------------------------
    // DETECÇÃO
    // ---------------------------------------------------------
    if (distance < DIST_THRESHOLD) {

      if (!detecting) {

        detecting = true;

        objectDetectedAt = now;

        Serial.println(
          "Objeto detectado..."
        );

      } else if (
        now - objectDetectedAt >=
        TRIGGER_MS
      ) {

        if (
          now - lastTrigger >
          COOLDOWN_MS
        ) {

          Serial.println(
            "=== EVENTO ==="
          );

          sendProximityEvent(
            distance,
            temperature,
            humidity
          );

          lastTrigger = now;
        }

        detecting = false;
      }

    } else {

      if (detecting) {

        detecting = false;

        Serial.println(
          "Objeto saiu."
        );
      }
    }

  } else {

    Serial.println(
      "VL53L0X fora de alcance"
    );

    detecting = false;
  }

  delay(2000);
}