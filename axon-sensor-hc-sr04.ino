#include <stdio.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoNATS.h>
#include <HCSR04.h>
#include <ArduinoJson.h>

/* HC-SR04 sensor config */
#define TRIGPIN 4
#define ECHOPIN 2

UltraSonicDistanceSensor distanceSensor(TRIGPIN, ECHOPIN);

/* WiFi config */
const char* WIFI_SSID = "your-wifi-ssid";
const char* WIFI_PSK = "your-wifi-password";

/* NATS config */
const String AXON_ID = "8795ff231a"; // Unique ID for axon subjects on demo.nats.io that prevents collision
const String DEVICE_ID = "5fc020ae-084f-48e2-9055-ad0d925e37af"; // Unique device ID
const String TOPIC_UPSTREAM = "axon." + AXON_ID + ".log"; // The prefix and postfix to the subject of incoming messages
const String TOPIC_DOWNSTREAM = "axon." + AXON_ID + ".measure"; // The prefix and postfix to the subject of outgoing messages

WiFiClient client;
NATS nats(
  &client,
  // "192.168.0.22", NATS_DEFAULT_PORT
  "demo.nats.io", NATS_DEFAULT_PORT
);

/*
 * Connect to WiFi network
 */
void connect_wifi() {
  Serial.print("\n\nConnecting to ");
  Serial.println(WIFI_SSID);
  Serial.println();
  Serial.print("Wait for WiFi... ");
    
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
    yield();
  }
  Serial.print("\nWiFi connected: ");
  Serial.println(WiFi.localIP().toString().c_str());
}

/*
 * Do the measurement via the sensor, then send the results to the corresponding channel
 */
void measure(double timestamp, const char*timePrecision) {
  float distance = distanceSensor.measureDistanceCm();
  // distance = duration * 0.034 / 2; // Speed of sound wave divided by 2 (go and back)
  // Displays the distance on the Serial Monitor
  // Serial.print("Distance: ");
  // Serial.print(distance);
  // Serial.println(" cm");
  
  nats.publishf(TOPIC_UPSTREAM.c_str(),
    "{\"time\":%.0f,\"type\":\"measurement\",\"meta\":{\"timePrecision\":\"%s\"},\"body\":{\"device\":\"%s\",\"distance\":%.2f}}",
    timestamp, timePrecision, DEVICE_ID.c_str(), distance);
}

/*
 * NATS message handler.
 *
 * Called every time a new message arrives,
 * then calls the corresponding function that do the measurement and manages the response.
 */
void nats_request_handler(NATS::msg msg) {
  Serial.print("data: ");
  Serial.println(msg.data);
  StaticJsonDocument<1000> jsonDocument;
  DeserializationError err = deserializeJson(jsonDocument, msg.data);
  double timestamp = 0.;
  const char* timePrecision = "";
  if(err) {
    Serial.println("msg.data JSON parsing failed");
  } else {
    timestamp = jsonDocument["time"];
    timePrecision = jsonDocument["meta"]["timePrecision"];
  }

  measure(timestamp, timePrecision);
}

/*
 * NATS connection handler
 */
void nats_on_connect() {
  Serial.print("nats connected and subscribe to ");
  Serial.println(TOPIC_DOWNSTREAM.c_str());
  nats.subscribe(TOPIC_DOWNSTREAM.c_str(), nats_request_handler);
}

/*
 * NATS error handler function
 */
void nats_on_error() {
  Serial.print("######## nats error occured ################\n");
}

/*
 * Initial setup of the unit
 */
void setup() {
  /* Setup serial communication for debugging */
  Serial.begin(9600);

  /* Setup wifi connection */
  connect_wifi();

  /* Setup NATS communication */
  nats.on_connect = nats_on_connect;
  nats.on_error = nats_on_error;
  nats.connect();

  /* Setup HC-SR04 ultrasonic distance sensor */
  sensorSetup();
}

void loop() {
  /* Check WiFi status, and reconnect if connection would has been lost */
  if (WiFi.status() != WL_CONNECTED) {
    connect_wifi();
  }

  /* Handle incoming NATS messages, and send responses */
  nats.process();

  yield(); // Required by NATS library to keep working
}
