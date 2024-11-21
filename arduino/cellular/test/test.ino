
#define TINY_GSM_MODEM_SIM7600

#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#include "Adafruit_FONA.h"
#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include "secrets.h" // to get network name and password
#include <PubSubClient.h>
#include <SSLClientESP32.h>


// LilyGO T-SIM7000G Pinout
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


// function prototypes 
boolean mqttConnect();
String getGPSString();
void enableGPSPower();
void disableGPSPower();

TinyGsm modem(SerialAT);  
TinyGsmClient base_client(modem, 0);
SSLClientESP32 client(&base_client);
PubSubClient mqtt(client);
unsigned long lastReconnectAttempt = 0;

// GPRS
const int FONA_RST = 34;
const int RELAY_PIN = 13;
char replybuffer[255];
uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);
String smsString = "";
char fonaNotificationBuffer[64];          //for notifications from the FONA
char smsBuffer[250];
HardwareSerial *fonaSerial = &SerialAT;
  
Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);

unsigned long timeout;
char charArray[20];
unsigned char data_buffer[4] = {0};
char buff[10];
#define SMS_TARGET  "+13472376074"
const char APN[32] = SECRET_CELLULAR_APN;

void setup(){
  SerialMon.begin(115200);
  SerialMon.println("Place your board outside to catch satelite signal");

  // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  //Turn on the modem
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(300);
  digitalWrite(PWR_PIN, LOW);

  delay(1000);
  
  // Set module baud rate and UART pins
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  fonaSerial->begin(UART_BAUD,SERIAL_8N1,PIN_RX, PIN_TX, false);
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  if (!modem.restart()) {
    Serial.println("Failed to restart modem, attempting to continue without restarting");
  }
  delay(1000);
  // Print modem info
  String modemName = modem.getModemName();
  delay(500);
  SerialMon.println("Modem Name: " + modemName);

  String modemInfo = modem.getModemInfo();
  delay(500);
  SerialMon.println("Modem Info: " + modemInfo);

  if (!modem.gprsConnect(APN)){
    SerialMon.println(("FAILED to connect gprs"));
  }
  else {
    SerialMon.println(("gprs successfully connected"));
  }
  delay(1000);
  // Set SIM7000G GPIO4 HIGH ,turn on GPS power
  // CMD:AT+SGPIO=0,4,1,1
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+SGPIO=0,4,1,1");
  if (modem.waitResponse(10000L) != 1) {
    SerialMon.println(" SGPIO=0,4,1,1 false ");
  }

  modem.enableGPS();
  delay(15000);

  
  if (!fona.begin(*fonaSerial))
  {
    Serial.println(F("Couldn't find FONA"));
    while(1);
  }
  Serial.println(F("FONA is OK"));

  fonaSerial->print("AT+CNMI=2,1\r\n");  //set up the FONA to send a +CMTI notification when an SMS is received
  Serial.println("FONA Ready");

  // Test cellular connectivity
  SerialMon.println("Checking for cellular connectivity...");
  if (modem.waitForNetwork(30000)) { // wait up to 30 seconds for network
    SerialMon.println("Connected to cellular network!");
    int16_t signal = modem.getSignalQuality();
    SerialMon.print("Signal Quality: ");
    SerialMon.println(signal);
    
    if (modem.localIP()> 0) {
      SerialMon.print("IP Address: ");
      SerialMon.println(modem.localIP());
    } else {
      SerialMon.println("Failed to obtain IP address.");
    }
    
    modem.sendAT("AT+PING=\"8.8.8.8\"");  // Ping Google's public DNS server
    if (modem.waitResponse(10000L, "OK") == 1) {
      SerialMon.println("Ping successful!");
    } else {
      SerialMon.println("Ping failed.");
    }
  } else {
    SerialMon.println("Failed to connect to cellular network.");
  }
  // fona.sendSMS("+13472376074", "ready to go");
  // modem.sendSMS(SMS_TARGET, "ready to go");
}

void loop() {
  
  // get GPS coordinates
  modem.maintain();
  // String coordinatesString = getGPSString();
  // String coordinatesString = "this is test coordinate string";
  // modem.sendSMS("+13472376074", coordinatesString);
  // send coordinates in the form of "latitude,longitude"   
  // SerialMon.println(coordinatesString);
  
    if (!mqtt.connected()) {
    SerialMon.println("=== MQTT NOT CONNECTED ===");
    // Reconnect every 10 seconds
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) { lastReconnectAttempt = 0; }
    }
    delay(100);
    return;
  }
  mqtt.loop();
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
  String busName = SECRET_CLIENT_DEVICE;

  for (int8_t i = 15; i; i--) {
    SerialMon.println("Requesting current GPS location");
    
    if (modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy, &year, &month, &day, &hour, &min, &sec)) {
      SerialMon.println("GPS data retrieved successfully");
      break;
    } else {
      SerialMon.println("Failed to get GPS data, retrying...");
      delay(15000L);  // Retry after 5 seconds
      
     if (i == 1) { 
        // disableGPSPower();  
        sprintf(jsonString, "{\"message\": {\"error\": \"could not retrieve GPS coordinates for %s\"}}", busName.c_str());
        return jsonString;
      }
    }
  }

  // Construct the JSON string after successful GPS data retrieval
  sprintf(jsonString, "{\"message\": \"{\\\"name\\\": \\\"%s\\\", \\\"latitude\\\": \\\"%.8f\\\", \\\"longitude\\\": \\\"%.8f\\\"}\"}", 
          busName.c_str(), lat, lon);

  // disableGPSPower();  // Disable GPS power after use

  // Print the final JSON string
  SerialMon.println(jsonString);
  
  return jsonString;
}


void connectMQTT() {
    // Set SSL/TLS certificates
    
    
    client.setCACert(AWS_CERT_CA);
    client.setCertificate(AWS_CERT_CRT);
    client.setPrivateKey(AWS_CERT_PRIVATE);
    
    // Set MQTT server and port
    mqtt.setServer(SECRET_AWS_ENDPOINT, 8883);  // AWS IoT Core uses port 8883 for secure MQTT

    SerialMon.println("Connecting to AWS IoT Core...");
    
    // Attempt MQTT connection
    while (!mqtt.connected()) {
        if (mqtt.connect(SECRET_CLIENT_DEVICE)) {
            SerialMon.println("MQTT connected successfully!");
        } else {
            SerialMon.print("MQTT connection failed. Error code: ");
            SerialMon.println(mqtt.state());
            SerialMon.println("Retrying in 5 seconds...");
            delay(5000);
        }
    }

    // Subscribe to topics (optional, only if you expect to receive messages)
    if (mqtt.subscribe(SECRET_MQTT_TOPIC)) {
        SerialMon.println("Subscribed to topic: " + String(SECRET_MQTT_TOPIC));
    } else {
        SerialMon.println("Failed to subscribe to topic.");
    }
}
