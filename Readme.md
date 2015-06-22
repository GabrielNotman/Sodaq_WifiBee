# SODAQ WifiBee
This is the Arduino library for the SODAQ WifiBee. \n 
It is designed to be used as a client for synchronous request response communication.
It does not support or handle any unsolicited incoming data/requests.

## Length Limitations
__Host + Port(digits):__ Limited to a combined maximum of 234 characters. \n
__SSID + Password:__ Limited to a combined maximum of 233 characters.

## Power Management
It uses the hardware power switch (Bee DTR pin) to leave the device powered
down when not in use.

It supports general __HTTP__ requests as well as __TCP__ and __UDP__ connections.

## HTTP Methods

~~~~~~~~~~~~~~~{.c}
  HTTPAction()
  HTTPGet()
  HTTPPost()
~~~~~~~~~~~~~~~

## TCP Methods

~~~~~~~~~~~~~~~{.c}
openTCP()
sendTCPAscii()
sendTCPBinary()
closeTCP()
~~~~~~~~~~~~~~~

## UDP Methods

~~~~~~~~~~~~~~~{.c}
openUDP()
sendUDPAscii()
sendUDPBinary()
closeTCP()
~~~~~~~~~~~~~~~

## Methods for Reading the Response

~~~~~~~~~~~~~~~{.c}
readHTTPResponse()
readResponseAscii()
readResponseBinary()
~~~~~~~~~~~~~~~

## Example Initialisation

~~~~~~~~~~~~~~~{.c}
  Sodaq_WifiBee wifiBee;
  
  wifiBee.init(Serial1, DTRPin, 256);
  wifiBee.connectionSettings(WIFI_SSID, "", WIFI_PASSWORD);
~~~~~~~~~~~~~~~

## Example Usage

~~~~~~~~~~~~~~~{.c}
  //Send request
  uint16_t code = 0;
  wifiBee.HTTPGet("www.google.com", 80, "/", "", code);
  Serial.println("Result code: " + String(code));
  
  //Read back and display the response
  char buffer[512];
  size_t bytesRead;
  wifiBee.readResponseAscii(buffer, sizeof(buffer), bytesRead);
  Serial.println("Response:");
  Serial.println(buffer);
~~~~~~~~~~~~~~~

The documentation for the library class can be found at: <Todo: Add documentation URL>.

Copyright (c) 2015 Gabriel Notman & M2M4ALL BV.  All rights reserved.
