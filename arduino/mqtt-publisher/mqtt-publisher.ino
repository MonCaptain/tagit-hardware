#include "WiFi.h" // to connect to wifi
#include <ArduinoMqttClient.h>
#include <WiFiClientSecure.h>
#include "secrets.h" // to get network name and password

// LilyGO T-SIM7600G Pinout
#define UART_BAUD   115200
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4
#define LED_PIN     12


// function prototypes
void connectWifi();
void connectAWS();
  
// Wifi and password
const char* ssid = SECRET_SSID;
const char* password =  SECRET_PASS;

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial

WiFiClientSecure wifiClient = WiFiClientSecure();
MqttClient mqttClient(wifiClient);

void setup() {
  SerialMon.begin(115200);
    // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  //Turn on the modem
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(300);
  digitalWrite(PWR_PIN, LOW);

  delay(1000);
  SerialMon.println("LSKDJLKDJSFLKSDJFLJ\n");
  connectWifi();
  connectAWS();
  
}

int count = 0;
void loop() {
  count++;
  mqttClient.beginMessage("location/bus1");
  mqttClient.print("Mama we made it");
  mqttClient.endMessage();
  SerialMon.println(count);
  delay(5000);

}

void connectWifi() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    SerialMon.println("connecting to wifi...\n");
  }
  SerialMon.println("Wifi Connected!!!\n");
  SerialMon.println("IP Address:");
  SerialMon.println(WiFi.localIP());
}

void connectAWS(){
  const String msg = "Hello World!";
  const String topic = "location/bus1";

  wifiClient.setCACert(AWS_CERT_CA);
  wifiClient.setCertificate(AWS_CERT_CRT);
  wifiClient.setPrivateKey(AWS_CERT_PRIVATE);

  if (mqttClient.connect(SECRET_AWS_ENDPOINT, 8883)) {
    SerialMon.println("You're connected to the MQTT Broker!\n");
  }
  else {
    SerialMon.println("Failed to connect to MQTT Broker!\n");
    SerialMon.println(mqttClient.connectError());
  }

  mqttClient.beginMessage(topic);
  mqttClient.print("Mama we made it");
  mqttClient.endMessage();
}

