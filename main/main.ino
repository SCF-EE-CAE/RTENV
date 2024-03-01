// WiFi library for ESP8266
#include <ESP8266WiFi.h>

// MQTT libraries
#include <WiFiClient.h>
#include <PubSubClient.h>

// NTP libraries
#include <NTPClient.h>
#include <WiFiUdp.h>

// DHT11 sensor library
#include <Bonezegei_DHT11.h>

// Logging library
#include <ArduinoLog.h>

// WiFi Credentials
const char *wifiSsid     = "";
const char *wifiPassword = "";

// Broker
const char *mqttServer = "";
const char *mqttId = "";
const char *mqttUser = "";
const char *mqttPassword = "";

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br");

// Define MQTT client to send data
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Buffer for MQTT message
#define MSG_BUFFER_SIZE	80
char msg[MSG_BUFFER_SIZE];

// DHT sensor, GPIO2 port
Bonezegei_DHT11 dht(2);

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
  
  // Read Temperature and Humidity with DHT11
  while(!dht.getData()) {
    Log.errorln("Failed to read DHT sensor, trying again in 2 seconds");
    delay(2000);
  }

  // Prepare values
  unsigned long timestamp = timeClient.getEpochTime(); // unix time in seconds
  int temp = dht.getTemperature(); // Celsius
  int hum = dht.getHumidity(); 

  // Write to message buffer, timestamp in milliseconds by adding 3 zeros
  snprintf(msg, MSG_BUFFER_SIZE, "{'ts':%lu000,'values':{'temperature':%d,'humidity':%d}}", timestamp, temp, hum);

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
