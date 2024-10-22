#define TINY_GSM_MODEM_SIM7600 //needs to be defined before including TINYGsmClient
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include "WiFi.h" // to connect to wifi
#include <TinyGsmClient.h> // for GPS module
#include "secrets.h" // to get network name and password
#include <ArduinoMqttClient.h>
#include <WiFiClientSecure.h>
// Function prototypes

String getGPSString();
String getGPSString2();
void connectWifi();
void connectAWS();
void enableGPSPower();
void disableGPSPower();

// Wifi and password
const char* ssid = SECRET_SSID;
const char* password =  SECRET_PASS;
// MQTT publisher / subscriber
const String topic = SECRET_MQTT_TOPIC;
const String BUS_NAME = SECRET_CLIENT_DEVICE;

// LilyGO T-SIM7600G Pinout
#define UART_BAUD   115200
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4
#define LED_PIN     12

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands
#define SerialAT  Serial1

// Serial communication for GPS
TinyGsm modem(SerialAT); 
// WifiClient to establish MQTT connection 
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
  connectWifi();
  connectAWS();
  
  // Set module baud rate and UART pins
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  if (!modem.restart()) {
    Serial.println("Failed to restart modem, attempting to continue without restarting");
  }
  
  // Print modem info
  String modemName = modem.getModemName();
  delay(500);
  SerialMon.println("Modem Name: " + modemName);

  String modemInfo = modem.getModemInfo();
  delay(500);
  SerialMon.println("Modem Info: " + modemInfo);
}

void loop() {
  
  // get GPS coordinates
  modem.maintain();
  String coordinatesString = getGPSString();
  // String coordinatesString = "this is test coordinate string";
  // send coordinates in the form of "latitude,longitude"   
  mqttClient.beginMessage(topic);
  SerialMon.println(coordinatesString);
  mqttClient.print(coordinatesString);
  mqttClient.endMessage();
  
  delay(5000);
}

void enableGPSPower() {
  modem.sendAT("+SGPIO=0,4,1,1");  // Enable GPS power
  if (modem.waitResponse(10000L) != 1) {
    SerialMon.println("Failed to enable GPS power");
  }
  modem.enableGPS();
}

void disableGPSPower() {
  SerialMon.println("Disabling GPS");
  modem.sendAT("+SGPIO=0,4,1,0");  // Disable GPS power
  if (modem.waitResponse(10000L) != 1) {
    SerialMon.println("Failed to disable GPS power");
  }
  modem.disableGPS();
}

String getGPSString() {
  enableGPSPower();  
  delay(15000);      

  float lat = 0, lon = 0, speed = 0, alt = 0, accuracy = 0;
  int vsat = 0, usat = 0, year = 0, month = 0, day = 0;
  int hour = 0, min = 0, sec = 0;
  
  char jsonString[256];  // Buffer to hold final JSON
  String busName = BUS_NAME;

  for (int8_t i = 5; i; i--) {
    SerialMon.println("Requesting current GPS location");
    
    if (modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy, &year, &month, &day, &hour, &min, &sec)) {
      SerialMon.println("GPS data retrieved successfully");
      break;
    } else {
      SerialMon.println("Failed to get GPS data, retrying...");
      delay(5000L);  // Retry after 5 seconds
      
     if (i == 1) { 
        disableGPSPower();  
        sprintf(jsonString, "{\"message\": {\"error\": \"could not retrieve GPS coordinates for %s\"}}", busName.c_str());
        return jsonString;
      }
    }
  }

  // Construct the JSON string after successful GPS data retrieval
  sprintf(jsonString, "{\"message\": \"{\\\"name\\\": \\\"%s\\\", \\\"latitude\\\": \\\"%.8f\\\", \\\"longitude\\\": \\\"%.8f\\\"}\"}", 
          busName.c_str(), lat, lon);

  disableGPSPower();  // Disable GPS power after use

  // Print the final JSON string
  SerialMon.println(jsonString);
  
  return jsonString;
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
}
