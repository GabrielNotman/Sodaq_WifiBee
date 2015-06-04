/* The MIT License (MIT)

 Copyright (c) <2015> <Gabriel Notman & M2M4ALL BV>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE. */

#include "Sodaq_WifiBee.h"

#define ENABLE_RADIO_DIAG 1

#if ENABLE_RADIO_DIAG
#define diagPrint(...) { if (_diagStream) _diagStream->print(__VA_ARGS__); }
#define diagPrintLn(...) { if (_diagStream) _diagStream->println(__VA_ARGS__); }
#else
#define diagPrint(...)
#define diagPrintLn(...)
#endif

// Lua prompts
#define LUA_PROMPT "\r\n> "
#define CONNECT_PROMPT ".:Connected:."
#define RECONNECT_PROMPT ".:Reconnected:."
#define DISCONNECT_PROMPT ".:Disconnected:."
#define SENT_PROMPT ".:Data Sent:."
#define RECEIVED_PROMPT ".:Data Received:."
#define STATUS_PROMPT "\r\nStatus: "

// Lua connection callback scripts
#define CONNECT_CALLBACK "function(socket) print(\".:Connected:.\") end"
#define RECONNECT_CALLBACK "function(socket) print(\".:Reconnected:.\") end"
#define DISCONNECT_CALLBACK "function(socket) print(\".:Disconnected:.\") end"
#define SENT_CALLBACK "function(socket) print(\".:Data Sent:.\") end"
#define RECEIVED_CALLBACK "function(socket, data) lastResponse=data lastSize=data:len() uart.write(0, \".:Data Received:.\\r\\n\", \"Length: \", tostring(data:len()), \"\\r\\n\", data, \"\\r\\n\") end"
#define STATUS_CALLBACK "Status: "

// Timeout constants
#define RESPONSE_TIMEOUT 2000
#define WIFI_CONNECT_TIMEOUT 4000
#define SERVER_CONNECT_TIMEOUT 5000
#define SERVER_DISCONNECT_TIMEOUT 2000
#define WAKE_DELAY 1000

// Other
#define DEFAULT_BAUD 9600

// Lua constants for each connection type
#define TCP_CONNECTION "net.TCP"
#define UDP_CONNECTION "net.UDP"

Sodaq_WifiBee::Sodaq_WifiBee()
{
  _APN = "";
  _username = "";
  _password = "";

  _dataStream = NULL;
  _diagStream = NULL;
}

void Sodaq_WifiBee::init(HardwareSerial& stream, const uint8_t dtrPin)
{
  _dataStream = &stream;
  _dtrPin = dtrPin;

  pinMode(_dtrPin, OUTPUT);
  
  off();
}

void Sodaq_WifiBee::connectionSettings(const String APN, const String username,
  const String password)
{
  _APN = APN;
  _username = username;
  _password = password;
}

void Sodaq_WifiBee::setDiag(Stream& stream)
{
  _diagStream = &stream;
}

void Sodaq_WifiBee::on()
{
  digitalWrite(_dtrPin, LOW);
  readForTime(WAKE_DELAY);  
}

void Sodaq_WifiBee::off()
{
  digitalWrite(_dtrPin, HIGH);
}


// HTTP methods
bool Sodaq_WifiBee::HTTPAction(const String server, const uint16_t port,
  const String method, const String location, String headers,
  String body, uint16_t& httpCode)
{
  headers.trim();
  body.trim();

  bool result = true;

  //We only care if we can open the connection
  result = openConnection(server, port, TCP_CONNECTION);

  if (result) {
    send("wifiConn:send(\"");
  
    send(method);
    send(" ");
    send(location);
    send(" HTTP/1.1\\r\\n");

    send("HOST: ");
    send(server);
    send(":");
    send(String(port, DEC));
    send("\\r\\n");

    send("Content-Length: ");
    send(String(body.length(), DEC));
    send("\\r\\n");

    sendEscaped(headers);
    send("\\r\\n");

    sendEscaped(body);

    send("\")\r\n");
  }

  // Wait till we hear that it was sent
  if (result) {
    result=readTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
  }

  // need to read response code

  // The connection might have closed automatically
  // Or it failed to open
  closeConnection();

  return result;
}

bool Sodaq_WifiBee::HTTPGet(const String server, const uint16_t port,
  const String location, const String headers, uint16_t& httpCode)
{
  return true;
}

bool Sodaq_WifiBee::HTTPPost(const String server, const uint16_t port,
  const String location, const String headers, const String body,
  uint16_t& httpCode)
{
  return true;
}

// TCP methods
bool Sodaq_WifiBee::openTCP(const String server, uint16_t port)
{
  return openConnection(server, port, TCP_CONNECTION);
}

bool Sodaq_WifiBee::sendTCPAscii(const String data) 
{
  return sendAsciiData(data);
}

bool Sodaq_WifiBee::sendTCPBinary(const uint8_t* data, const size_t length)
{
  return sendBinaryData(data, length);
}

bool Sodaq_WifiBee::closeTCP()
{
  return closeConnection();
}

// UDP methods
bool Sodaq_WifiBee::openUDP(const String server, uint16_t port)
{
  return openConnection(server, port, UDP_CONNECTION);
}

bool Sodaq_WifiBee::sendUDPAscii(const String data)
{
  return sendAsciiData(data);
}

bool Sodaq_WifiBee::sendUDPBinary(const uint8_t* data, const size_t length)
{
  return sendBinaryData(data, length);
}

bool Sodaq_WifiBee::closeUDP()
{
  return closeConnection();
}

bool Sodaq_WifiBee::readResponse(uint8_t& buffer, const size_t size)
{
  on();
  //Send command to dump 
  //Read size-1 or response size
  off();
  return true;
}

bool Sodaq_WifiBee::readHTTPResponse(uint8_t& buffer, const size_t size, 
  uint16_t& httpCode)
{
  on();
  //Send command to dump
  //Read until empty line
  //Save the rest until buffer is full or response is complete
  off();
  return true;
}

// Private methods
void Sodaq_WifiBee::flushInputStream()
{
  if (_dataStream) {
    while (_dataStream->available()) {
      diagPrint((char)_dataStream->read());
    }
  }
}

int Sodaq_WifiBee::readForTime(const uint32_t timeMS)
{
  if (!_dataStream) {
    return 0;
  }

  int count = 0;
  uint32_t maxTS = millis() + timeMS;

  while (millis()  < maxTS) {
    if (_dataStream->available()) {
      char c = _dataStream->read();
      diagPrint(c);
      count++;
    } else {
      _delay(10);
    }
  }

  return count;
}

bool Sodaq_WifiBee::readChar(char& data, const uint32_t timeMS)
{
  if (!_dataStream) {
    return false;
  }

  bool result = false;

  uint32_t maxTS = millis() + timeMS;
  while ((millis()  < maxTS) && (!result)) {
    if (_dataStream->available()) {
      data = _dataStream->read();
      diagPrintLn(data);
      result = true;
    }
    else {
      _delay(10);
    }
  }
  
  return result;
}

bool Sodaq_WifiBee::readTillPrompt(const String prompt, const uint32_t timeMS)
{
  if (!_dataStream) {
    return false;
  }

  bool result = false;

  uint32_t maxTS = millis() + timeMS;
  uint16_t index = 0;
  uint16_t promptLen = prompt.length();
  
  while (millis()  < maxTS) {
    if (_dataStream->available()) {
      char c = _dataStream->read();
      diagPrint(c);

      if (c == prompt[index]) {
        index++;

        if (index == promptLen) {
          result = true;
          break;
        }
      }
      else {
        index = 0;
      }
    }
    else {
      _delay(10);
    }
  }

  return result; 
}

inline void Sodaq_WifiBee::send(const String data)
{
  if (_dataStream) {
    _dataStream->print(data);
  }
}

inline void Sodaq_WifiBee::sendChar(const char data)
{
  if (_dataStream) {
    _dataStream->print(data);
  }
}

void Sodaq_WifiBee::sendEscaped(const String data)
{
  size_t length = data.length();

  //Todo add other lua escape characters?
  for (size_t i = 0; i < length; i++) {
    switch (data[i]) {
      case '\r': send("\\r");
        break;
      case '\n': send("\\n");
        break;
      default: sendChar(data[i]);
    }
  }
}

void Sodaq_WifiBee::sendBinary(const uint8_t* data, const size_t length)
{
  if (_dataStream) {
    _dataStream->write(data, length);
  }
}

bool Sodaq_WifiBee::sendWaitForPrompt(const String data, const String prompt,
  const uint32_t timeMS)
{
  flushInputStream();
  send(data);
  send("\r\n");
  return readTillPrompt(prompt, timeMS);
}

bool Sodaq_WifiBee::openConnection(const String server, const uint16_t port,
  const String type)
{
  on();

  bool result = false;
  
  if (connect()) {
    String data;

    //Create the connection object
    data = String("wifiConn=net.createConnection(") + type + ", false)";
    sendWaitForPrompt(data, LUA_PROMPT, RESPONSE_TIMEOUT);

    //Setup the callbacks
    data = String("wifiConn:on(\"connection\", ") + CONNECT_CALLBACK + ")";
    sendWaitForPrompt(data, LUA_PROMPT, RESPONSE_TIMEOUT);

    data = String("wifiConn:on(\"reconnection\", ") + RECONNECT_CALLBACK + ")";
    sendWaitForPrompt(data, LUA_PROMPT, RESPONSE_TIMEOUT);

    data = String("wifiConn:on(\"disconnection\", ") + DISCONNECT_CALLBACK + ")";
    sendWaitForPrompt(data, LUA_PROMPT, RESPONSE_TIMEOUT);

    data = String("wifiConn:on(\"sent\", ") + SENT_CALLBACK + ")";
    sendWaitForPrompt(data, LUA_PROMPT, RESPONSE_TIMEOUT);

    data = String("wifiConn:on(\"receive\", ") + RECEIVED_CALLBACK + ")";
    sendWaitForPrompt(data, LUA_PROMPT, RESPONSE_TIMEOUT);

    //Open the connection to the server
    data = String("wifiConn:connect(") + port + ",\"" + server + "\")";
    result = sendWaitForPrompt(data, CONNECT_PROMPT, SERVER_CONNECT_TIMEOUT);
  }

  return result;
}

bool Sodaq_WifiBee::sendAsciiData(const String data)
{
  send("wifiConn:send(\"");
  sendEscaped(data);
  send("\")\r\n");

  return readTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
}

bool Sodaq_WifiBee::sendBinaryData(const uint8_t* data, const size_t length)
{
  send("wifiConn:send(\"");
  sendBinary(data, length);
  send("\")\r\n");

  return readTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
}

bool Sodaq_WifiBee::closeConnection()
{
  String data;

  data = String("wifiConn:close()");
  bool result = sendWaitForPrompt(data, DISCONNECT_PROMPT, SERVER_DISCONNECT_TIMEOUT);

  off();

  return result;
}

bool Sodaq_WifiBee::connect()
{
  String data;

  data = String("wifi.setmode(wifi.STATION)");
  sendWaitForPrompt(data, LUA_PROMPT, RESPONSE_TIMEOUT);

  data = String("wifi.sta.config(\"") + _APN + "\",\"" + _password + "\")";
  sendWaitForPrompt(data, LUA_PROMPT, RESPONSE_TIMEOUT);

  data = String("wifi.sta.connect()");
  sendWaitForPrompt(data, LUA_PROMPT, RESPONSE_TIMEOUT);

  return waitForIP(WIFI_CONNECT_TIMEOUT);
}

bool Sodaq_WifiBee::disconnect()
{
  String data;

  data = String("wifi.sta.disconnect()");
  return sendWaitForPrompt(data, LUA_PROMPT, RESPONSE_TIMEOUT);
}

bool Sodaq_WifiBee::getStatus(uint8_t& status)
{
  bool result = false;
  String data;

  data = String("print(\"") + STATUS_CALLBACK + "\" .. wifi.sta.status())";

  if (sendWaitForPrompt(data, STATUS_PROMPT, RESPONSE_TIMEOUT)) {
    char statusCode;    
    if (readChar(statusCode, RESPONSE_TIMEOUT)) {
      if ((statusCode > 47) && (statusCode < 54)) { //characters 0..5
        status = statusCode - 48;
        result = true;
      }
    }
  }
  flushInputStream();

  return result;
}

bool Sodaq_WifiBee::waitForIP(const uint32_t timeMS)
{
  bool result = false;

  uint8_t status = 1;
  uint32_t maxTS = millis() + timeMS;

  while ((millis() < maxTS) && (status == 1)) {
    _delay(10);
    getStatus(status);
  }

  //0 = Idle
  //1 = Connecting
  //2 = Wrong Credentials
  //3 = AP not found
  //4 = Connect Fail
  //5 = Got IP

  switch(status) {
    case 0: diagPrintLn("Failed to connect: Station idle");
      break;
    case 1: diagPrintLn("Failed to connect: Timeout");
      break;
    case 2: diagPrintLn("Failed to connect: Wrong credentials");
      break;
    case 3: diagPrintLn("Failed to connect: AP not found");
      break;
    case 4: diagPrintLn("Failed to connect: Connection failed");
      break;
    case 5: diagPrintLn("Success: IP received");
      result = true;
      break;
  }

  return result;
}

inline void Sodaq_WifiBee::_delay(uint32_t ms)
{
  delay(ms);
}