#define TINY_GSM_MODEM_SIM7600 //needs to be defined before including TINYGsmClient
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include "WiFi.h" // to connect to wifi
#include <TinyGsmClient.h> // for GPS module
#include "secrets.h" // to get network name and password
#include <ArduinoMqttClient.h>
#include <WiFiClientSecure.h>
// Function prototypes
String getGPSString();
void connectWifi();
void connectAWS();

// Wifi and password
const char* ssid = SECRET_SSID;
const char* password =  SECRET_PASS;
const String topic = "location/bus1"; // for MQTT 

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
  // send coordinates in the form of "latitude,longitude"   
  mqttClient.beginMessage(topic);
  mqttClient.print(coordinatesString);
  mqttClient.endMessage();
  
  delay(5000);
}


String getGPSString(){
    // Set SIM7000G GPIO4 HIGH ,turn on GPS power
  // CMD:AT+SGPIO=0,4,1,1
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+SGPIO=0,4,1,1");
  if (modem.waitResponse(10000L) != 1) {
    SerialMon.println(" SGPIO=0,4,1,1 false ");
  }

  modem.enableGPS();
  
  delay(15000);
  float lat      = 0;
  float lon      = 0;
  float speed    = 0;
  float alt      = 0;
  int   vsat     = 0;
  int   usat     = 0;
  float accuracy = 0;
  int   year     = 0;
  int   month    = 0;
  int   day      = 0;
  int   hour     = 0;
  int   min      = 0;
  int   sec      = 0;
  
  for (int8_t i = 15; i; i--) {
    SerialMon.println("Requesting current GPS/GNSS/GLONASS location");
    if (modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy,
                     &year, &month, &day, &hour, &min, &sec)) {
      SerialMon.println("Latitude: " + String(lat, 8) + "\tLongitude: " + String(lon, 8));
      SerialMon.println("Speed: " + String(speed) + "\tAltitude: " + String(alt));
      SerialMon.println("Visible Satellites: " + String(vsat) + "\tUsed Satellites: " + String(usat));
      SerialMon.println("Accuracy: " + String(accuracy));
      SerialMon.println("Year: " + String(year) + "\tMonth: " + String(month) + "\tDay: " + String(day));
      SerialMon.println("Hour: " + String(hour) + "\tMinute: " + String(min) + "\tSecond: " + String(sec));
      break;
    } 
    else {
      SerialMon.println("Couldn't get GPS/GNSS/GLONASS location, retrying in 15s.");
      delay(15000L);
      if (i == 0) {
        return "couldn't get any coordinates";
      }
    }
  }
  SerialMon.println("Retrieving GPS/GNSS/GLONASS location again as a string");
  String gps_raw = modem.getGPSraw();
  SerialMon.println("GPS/GNSS Based Location String: " + gps_raw);
  // SerialMon.println("Disabling GPS");
  // modem.disableGPS();

//   Set SIM7000G GPIO4 LOW ,turn off GPS power
//   CMD:AT+SGPIO=0,4,1,0
//   Only in version 20200415 is there a function to control GPS power
//   modem.sendAT("+SGPIO=0,4,1,0");
//   if (modem.waitResponse(10000L) != 1) {
//     SerialMon.println(" SGPIO=0,4,1,0 false ");
//   }

  return String(lat, 8) + "," + String(lon, 8);
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
