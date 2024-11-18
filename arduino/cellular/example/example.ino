
#define TINY_GSM_MODEM_SIM7600
 
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#include "Adafruit_FONA.h"
#include <HardwareSerial.h>
#include <TinyGsmClient.h>
 
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
#define SMS_TARGET  "+923339218213"
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
  
  // Print modem info
  String modemName = modem.getModemName();
  delay(500);
  SerialMon.println("Modem Name: " + modemName);
 
  String modemInfo = modem.getModemInfo();
  delay(500);
  SerialMon.println("Modem Info: " + modemInfo);
 
 // Set SIM7000G GPIO4 HIGH ,turn on GPS power
  // CMD:AT+SGPIO=0,4,1,1
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+SGPIO=0,4,1,1");
  if (modem.waitResponse(10000L) != 1) {
    SerialMon.println(" SGPIO=0,4,1,1 false ");
  }
 
  modem.enableGPS();
  
  delay(15000);
 
  
   if (! fona.begin(*fonaSerial))
  {
    Serial.println(F("Couldn't find FONA"));
    while(1);
  }
  Serial.println(F("FONA is OK"));
  
  fonaSerial->print("AT+CNMI=2,1\r\n");  //set up the FONA to send a +CMTI notification when an SMS is received
  Serial.println("FONA Ready");
 
}
 
void loop(){
 
 
  
char* bufPtr = fonaNotificationBuffer;    //handy buffer pointer
  
  if (fona.available())      //any data available from the FONA?
  {
    int slot = 0;            //this will be the slot number of the SMS
    int charCount = 0;
    //Read the notification into fonaInBuffer
    do
    {
      *bufPtr = fona.read();
      Serial.write(*bufPtr);
      delay(1);
    } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaNotificationBuffer)-1)));
 
    //Add a terminal NULL to the notification string
    *bufPtr = 0;
 
    //Scan the notification string for an SMS received notification.
    //  If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(fonaNotificationBuffer, "+CMTI: " FONA_PREF_SMS_STORAGE ",%d", &slot)) 
    {
      Serial.print("slot: "); Serial.println(slot);
 
      char callerIDbuffer[32];  //we'll store the SMS sender number in here
 
      // Retrieve SMS sender address/phone number.
      if (! fona.getSMSSender(slot, callerIDbuffer, 31)) 
      {
        Serial.println("Didn't find SMS message in slot!");
      }
      Serial.print(F("FROM: ")); Serial.println(callerIDbuffer);
 
      // Retrieve SMS value.
      uint16_t smslen;
      if (fona.readSMS(slot, smsBuffer, 250, &smslen)) // pass in buffer and max len!
      { 
        smsString = String(smsBuffer);
        Serial.println(smsString);
      }
 
      if (smsString == "location")  
      {
        gpslocation();
//        modem.sendSMS(SMS_TARGET, textForSMS);
//        //fona.sendSMS(callerIDbuffer,textForSMS );
//        Serial.println("SMS send");
//     textForSMS="";
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
void gpslocation(){
 
  float lat      = 0;
  float lon      = 0;
  float speed    = 0;
  float alt     = 0;
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
    }
  }
  SerialMon.println("Retrieving GPS/GNSS/GLONASS location again as a string");
  String gps_raw = modem.getGPSraw();
  SerialMon.println("GPS/GNSS Based Location String: " + gps_raw);
 
  mylati = dtostrf(lat, 3, 6, buff);
  mylong = dtostrf(lon, 3, 6, buff);
 textForSMS = textForSMS + "http://www.google.com/maps/place/" + mylati + "," + mylong ; 
delay(5000);
modem.sendSMS(SMS_TARGET, textForSMS);
        //fona.sendSMS(callerIDbuffer,textForSMS );
        Serial.println("SMS send");
     textForSMS="";
  }