// WiFi library for ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h> // for OTA discovery in Arduino

// MQTT libraries
#include <WiFiClient.h>
#include <PubSubClient.h>

// NTP libraries
#include <NTPClient.h>
#include <WiFiUdp.h>

// DHT sensor library
#include "DHT.h"

// Logging library
#include <ArduinoLog.h>

// OTA library
#include <ArduinoOTA.h>

// Configuration file
#include "config.h"

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER);

// Define MQTT client to send data
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Buffer for MQTT message
char msg[MSG_BUFFER_SIZE];

// DHT object declaration
DHT dht(DHT_PIN, DHT_TYPE);

void WifiSetup() {
  Log.noticeln(NL NL "Connecting to " WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Log.noticeln(".");
  }

  Log.noticeln(NL "WiFi connected. IP address: ");
  Log.noticeln(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Log.noticeln("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Log.noticeln("Connected.");
    } else {
      Log.errorln("Failed, rc=%d trying again in 5 seconds", mqttClient.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void sendData() {
  // Save current timestamp
  unsigned long timestamp = timeClient.getEpochTime(); // unix time in seconds

  float temp;
  float hum;
  
  // Read Temperature and Humidity with DHT sensor
  while(true) {
    temp = dht.readTemperature(); // Celsius
    hum = dht.readHumidity();

    if(isnan(temp) || isnan(hum)) {
      Log.errorln("Failed to read DHT sensor, trying again in 2 seconds");
      delay(2000);
    }
    else {
      break;
    }
  }

  // Write to message buffer, timestamp in milliseconds by adding 3 zeros
  snprintf(msg, MSG_BUFFER_SIZE, "{'ts':%lu000,'values':{'temperature':%.0f,'humidity':%.0f}}", timestamp, temp, hum);

  // Send MQTT message to topic v1/devices/me/telemetry (Thingsboard)
  Log.noticeln("Publish message: %s", msg);

  while(!mqttClient.publish(MQTT_TELEMETRY_TOPIC, msg)) {
    Log.errorln("Failed.");
    delay(500);
    if (!mqttClient.connected()) {
      reconnect();
    }
    Log.noticeln("Trying to send message again now.");
  }

  Log.noticeln("Success, next in 30s.");
  delay(2000);
}

void setup() {
  // Iniatilize serial interface for logging
  Serial.begin(115200);
  while(!Serial && !Serial.available());
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  // Initialize WiFi connection
  WifiSetup();

  // Configure MQTT server address and port and connect
  mqttClient.setServer(MQTT_SERVER_ADDRESS, MQTT_SERVER_PORT);
  reconnect();
  
  // Initialize NTP client
  timeClient.begin();

  // Initialize DHT11 sensor
  dht.begin();

  // OTA setup
  ArduinoOTA.begin();
}

void loop() {
  // Handle OTA firmware flash request
  ArduinoOTA.handle();
  
  // Keep connection with MQTT Broker
  mqttClient.loop();

  // Update internal time with NTP server
  timeClient.update();

  int seconds = timeClient.getSeconds();
  if(seconds == 0 || seconds == 30)
    sendData();

  delay(200);
}
