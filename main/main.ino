// WiFi library for ESP8266
#include <ESP8266WiFi.h>

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

// WiFi Credentials
const char *wifiSsid     = "";
const char *wifiPassword = "";

// MQTT Broker address and credentials
const char *mqttServer = "";
const char *mqttId = "";
const char *mqttUser = "";
const char *mqttPassword = "";

// DHT pin and type definitions
#define DHTPIN 2
#define DHTTYPE DHT11

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br");

// Define MQTT client to send data
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Buffer for MQTT message
#define MSG_BUFFER_SIZE	80
char msg[MSG_BUFFER_SIZE];

// DHT object declaration
DHT dht(DHTPIN, DHTTYPE);

void WifiSetup() {
  Log.noticeln(NL NL"Connecting to %s", wifiSsid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Log.notice(".");
  }

  Log.notice(NL"WiFi connected. IP address: ");
  Log.noticeln(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Log.noticeln("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(mqttId, mqttUser, mqttPassword)) {
      Log.noticeln("Connected.");
    } else {
      Log.errorln("Failed, rc=%d trying again in 5 seconds", mqttClient.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  // Iniatilize serial interface for logging
  Serial.begin(115200);
  while(!Serial && !Serial.available());
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  // Initialize WiFi connection
  WifiSetup();

  // Configure MQTT server address, port 1883 (default) and connect
  mqttClient.setServer(mqttServer, 1883);
  reconnect();
  
  // Initialize NTP client
  timeClient.begin();

  // Initialize DHT11 sensor
  dht.begin();
}

void loop() {
  // Confirms MQTT client connection
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  // Update internal time with NTP server
  timeClient.update();

  // Waits to correct time second (0 or 30 seconds)
  int seconds;
  do {
    delay(200);
    seconds = timeClient.getSeconds();
  } while(seconds != 0 && seconds != 30);

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

  while(!mqttClient.publish("v1/devices/me/telemetry", msg)) {
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
