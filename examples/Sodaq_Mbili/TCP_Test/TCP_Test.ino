#include <Sodaq_WifiBee.h>

//Wifi Credentials
#define SSID ""
#define PASSWORD ""

//Serial Connections
#define SerialMonitor Serial
#define BeeSerial Serial1

//HTTP GET command sent over TCP
#define TCP_DATA "GET /get HTTP/1.1\r\nHOST: httpbin.org:80\r\n\r\n"

Sodaq_WifiBee wifiBee;

void setup() {
  SerialMonitor.begin(57600);
  BeeSerial.begin(9600);
  
  Serial.println("Device Type: " + String(wifiBee.getDeviceType()));
  
  wifiBee.init(BeeSerial, -1, BEEDTR, BEECTS, 1024);
  wifiBee.connectionSettings(SSID, "", PASSWORD);
  
  //This sets the WifiBee to debug mode over Serial
  wifiBee.setDiag(SerialMonitor);
  
  SerialMonitor.println("-----------------------------------------");
  SerialMonitor.println("Testing TCP Connection to: httpbin.org:80");
  SerialMonitor.println("-----------------------------------------");

  if (wifiBee.openTCP("httpbin.org", 80)) {
    SerialMonitor.println();
    SerialMonitor.println("---------------------");
    SerialMonitor.println("TCP Connection Opened");
    SerialMonitor.println("---------------------");

    if (wifiBee.sendTCPAscii(TCP_DATA)) {
      SerialMonitor.println();
      SerialMonitor.println("---------");
      SerialMonitor.println("Data Sent");
      SerialMonitor.println("---------");
       
      char buffer[1024];
      size_t bytesRead;
      if (wifiBee.readResponseAscii(buffer, sizeof(buffer), bytesRead)) {
        SerialMonitor.println();
        SerialMonitor.println("---------");
        SerialMonitor.println("Response:");
        SerialMonitor.println("---------");
        SerialMonitor.print(buffer);
      }
    }
    
    //Close the connection, this powers off the device even if it returns false
    if (wifiBee.closeTCP()) {
      SerialMonitor.println("---------------------");
      SerialMonitor.println("TCP Connection Closed");
      SerialMonitor.println("---------------------");
    }
  }
}

void loop() 
{
}
