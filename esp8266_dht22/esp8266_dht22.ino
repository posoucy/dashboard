/*
 * ESP8266 + DHT22 — Serveur de température/humidité pour le dashboard
 *
 * Matériel requis :
 *   - ESP8266 (NodeMCU, Wemos D1 Mini, etc.)
 *   - Capteur DHT22 (AM2302)
 *     VCC  → 3.3V (ou 5V selon module)
 *     GND  → GND
 *     DATA → D4 (GPIO2) — résistance pull-up 10kΩ entre DATA et VCC
 *
 * Bibliothèques à installer via Arduino IDE (Gestionnaire de bibliothèques) :
 *   - "DHT sensor library" par Adafruit
 *   - "Adafruit Unified Sensor" (dépendance)
 *   - "ESP8266WiFi"  )
 *   - "ESP8266WebServer" ) incluses avec le package ESP8266 dans le board manager
 *   - "ESP8266mDNS"  )
 *
 * URL board manager ESP8266 :
 *   http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *
 * Endpoint exposé :  GET http://<IP>/sensor
 * ou via mDNS :      GET http://sensor.local/sensor
 *
 * Réponse JSON :
 *   { "temperature": 21.3, "humidity": 58.0, "uptime": 3600 }
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DHT.h>

// ═══════════════════════════════════════════════
//   PARAMÈTRES À MODIFIER
// ═══════════════════════════════════════════════
const char* WIFI_SSID = "VOTRE_SSID";        // Nom de votre réseau WiFi
const char* WIFI_PASS = "VOTRE_MOT_DE_PASSE"; // Mot de passe WiFi
const char* HOSTNAME  = "sensor";             // → http://sensor.local/sensor

#define DHTPIN  D4      // Broche DATA du DHT22 (GPIO2)
#define DHTTYPE DHT22   // Capteur DHT22 (AM2302)
// ═══════════════════════════════════════════════

DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);

// Dernières valeurs lues (mise en cache pour ne pas saturer le capteur)
float lastTemp = NAN;
float lastHumi = NAN;
unsigned long lastReadMs = 0;

// ── Lecture non-bloquante du capteur (minimum 2s entre deux lectures) ──
void readSensor() {
  if (millis() - lastReadMs < 2000) return;
  lastReadMs = millis();

  float t = dht.readTemperature();  // °C
  float h = dht.readHumidity();     // %

  if (!isnan(t)) lastTemp = t;
  if (!isnan(h)) lastHumi = h;

  if (!isnan(t) && !isnan(h)) {
    Serial.printf("[DHT22] %.1f°C  %.0f%%\n", t, h);
  } else {
    Serial.println("[DHT22] Erreur de lecture");
  }
}

// ── En-têtes CORS (pour autoriser les requêtes du navigateur) ──
void sendCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin",  "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Cache-Control",                "no-cache, no-store");
}

// ── GET /sensor — réponse JSON ──
void handleSensor() {
  readSensor();
  sendCORSHeaders();

  if (isnan(lastTemp) || isnan(lastHumi)) {
    server.send(503, "application/json",
      "{\"error\":\"sensor_not_ready\",\"message\":\"Capteur pas encore lu\"}");
    return;
  }

  String json = String("{")
    + "\"temperature\":"  + String(lastTemp, 1)
    + ",\"humidity\":"    + String(lastHumi, 1)
    + ",\"uptime\":"      + String(millis() / 1000)
    + ",\"ip\":\""        + WiFi.localIP().toString() + "\""
    + "}";

  server.send(200, "application/json", json);
}

// ── OPTIONS /sensor — réponse au preflight CORS ──
void handleOptions() {
  sendCORSHeaders();
  server.send(204);
}

// ═══════════════════════════════════════════════
//   SETUP
// ═══════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP8266 DHT22 Dashboard Sensor ===");

  // Initialisation du capteur
  dht.begin();

  // Connexion WiFi
  WiFi.hostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connexion WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("✓ Connecté — IP : ");
  Serial.println(WiFi.localIP());

  // mDNS : accessible via http://sensor.local
  if (MDNS.begin(HOSTNAME)) {
    Serial.print("✓ mDNS actif  : http://");
    Serial.print(HOSTNAME);
    Serial.println(".local/sensor");
  }

  // Routes HTTP
  server.on("/sensor", HTTP_GET,     handleSensor);
  server.on("/sensor", HTTP_OPTIONS, handleOptions);
  server.on("/",       HTTP_GET,     handleSensor);  // alias racine

  server.begin();
  Serial.println("✓ Serveur HTTP démarré (port 80)");
  Serial.println("======================================\n");

  // Première lecture immédiate
  delay(2100);
  readSensor();
}

// ═══════════════════════════════════════════════
//   LOOP
// ═══════════════════════════════════════════════
void loop() {
  server.handleClient();
  MDNS.update();

  // Lecture périodique toutes les 10 secondes (non-bloquant)
  if (millis() - lastReadMs >= 10000) {
    readSensor();
  }
}
