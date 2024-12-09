
#define TINY_GSM_MODEM_SIM7600

#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#include "secrets.h" // to get network name and password
#include <PubSubClient.h>

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
String gpsTestString();

// initialize GPRS modem and mqtt client
TinyGsm modem(SerialAT);  
TinyGsmClient client(modem);
PubSubClient mqtt(client);
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
  // test cellular connectivity
  SerialMon.println("Checking for cellular connectivity...");
  if (modem.waitForNetwork(30000)) { // wait up to 30 seconds for network
    SerialMon.println("Connected to cellular network!");
    int16_t signal = modem.getSignalQuality();
    SerialMon.print("Signal Quality: ");
    SerialMon.println(signal);
    
    if (modem.localIP()> 0) {
      SerialMon.print("IP Address:");
      SerialMon.println(modem.localIP());
    } else {
      SerialMon.println("Failed to obtain IP address.");
    }
  } else {
    SerialMon.println("Failed to connect to cellular network.");
  }
  mqtt.setServer("broker.emqx.io", 1883);
  delay(500);
  mqttConnect();
}

void loop() {
  modem.maintain();
  delay(15000);
  String coordinatesJson = getGPSString();   
  SerialMon.println(coordinatesJson);
  mqtt.publish(SECRET_MQTT_TOPIC, coordinatesJson.c_str());
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

boolean mqttConnect() {
  SerialMon.print("Connecting to ");
  SerialMon.print(MQTT_BROKER_ENDPOINT);

  boolean status = mqtt.connect(SECRET_CLIENT_DEVICE);

  if (status == false) {
    SerialMon.println(" fail");
    return false;
  }
  SerialMon.println("success");
  mqtt.publish(SECRET_MQTT_TOPIC, "shuttle1 ready");
  mqtt.subscribe(SECRET_MQTT_TOPIC);
  return mqtt.connected();
}

String gpsTestString() {
  char jsonString[256];
  String busName = SECRET_CLIENT_DEVICE;
  float lat = 40.813540, lon = -73.955286;
  sprintf(jsonString, "{\"message\": \"{\\\"name\\\": \\\"%s\\\", \\\"latitude\\\": \\\"%.8f\\\", \\\"longitude\\\": \\\"%.8f\\\"}\"}", 
          busName.c_str(), lat, lon);
  return jsonString;
}
