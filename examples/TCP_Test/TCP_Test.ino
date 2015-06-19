#include "Sodaq_WifiBee.h"

//Wifi Credentials
#define SSID ""
#define PASSWORD ""

//The DTR pin used for on/off
#define DTR_PIN BEEDTR

//HTTP GET command sent over TCP
#define TCP_DATA "GET /get HTTP/1.1\r\nHOST: httpbin.org:80\r\n\r\n"

Sodaq_WifiBee wifiBee;

void setup() {
  Serial.begin(57600);
  Serial1.begin(9600);
  
  Serial.println("Device Type: " + String(wifiBee.getDeviceType()));
  
  wifiBee.init(Serial1, DTR_PIN, 1024);
  wifiBee.connectionSettings(SSID, "", PASSWORD);
  
  //This sets the WifiBee to debug mode over Serial
  //wifiBee.setDiag(Serial);
  
  Serial.println("-----------------------------------------");
  Serial.println("Testing TCP Connection to: httpbin.org:80");
  Serial.println("-----------------------------------------");

  if (wifiBee.openTCP("httpbin.org", 80)) {
    Serial.println("---------------------");
    Serial.println("TCP Connection Opened");
    Serial.println("---------------------");

    if (wifiBee.sendTCPAscii(TCP_DATA)) {
      Serial.println("---------");
      Serial.println("Data Sent");
      Serial.println("---------");
       
      char buffer[1024];
      size_t bytesRead;
      if (wifiBee.readResponseAscii(buffer, sizeof(buffer), bytesRead)) {
        Serial.println("---------");
        Serial.println("Response:");
        Serial.println("---------");
        Serial.print(buffer);
      }
    }
    
    //Close the connection, this powers off the device even if it returns false
    if (wifiBee.closeTCP()) {
      Serial.println("---------------------");
      Serial.println("TCP Connection Closed");
      Serial.println("---------------------");
    }
  }
}

void loop() 
{
}
