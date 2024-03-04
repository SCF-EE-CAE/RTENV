// WiFi library for ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h> // for OTA discovery in Arduino

// MQTT libraries
#include <WiFiClient.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

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

// Define MQTT client to send data
WiFiClient wifiClient;
Adafruit_MQTT_Client mqttClient(&wifiClient,
                                MQTT_SERVER_ADDRESS, MQTT_SERVER_PORT,
                                MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);

// Topic for publish telemetry data
Adafruit_MQTT_Publish telemetryTopic = Adafruit_MQTT_Publish(&mqttClient, MQTT_TELEMETRY_TOPIC, MQTT_QOS_LEVEL);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER);

// DHT object declaration
DHT dht(DHT_PIN, DHT_TYPE);

// Buffer for MQTT message
char msg[MSG_BUFFER_SIZE];

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

void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqttClient.connected()) return;

  Log.noticeln("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqttClient.connect()) != 0) { // connect will return 0 for connected
    Log.errorln(mqttClient.connectErrorString(ret));
    Log.errorln("Retrying MQTT connection in 5 seconds...");
    mqttClient.disconnect();

    for(int i = 0; i < 50; i++) { // waits 5 seconds while checking for OTA updates
      ArduinoOTA.handle();
      delay(100);
    }

    retries--;
    if (retries == 0) ESP.restart();
  }

  Log.noticeln("MQTT Connected!");
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

  while(!telemetryTopic.publish(msg)) {
    Log.errorln("Failed.");
    MQTT_connect();
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

  // Initialize NTP client
  timeClient.begin();

  // Initialize DHT11 sensor
  dht.begin();

  // OTA setup
  ArduinoOTA.setHostname(MQTT_CLIENT_ID);
  ArduinoOTA.begin();
}

void loop() {
  // Handle OTA firmware flash request
  ArduinoOTA.handle();
  
  // Keep connection with MQTT Broker
  MQTT_connect();

  // Update internal time with NTP server
  timeClient.update();

  int seconds = timeClient.getSeconds();
  if(seconds == 0 || seconds == 30)
    sendData();

  delay(200);
}
