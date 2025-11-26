
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <PubSubClient.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>
#include <Wire.h>
#include <MPU6050_tockn.h>

/* Your Wifi  ssid and password */
#define WIFI_SSID "Arthur"
#define WIFI_PASSWORD "arthur0211"

/* Firebase Configurations */

#define USER_EMAIL ""
#define USER_PASSWORD ""

#define API_KEY "AIzaSyAVnn6alm4NgGG1DP1mmxb4HuCWGl323qM"
#define DATABASE_URL "prevention-sense-default-rtdb.firebaseio.com"

/* MQTT Server -> Mosquitto */

const char* mqtt_server = "test.mosquitto.org";

/* Macros for devices pin */
#define CURRENT_PIN       33
#define DHT_SENSOR_PIN    32
#define DHT_SENSOR_TYPE DHT22


MPU6050 mpu(Wire);
DHT dht(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

WiFiClient espClient;
PubSubClient client(espClient);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// -------------- VARIÁVEIS GLOBAIS --------
float temperatura = 0.0;
float corrente = 0.0;
float vibracao = 0.0;

// Buffer global para MQTT
char buffer[10];

unsigned long sendDataPrevMillis = 0;

void setup_wifi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to ");
    Serial.print(WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(".");
      delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
}

void reconnect() 
{ 
  while(!client.connected()) 
  {
      Serial.println("Attempting MQTT connection...");

      if(client.connect("ESPClient")) 
      {
          Serial.println("Connected");
          client.publish("motor/temperatura", buffer);
          client.publish("motor/corrente", buffer);
          client.publish("motor/vibracao", buffer);
      } 
      else 
      {
          Serial.print("Failed, rc=");
          Serial.print(client.state());
          Serial.println(" try again in 5 seconds");
          delay(5000);
      }
    }
}

void setup()
{
    Serial.begin(115200);          // Serial Monitor
    setup_wifi();                  // Wifi setup
    dht.begin();                   // DHT Sensor setup
    Wire.begin();
    mpu.begin();

    /* Firebase Setup */
    config.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    config.database_url = DATABASE_URL;
    Firebase.reconnectNetwork(true);
    Firebase.begin(&config, &auth);
    Firebase.setDoubleDigits(5);
    config.timeout.serverResponse = 10 * 1000;

    /* Node-RED Setup */
    client.setServer(mqtt_server, 1883);
}

void loop()
{
    /******************************************     Reading Sensors value     **********************************************
    
    */

    // Atualiza MPU
  mpu.update();

  // ---------- LEITURAS ---------------
  temperatura = dht.readTemperature();

  int correnteRaw = analogRead(CURRENT_PIN);
  corrente = map(correnteRaw, 0, 4095, 0, 25); 

  float ax = mpu.getAccX();
  float ay = mpu.getAccY();
  float az = mpu.getAccZ();
  vibracao = sqrt(ax*ax + ay*ay + az*az);

    /*****************************************    Sending Data to Firebase RTDB    ******************************************/
    /* Firebase.ready() should be called repeatedly to handle authentication tasks */
    if (Firebase.ready() && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0))  {        
        
        sendDataPrevMillis = millis();

          if (Firebase.RTDB.setFloat(&fbdo, "/motor/temperatura", temperatura)) {
            Serial.println("Temperatura enviada");
          } else {
            Serial.println("Erro Temp: " + fbdo.errorReason());
          }

          if (Firebase.RTDB.setFloat(&fbdo, "/motor/corrente", corrente)) {
            Serial.println("Corrente enviada");
          } else {
            Serial.println("Erro Corrente: " + fbdo.errorReason());
          }

          if (Firebase.RTDB.setFloat(&fbdo, "/motor/vibracao", vibracao)) {
            Serial.println("Vibração enviada");
          } else {
            Serial.println("Erro Vibração: " + fbdo.errorReason());
          }
    }
    

    /******************************************     Sending Data to Node-RED     *******************************************/
    // ---------- MQTT ------------------
  if (!client.connected()) reconnect();

  dtostrf(temperatura, 4, 2, buffer);
  client.publish("motor/temperatura", buffer);

  dtostrf(corrente, 4, 2, buffer);
  client.publish("motor/corrente", buffer);

  dtostrf(vibracao, 4, 2, buffer);
  client.publish("motor/vibracao", buffer);

  client.loop();

  delay(2000);
}