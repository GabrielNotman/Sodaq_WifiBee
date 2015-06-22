#include <Sodaq_WifiBee.h>

//Wifi Credentials
#define SSID ""
#define PASSWORD ""

//The DTR pin used for on/off
#define DTR_PIN BEEDTR

//You can add multiple headers each must be followed by "\r\n"
#define TEST_HEADERS "Accept: */*\r\n"

//The body can contain any ASCII data, cannot start with "\r\n"
#define TEST_BODY "Test body data..." 

Sodaq_WifiBee wifiBee;

void setup() {
  Serial.begin(57600);
  Serial1.begin(9600);
  
  Serial.println("Device Type: " + String(wifiBee.getDeviceType()));
  
  wifiBee.init(Serial1, DTR_PIN, 1024);
  wifiBee.connectionSettings(SSID, "", PASSWORD);
  
  //This sets the WifiBee to debug mode over Serial
  //wifiBee.setDiag(Serial);

  Serial.println("------------------------------------");
  Serial.println("HTTP POST to http://httpbin.org/post");
  Serial.println("------------------------------------");
  
  uint16_t code = 0;
  if (wifiBee.HTTPPost("httpbin.org", 80, "/post", TEST_HEADERS, TEST_BODY, code)) {
    Serial.println("------------------");
    Serial.println("Response Code: " + String(code));
    Serial.println("------------------");
  
    char buffer[1024];
    size_t bytesRead;
    if (wifiBee.readResponseAscii(buffer, sizeof(buffer), bytesRead)) {
      Serial.println("--------------");
      Serial.println("Full Response:");
      Serial.println("--------------");
      Serial.print(buffer);
      
      buffer[0] = '\0';
      code = 0;
      
      wifiBee.readHTTPResponse(buffer, sizeof(buffer), bytesRead, code);
      Serial.println("-------------------");
      Serial.println("HTTP Response Body:");
      Serial.println("-------------------");
      Serial.println("------------------");
      Serial.println("Response Code: " + String(code));
      Serial.println("------------------");
      Serial.print(buffer);
    }
  }
}

void loop() 
{
}
