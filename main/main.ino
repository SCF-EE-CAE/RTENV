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

// Topics to publish data
Adafruit_MQTT_Publish telemetryTopic = Adafruit_MQTT_Publish(&mqttClient, MQTT_TELEMETRY_TOPIC, MQTT_QOS_LEVEL);
Adafruit_MQTT_Publish attributeTopic = Adafruit_MQTT_Publish(&mqttClient, MQTT_ATTRIBUTE_TOPIC, MQTT_QOS_LEVEL);

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

  attributeTopic.publish("{'firmwareVersion':" FIRMWARE_VERSION "}");

  Log.noticeln("MQTT Connected! Firmware version " FIRMWARE_VERSION " sent to Thingsboard.");
}

bool readDHTSensor(float& temperature, float& humidity, int maxAttempts = 3) {
  for (int attempt = 0; attempt < maxAttempts; ++attempt) {
    temperature = dht.readTemperature(); // Celsius
    humidity = dht.readHumidity();

    if (!isnan(temperature) && !isnan(humidity)) {
      return true; // Sensor read successful
    } else {
      Log.warningln("Failed to read DHT sensor, retrying in 2 seconds...");
      delay(2000);
    }
  }

  Log.errorln("Unable to read DHT sensor after multiple attempts.");
  return false; // All attempts failed
}

bool testDHTSensor() {
  float temp, hum;
  return readDHTSensor(temp, hum);
}

void sendData() {
  // Save current timestamp
  unsigned long timestamp = timeClient.getEpochTime(); // unix time in seconds

  // Read Temperature and Humidity with DHT sensor
  float temp, hum;
  if(!readDHTSensor(temp, hum)) {
    Log.errorln("DHT Sensor error.");
    sendStatus("DHT", "ERROR");
    ESP.restart();
  }
  Log.noticeln("DHT Read OK.");

  // Write to message buffer, timestamp in milliseconds by adding 3 zeros
  snprintf(msg, MSG_BUFFER_SIZE, "{'ts':%lu000,'values':{'temperature':%.0f,'humidity':%.0f}}", timestamp, temp, hum);

  // Send MQTT message to telemetryTopic
  Log.noticeln("Publish message: %s", msg);

  while(!telemetryTopic.publish(msg)) {
    Log.errorln("Failed.");
    MQTT_connect();
    Log.noticeln("Trying to send message again now.");
  }

  Log.noticeln("Success, next in 30s.");
  delay(2000);
}

void sendStatus(const char *label, const char *status) {
  snprintf(msg, MSG_BUFFER_SIZE, "{'%s_status':'%s'}", label, status);

  // Send MQTT message to attributeTopic
  Log.noticeln("Publish message: %s", msg);

  while(!attributeTopic.publish(msg)) {
    Log.errorln("Failed.");
    MQTT_connect();
    Log.noticeln("Trying to send message again now.");
  }
}

void setup() {
  // Iniatilize serial interface for logging
  Serial.begin(115200);
  while(!Serial && !Serial.available());
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  // Initialize WiFi connection
  WifiSetup();

  // OTA setup
  ArduinoOTA.setHostname(MQTT_CLIENT_ID);
  ArduinoOTA.begin();

  // Connect with MQTT Broker
  MQTT_connect();

  // Initialize NTP client
  timeClient.begin();

  // Get time for the first time, timesout in 15s after start
  // Send to Thingsboard status of NTP time
  while(!timeClient.isTimeSet()) {
    Log.noticeln("Waiting for time to be set by NTP.");
    if(millis() > 15000) {
      Log.noticeln("Taking too long. Reporting error and restarting.");
      sendStatus("NTP", "ERROR");
      ESP.restart();
    }
    timeClient.update();
    delay(1000);
  }
  Log.noticeln("NTP time set successfully.");
  sendStatus("NTP", "OK");

  // Initialize DHT11 sensor and test
  dht.begin();
  
  if(!testDHTSensor()) {
    Log.errorln("DHT Sensor not working.");
    sendStatus("DHT", "ERROR");
    ESP.restart();
  }
  Log.noticeln("DHT Sensor working.");
  sendStatus("DHT", "OK");

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
