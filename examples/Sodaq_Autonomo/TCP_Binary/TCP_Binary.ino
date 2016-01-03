#include <Sodaq_WifiBee.h>

//Wifi Credentials
#define SSID ""
#define PASSWORD ""

//Serial Connections
#define SerialMonitor SerialUSB
#define BeeSerial Serial1

//HTTP POST command sent over TCP
#define TCP_DATA "POST /post HTTP/1.1\r\nHOST: httpbin.org:80\r\nContent-Length: 256\r\n\r\n"

Sodaq_WifiBee wifiBee;

void setup() {
  delay(2000);
  SerialMonitor.begin(57600);
  BeeSerial.begin(9600);
  
  SerialMonitor.println("Device Type: " + String(wifiBee.getDeviceType()));
  
  wifiBee.init(BeeSerial, BEE_VCC, BEEDTR, BEECTS, 1024);
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
    
    uint8_t sendBuffer[256];
    for (uint16_t i = 0; i < 256; i++) {
      sendBuffer[i] = i;
    }
    
    //We will send the data in two pieces
    //First the HTTP part then the binary payload
    if (wifiBee.sendTCPAscii(TCP_DATA, false)) {
      wifiBee.sendTCPBinary(sendBuffer, sizeof(sendBuffer));
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
