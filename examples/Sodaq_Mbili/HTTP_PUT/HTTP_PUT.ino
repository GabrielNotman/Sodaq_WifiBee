#include <Sodaq_WifiBee.h>

//Wifi Credentials
#define SSID ""
#define PASSWORD ""

//Serial Connections
#define SerialMonitor Serial
#define BeeSerial Serial1

//You can add multiple headers each must be followed by "\r\n"
#define TEST_HEADERS "Accept: */*\r\n"

//The body can contain any ASCII data, cannot start with "\r\n"
#define TEST_BODY "Test body data..." 

Sodaq_WifiBee wifiBee;

void setup() {
  SerialMonitor.begin(57600);
  BeeSerial.begin(9600);
  
  SerialMonitor.println("Device Type: " + String(wifiBee.getDeviceType()));
  
  wifiBee.init(BeeSerial, -1, BEEDTR, BEECTS, 1024);
  wifiBee.connectionSettings(SSID, "", PASSWORD);
  
  //This sets the WifiBee to debug mode over Serial
  wifiBee.setDiag(SerialMonitor);

  SerialMonitor.println("----------------------------------");
  SerialMonitor.println("HTTP PUT to http://httpbin.org/put");
  SerialMonitor.println("----------------------------------");
  
  uint16_t code = 0;
  if (wifiBee.HTTPPut("httpbin.org", 80, "/put", TEST_HEADERS, TEST_BODY, code)) {
    SerialMonitor.println("------------------");
    SerialMonitor.println("Response Code: " + String(code));
    SerialMonitor.println("------------------");
    
    char buffer[1024];
    size_t bytesRead;
    if (wifiBee.readResponseAscii(buffer, sizeof(buffer), bytesRead)) {
      SerialMonitor.println("--------------");
      SerialMonitor.println("Full Response:");
      SerialMonitor.println("--------------");
      SerialMonitor.print(buffer);
      
      buffer[0] = '\0';
      code = 0;
      
      wifiBee.readHTTPResponse(buffer, sizeof(buffer), bytesRead, code);
      SerialMonitor.println("-------------------");
      SerialMonitor.println("HTTP Response Body:");
      SerialMonitor.println("-------------------");
      SerialMonitor.println("------------------");
      SerialMonitor.println("Response Code: " + String(code));
      SerialMonitor.println("------------------");
      SerialMonitor.print(buffer);
    }
  }
}

void loop() 
{
}
