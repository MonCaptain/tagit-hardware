#define TINY_GSM_MODEM_SIM7600

#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#include "Adafruit_FONA.h"
#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <ArduinoMqttClient.h>
#include "secrets.h" // to get network name and password

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

TinyGsm modem(SerialAT);
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
  String mylong = ""; // for storing the longittude value
  String mylati = ""; // for storing the latitude value
  String textForSMS;
  char buff[10];
  #define SMS_TARGET  "+13472376074"

// mqtt stuff
TinyGsmClient gsmClient(modem);
MqttClient mqttClient(gsmClient);
const String topic = "location/bus1"; // for MQTT
#define SMS_TARGET  "+13472376074"

void setup(){
  SerialMon.begin(115200);
  SerialMon.println("Place your board outside to catch satellite signal");

  // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Turn on the modem
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(300);
  digitalWrite(PWR_PIN, LOW);
  delay(1000);

  // Set module baud rate and UART pins
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  modem.restart();

  // Print modem info
  String modemName = modem.getModemName();
  SerialMon.println("Modem Name: " + modemName);

  String modemInfo = modem.getModemInfo();
  SerialMon.println("Modem Info: " + modemInfo);

  // Set SIM7000G GPIO4 HIGH, turn on GPS power
  modem.sendAT("+SGPIO=0,4,1,1");
  if (modem.waitResponse(10000L) != 1) {
    SerialMon.println("SGPIO=0,4,1,1 false");
  }

  modem.enableGPS();

  delay(15000);

  // Test cellular connectivity
  SerialMon.println("Checking for cellular connectivity...");
  if (modem.waitForNetwork(30000)) { // wait up to 30 seconds for network
    SerialMon.println("Connected to cellular network!");
  } else {
    SerialMon.println("Failed to connect to cellular network.");
  }

  modem.sendSMS(SMS_TARGET, "ready to go");

  // Connect to AWS IoT and MQTT
  connectAWS();
}

void loop(){
  // Handle incoming FONA notifications
  char* bufPtr = fonaNotificationBuffer;

  if (fona.available()) {
    int slot = 0; // SMS slot
    int charCount = 0;
    // Read notification into fonaInBuffer
    do {
      *bufPtr = fona.read();
      Serial.write(*bufPtr);
      delay(1);
    } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaNotificationBuffer)-1)));

    *bufPtr = 0;

    // Process SMS notifications
    if (1 == sscanf(fonaNotificationBuffer, "+CMTI: " FONA_PREF_SMS_STORAGE ",%d", &slot)) {
      char callerIDbuffer[32];
      if (! fona.getSMSSender(slot, callerIDbuffer, 31)) {
        Serial.println("Didn't find SMS message in slot!");
      }
      Serial.print(F("FROM: ")); Serial.println(callerIDbuffer);

      uint16_t smslen;
      if (fona.readSMS(slot, smsBuffer, 250, &smslen)) {
        smsString = String(smsBuffer);
        Serial.println(smsString);
      }

      if (smsString == "location") {
        modem.sendSMS(SMS_TARGET, "retrieving gps...");
        fona.sendSMS(callerIDbuffer,"retrieving gps...");
        gpslocation();
        modem.sendSMS(SMS_TARGET, textForSMS);
        textForSMS = "";
      }

      if (fona.deleteSMS(slot)) {
        Serial.println(F("OK!"));
      } else {
        Serial.print(F("Couldn't delete SMS in slot "));
        Serial.println(slot);
        fona.print(F("AT+CMGD=?\r\n"));
      }
    }
  }
}

void gpslocation() {
  float lat = 0;
  float lon = 0;
  float speed = 0;
  float alt = 0;
  int vsat = 0;
  int usat = 0;
  float accuracy = 0;
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int min = 0;
  int sec = 0;

  for (int8_t i = 5; i; i--) {
    SerialMon.println("Requesting current GPS/GNSS/GLONASS location");
    if (modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy,
                    &year, &month, &day, &hour, &min, &sec)) {
      SerialMon.println("Latitude: " + String(lat, 8) + "\tLongitude: " + String(lon, 8));
      break;
    } else {
      SerialMon.println("Couldn't get GPS, retrying in 15s.");
      modem.sendSMS(SMS_TARGET, "couldn't get GPS, retrying...");
      delay(15000L);
    }
  }

  String gps_raw = modem.getGPSraw();
  SerialMon.println("GPS Location: " + gps_raw);

  textForSMS = "http://www.google.com/maps/place/" + String(lat, 6) + "," + String(lon, 6);
  modem.sendSMS(SMS_TARGET, textForSMS);
  Serial.println("SMS sent");
  textForSMS = "";
}

void connectAWS() {
  const String msg = "Hello World!";

  // Use your AWS IoT certificates (you will need to replace these placeholders with actual certificate values)
  gsmClient.setCACert(AWS_CERT_CA);
  gsmClient.setCertificate(AWS_CERT_CRT);
  gsmClient.setPrivateKey(AWS_CERT_PRIVATE);

  if (mqttClient.connect(SECRET_AWS_ENDPOINT, 8883)) {
    SerialMon.println("You're connected to the MQTT Broker!");
    modem.sendSMS(SMS_TARGET, "connected to MQTT!");
  } else {
    SerialMon.println("Failed to connect to MQTT Broker!");
    modem.sendSMS(SMS_TARGET, "could not connect to MQTT :(");
    SerialMon.println(mqttClient.connectError());
  }
}
