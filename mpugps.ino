const int detapin =7;
const int clockpin = 6;
const in Alimentation = A5;

void setup() 
pinmode (dataPin, INPUT);
pinmode (clock pin , OUTPUT).
pinmode (Alimentation, OUTPUT)
digitalwrite (Alimentation,low)//ALIMENTER LE CAPTEUR
}

void loop () 

#include <PubSubClient.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include <MPU6050.h>

// WiFi
const char* ssid = "TP-LINK_Outdoor_B0FFBE";
const char* password = "BfeA34xyM/";
const char* mqtt_server = "172.18.241.19";
const int mqtt_port = 1883;

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// GPS
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
static const int RXPin = 16;
static const int TXPin = 17;
const uint32_t GPSBaud = 9600;

// MPU6050
MPU6050 mpu;

unsigned long lastMsg = 0;

// Fonction pour connexion WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connexion au WiFi : ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println();
  Serial.println("WiFi connecté");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());
}

// Fonction pour reconnexion MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connexion au broker MQTT...");
    String clientId = "ESP32-GPS-MPU-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println(" connecté");
    } else {
      Serial.print(" échec, code = ");
      Serial.print(client.state());
      Serial.println(" nouvelle tentative dans 5 secondes");
      delay(5000);
    }
  }
}

// Setup GPS
void setup_GPS() {
  gpsSerial.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);
  Serial.println("Lecture GPS avec ESP32...");
}

// Setup MPU6050
void setup_MPU() {
  Wire.begin(21, 22); // SDA, SCL
  mpu.initialize();
  if (mpu.testConnection()) {
    Serial.println("MPU6050 connecté");
  } else {
    Serial.println("Échec de la connexion MPU6050");
  }
  Serial.println("--------------------------------");
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  setup_GPS();
  setup_MPU();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Lecture GPS
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  unsigned long now = millis();
  if (now - lastMsg > 2000) { // 2 secondes
    lastMsg = now;

    // Lecture MPU6050
    int16_t ax, ay, az, gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    // Conversion en unités physiques
    float ax_g = ax / 16384.0;
    float ay_g = ay / 16384.0;
    float az_g = az / 16384.0;
    float gx_dps = gx / 131.0;
    float gy_dps = gy / 131.0;
    float gz_dps = gz / 131.0;

    // Vérification GPS
    if (gps.location.isValid()) {
      Serial.print("Latitude: "); Serial.println(gps.location.lat(), 6);
      Serial.print("Longitude: "); Serial.println(gps.location.lng(), 6);
      Serial.print("Altitude: "); Serial.println(gps.altitude.meters());
      Serial.print("Vitesse (km/h): "); Serial.println(gps.speed.kmph());
      Serial.print("Satellites: "); Serial.println(gps.satellites.value());
      
      // Création du payload JSON
      char payload[512];
      snprintf(payload, sizeof(payload),
        "{\"device\":\"esp32-gps-mpu\",\"latitude\":%.6f,\"longitude\":%.6f,"
        "\"altitude\":%.2f,\"vitesse_kmh\":%.2f,"
        "\"ax_g\":%.3f,\"ay_g\":%.3f,\"az_g\":%.3f,"
        "\"gx_dps\":%.3f,\"gy_dps\":%.3f,\"gz_dps\":%.3f,"
        "\"satellites\":%d,\"timestamp\":%lu}",
        gps.location.lat(),
        gps.location.lng(),
        gps.altitude.meters(),
        gps.speed.kmph(),
        ax_g, ay_g, az_g,
        gx_dps, gy_dps, gz_dps,
        gps.satellites.value(),
        millis()
      );
      client.publish("gps_mpu/data", payload);
      Serial.println("Message MQTT envoyé :");
      Serial.println(payload);
    } else {
      Serial.println("Position GPS non valide pour le moment.");
    }
  }
}