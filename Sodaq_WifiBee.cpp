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
#define CONNECT_PROMPT "|C|"
#define RECONNECT_PROMPT "|RC|"
#define DISCONNECT_PROMPT "|DC|"
#define SENT_PROMPT "|DS|"
#define RECEIVED_PROMPT "|DR|"
#define STATUS_PROMPT "|STS|"

// Lua connection callback scripts
#define CONNECT_CALLBACK "function(s) print(\"|C|\") end"
#define RECONNECT_CALLBACK "function(s) print(\"|RC|\") end"
#define DISCONNECT_CALLBACK "function(s) print(\"|DC|\") end"
#define SENT_CALLBACK "function(s) print(\"|DS|\") end"
#define RECEIVED_CALLBACK "function(s, d) file.open(\"last.dat\", \"w+\") file.write(d) file.flush() file.close() print(\"|DR|\" .. string.sub(d,10,13)) end"
#define STATUS_CALLBACK "print(\"|\" .. \"STS|\" .. wifi.sta.status() .. \"|\")"

// Timeout constants
#define RESPONSE_TIMEOUT 2000
#define WIFI_CONNECT_TIMEOUT 4000
#define SERVER_CONNECT_TIMEOUT 5000
#define SERVER_DISCONNECT_TIMEOUT 2000
#define READBACK_TIMEOUT 2500
#define WAKE_DELAY 1000
#define STATUS_DELAY 100

Sodaq_WifiBee::Sodaq_WifiBee()
{
  _APN = "";
  _username = "";
  _password = "";

  _dataStream = NULL;
  _diagStream = NULL;
}

void Sodaq_WifiBee::init(Stream& stream, const uint8_t dtrPin)
{
  _dataStream = &stream;
  _dtrPin = dtrPin;

  pinMode(_dtrPin, OUTPUT);

  on();

  diagPrintLn("\r\nDeleting old data..\r\n");
  println("file.remove(\"last.dat\")");
  skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);

  off();
}

void Sodaq_WifiBee::connectionSettings(const String APN, const String username,
    const String password)
{
  connectionSettings(APN.c_str(), username.c_str(), password.c_str());
}

void Sodaq_WifiBee::connectionSettings(const char* APN, const char* username,
    const char* password)
{
  _APN = APN;
  _username = username;
  _password = password;
}

void Sodaq_WifiBee::setDiag(Stream& stream)
{
  _diagStream = &stream;
}

const char* Sodaq_WifiBee::getDeviceType()
{
  return "WifiBee";
}

void Sodaq_WifiBee::on()
{
  diagPrintLn("\r\nPower ON");
  digitalWrite(_dtrPin, LOW);
  skipForTime(WAKE_DELAY);
}

void Sodaq_WifiBee::off()
{
  diagPrintLn("\r\nPower OFF");
  digitalWrite(_dtrPin, HIGH);
}

// HTTP methods
bool Sodaq_WifiBee::HTTPAction(const String server, const uint16_t port,
    const String method, const String location, const String headers,
    const String body, uint16_t& httpCode)
{
  HTTPAction(server.c_str(), port, method.c_str(), location.c_str(),
      headers.c_str(), body.c_str(), httpCode);
}

bool Sodaq_WifiBee::HTTPAction(const char* server, const uint16_t port,
    const char* method, const char* location, const char* headers,
    const char* body, uint16_t& httpCode)
{
  bool result;

  // Open the connection
  result = openConnection(server, port, "net.TCP");

  if (result) {
    print("wifiConn:send(\"");

    print(method);
    print(" ");
    print(location);
    print(" HTTP/1.1\\r\\n");

    print("HOST: ");
    print(server);
    print(":");
    print(port);
    print("\\r\\n");

    print("Content-Length: ");
    print(strlen(body));
    print("\\r\\n");

    sendEscapedAscii(headers);
    print("\\r\\n\\r\\n");

    sendEscapedAscii(body);

    println("\")");
  }

  // Wait till we hear that it was sent
  if (result) {
    result = skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
  }

  // Wait till we get the data received prompt
  if (result) {
    result = skipTillPrompt(RECEIVED_PROMPT, RESPONSE_TIMEOUT);
  }

  // Attempt to read the response code  
  if (result) {
    result = parseHTTPResponse(httpCode);
  }

  // The connection might have closed automatically
  // Or it failed to open
  closeConnection();

  return result;
}

bool Sodaq_WifiBee::HTTPGet(const String server, const uint16_t port,
    const String location, const String headers, uint16_t& httpCode)
{
  HTTPGet(server.c_str(), port, location.c_str(), headers.c_str(), httpCode);
}

bool Sodaq_WifiBee::HTTPGet(const char* server, const uint16_t port,
    const char* location, const char* headers, uint16_t& httpCode)
{
  bool result;

  // Open the connection
  result = openConnection(server, port, "net.TCP");

  if (result) {
    print("wifiConn:send(\"");

    print("GET ");
    print(location);
    print(" HTTP/1.1\\r\\n");

    print("HOST: ");
    print(server);
    print(":");
    print(port);
    print("\\r\\n");

    sendEscapedAscii(headers);
    print("\\r\\n\\r\\n");

    println("\")");
  }

  // Wait till we hear that it was sent
  if (result) {
    result = skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
  }

  // Wait till we get the data received prompt
  if (result) {
    result = skipTillPrompt(RECEIVED_PROMPT, RESPONSE_TIMEOUT);
  }

  // Attempt to read the response code  
  if (result) {
    result = parseHTTPResponse(httpCode);
  }

  // The connection might have closed automatically
  // Or it failed to open
  closeConnection();

  return result;
}

bool Sodaq_WifiBee::HTTPPost(const String server, const uint16_t port,
    const String location, const String headers, const String body,
    uint16_t& httpCode)
{
  HTTPPost(server.c_str(), port, location.c_str(), headers.c_str(),
      body.c_str(), httpCode);
}

bool Sodaq_WifiBee::HTTPPost(const char* server, const uint16_t port,
    const char* location, const char* headers, const char* body,
    uint16_t& httpCode)
{
  bool result;

  // Open the connection
  result = openConnection(server, port, "net.TCP");

  if (result) {
    print("wifiConn:send(\"");

    print("POST ");
    print(location);
    print(" HTTP/1.1\\r\\n");

    print("HOST: ");
    print(server);
    print(":");
    print(port);
    print("\\r\\n");

    print("Content-Length: ");
    print(strlen(body));
    print("\\r\\n");

    sendEscapedAscii(headers);
    print("\\r\\n\\r\\n");

    sendEscapedAscii(body);

    println("\")");
  }

  // Wait till we hear that it was sent
  if (result) {
    result = skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
  }

  // Wait till we get the data received prompt
  if (result) {
    result = skipTillPrompt(RECEIVED_PROMPT, RESPONSE_TIMEOUT);
  }

  // Attempt to read the response code  
  if (result) {
    result = parseHTTPResponse(httpCode);
  }

  // The connection might have closed automatically
  // Or it failed to open
  closeConnection();

  return result;
}

// TCP methods
bool Sodaq_WifiBee::openTCP(const String server, uint16_t port)
{
  return openTCP(server.c_str(), port);
}

bool Sodaq_WifiBee::openTCP(const char* server, uint16_t port)
{
  return openConnection(server, port, "net.TCP");
}

bool Sodaq_WifiBee::sendTCPAscii(const String data)
{
  return transmitAsciiData(data.c_str());
}

bool Sodaq_WifiBee::sendTCPAscii(const char* data)
{
  return transmitAsciiData(data);
}

bool Sodaq_WifiBee::sendTCPBinary(const uint8_t* data, const size_t length)
{
  return transmitBinaryData(data, length);
}

bool Sodaq_WifiBee::closeTCP()
{
  return closeConnection();
}

// UDP methods
bool Sodaq_WifiBee::openUDP(const String server, uint16_t port)
{
  openUDP(server.c_str(), port);
}

bool Sodaq_WifiBee::openUDP(const char* server, uint16_t port)
{
  return openConnection(server, port, "net.UDP");
}

bool Sodaq_WifiBee::sendUDPAscii(const String data)
{
  return transmitAsciiData(data.c_str());
}

bool Sodaq_WifiBee::sendUDPAscii(const char* data)
{
  return transmitAsciiData(data);
}

bool Sodaq_WifiBee::sendUDPBinary(const uint8_t* data, const size_t length)
{
  return transmitBinaryData(data, length);
}

bool Sodaq_WifiBee::closeUDP()
{
  return closeConnection();
}

bool Sodaq_WifiBee::readResponseAscii(char* buffer, const size_t size, size_t& bytesRead)
{
  on();

  bool result;

  println("file.open(\"last.dat\", \"r+\")");
  result = skipTillPrompt("> ", RESPONSE_TIMEOUT);

  if (result) {
    println("uart.write(0, \"|\" .. \"SOF|\\r\\n\" .. file.read() .. \"|EOF|\")");
    result = skipTillPrompt("|SOF|\r\n", RESPONSE_TIMEOUT);
  }

  if (result) {
    result = readTillPrompt((uint8_t*)buffer, size - 1, bytesRead, "|EOF|",
      READBACK_TIMEOUT);
    buffer[bytesRead] = '\0';
  }

  off();
  return result;
}

bool Sodaq_WifiBee::readResponseBinary(uint8_t* buffer, const size_t size, size_t& bytesRead)
{
  on();

  bool result;

  println("file.open(\"last.dat\", \"r+\")");
  result = skipTillPrompt("> ", RESPONSE_TIMEOUT);

  if (result) {
    println("uart.write(0, \"|\" .. \"SOF|\\r\\n\" .. file.read() .. \"|EOF|\")");
    result = skipTillPrompt("|SOF|\r\n", RESPONSE_TIMEOUT);
  }

  if (result) {
    result = readTillPrompt(buffer, size, bytesRead, "|EOF|",
        READBACK_TIMEOUT);
  }

  off();
  return result;
}

bool Sodaq_WifiBee::readHTTPResponse(char* buffer, const size_t size,
    size_t& bytesRead,uint16_t& httpCode)
{
  on();

  bool result;

  println("file.open(\"last.dat\", \"r+\")");
  result = skipTillPrompt("> ", RESPONSE_TIMEOUT);

  if (result) {
    println("uart.write(0, \"|\" .. \"SOF|\\r\\n\" .. file.read() .. \"|EOF|\")");
    result = skipTillPrompt("|SOF|\r\n", RESPONSE_TIMEOUT);
  }

  if (result) {
    skipTillPrompt(" ", READBACK_TIMEOUT);
    result = parseHTTPResponse(httpCode);
  }

  if (result) {
    result = skipTillPrompt("\r\n\r\n", READBACK_TIMEOUT);
  }

  if (result) {
    result = readTillPrompt((uint8_t*)buffer, size - 1, bytesRead, "|EOF|",
        READBACK_TIMEOUT);
    buffer[bytesRead] = '\0';
  }

  off();
  return result;
}

// Stream implementations
size_t Sodaq_WifiBee::write(uint8_t x)
{ 
  if (_dataStream) {
    return _dataStream->write(x);
  } else {
    return 0;
  }
}

int Sodaq_WifiBee::available()
{
  if (_dataStream) {
    return _dataStream->available();
  } else {
    return -1;
  }
}

int Sodaq_WifiBee::peek()
{ 
  if (_dataStream) {
    return _dataStream->peek();
  } else {
    return -1;
  }
}

int Sodaq_WifiBee::read()
{ 
  if (_dataStream) {
    return _dataStream->read();
  } else {
    return -1;
  }
}

void Sodaq_WifiBee::flush() {
  if (_dataStream) {
    _dataStream->flush();
  }
}

// Private methods
void Sodaq_WifiBee::flushInputStream()
{
  if (_dataStream) {
    while (_dataStream->available()) {
      diagPrint((char )_dataStream->read());
    }
  }
}

int Sodaq_WifiBee::skipForTime(const uint32_t timeMS)
{
  if (!_dataStream) {
    return 0;
  }

  int count = 0;
  uint32_t maxTS = millis() + timeMS;

  while (millis() < maxTS) {
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

bool Sodaq_WifiBee::skipTillPrompt(const char* prompt, const uint32_t timeMS)
{
  if (!_dataStream) {
    return false;
  }

  bool result = false;

  uint32_t maxTS = millis() + timeMS;
  size_t index = 0;
  size_t promptLen = strlen(prompt);

  while (millis() < maxTS) {
    if (_dataStream->available()) {
      char c = _dataStream->read();
      diagPrint(c);

      if (c == prompt[index]) {
        index++;

        if (index == promptLen) {
          result = true;
          break;
        }
      } else {
        index = 0;
      }
    } else {
      _delay(10);
    }
  }

  return result;
}

bool Sodaq_WifiBee::readChar(char& data, const uint32_t timeMS)
{
  if (!_dataStream) {
    return false;
  }

  bool result = false;

  uint32_t maxTS = millis() + timeMS;
  while ((millis() < maxTS) && (!result)) {
    if (_dataStream->available()) {
      data = _dataStream->read();
      diagPrint(data);
      result = true;
    }
    else {
      _delay(10);
    }
  }

  return result;
}

bool Sodaq_WifiBee::readTillPrompt(uint8_t* buffer, const size_t size,
    size_t& bytesStored, const char* prompt, const uint32_t timeMS)
{
  if (!_dataStream) {
    return false;
  }

  bool result = false;

  uint32_t maxTS = millis() + timeMS;
  size_t promptIndex = 0;
  size_t promptLen = strlen(prompt);

  size_t bufferIndex = 0;
  size_t streamCount = 0;
  

  while (millis() < maxTS) {
    if (_dataStream->available()) {
      char c = _dataStream->read();
      diagPrint(c);

      streamCount++;

      if (bufferIndex < size) {
        buffer[bufferIndex] = c;
        bufferIndex++;
      }

      if (c == prompt[promptIndex]) {
        promptIndex++;

        if (promptIndex == promptLen) {
          result = true;
          bufferIndex = min(size - 1, streamCount - promptLen);
          break;
        }
      } else {
        promptIndex = 0;
      }
    } else {
      _delay(10);
    }
  }

  bytesStored = bufferIndex;

  return result;
}

void Sodaq_WifiBee::sendEscapedAscii(const char* data)
{
  size_t length = strlen(data);

  //Todo add other lua escape characters?
  for (size_t i = 0; i < length; i++) {
    switch (data[i]) {
    case '\a':
      print("\\a");
      break;
    case '\b':
      print("\\b");
      break;
    case '\f':
      print("\\f");
      break;
    case '\n':
      print("\\n");
      break;
    case '\r':
      print("\\r");
      break;
    case '\t':
      print("\\t");
      break;
    case '\v':
      print("\\v");
      break;
    case '\\':
      print("\\\\");
      break;
    case '\"':
      print("\\\"");
      break;
    case '\'':
      print("\\\'");
      break;
    case '[':
      print("\\[");
      break;
    case ']':
      print("\\]");
      break;
    default:
      print(data[i]);
      break;
    }
  }
}

void Sodaq_WifiBee::sendEscapedBinary(const uint8_t* data, const size_t length)
{
  for (size_t i = 0; i < length; i++) {
    print("\\");
    print(data[i]);
  }
}

bool Sodaq_WifiBee::openConnection(const char* server, const uint16_t port,
    const char* type)
{
  on();

  bool result;

  result = connect();

  if (result) {
    String data;

    //Create the connection object
    print("wifiConn=net.createConnection(");
    print(type);
    println(", false)");
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);

    //Setup the callbacks
    print("wifiConn:on(\"connection\", ");
    print(CONNECT_CALLBACK);
    println(")");
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);

    print("wifiConn:on(\"reconnection\", ");
    print(RECONNECT_CALLBACK);
    println(")");
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);

    print("wifiConn:on(\"disconnection\", ");
    print(DISCONNECT_CALLBACK);
    println(")");
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);

    print("wifiConn:on(\"sent\", ");
    print(SENT_CALLBACK);
    println(")");
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);

    print("wifiConn:on(\"receive\", ");
    print(RECEIVED_CALLBACK);
    println(")");
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);

    print("wifiConn:connect(");
    print(port);
    print(",\"");
    print(server);
    println("\")");
    result = skipTillPrompt(CONNECT_PROMPT, SERVER_CONNECT_TIMEOUT);
  }

  return result;
}

bool Sodaq_WifiBee::closeConnection()
{
  bool result;
  println("wifiConn:close()");
  result = skipTillPrompt(DISCONNECT_PROMPT, SERVER_DISCONNECT_TIMEOUT);

  off();

  return result;
}

bool Sodaq_WifiBee::transmitAsciiData(const char* data)
{
  print("wifiConn:send(\"");
  sendEscapedAscii(data);
  println("\")");

  return skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
}

bool Sodaq_WifiBee::transmitBinaryData(const uint8_t* data, const size_t length)
{
  print("wifiConn:send(\"");
  sendEscapedBinary(data, length);
  println("\")");

  return skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
}

bool Sodaq_WifiBee::connect()
{
  println("wifi.setmode(wifi.STATION)");
  skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);

  print("wifi.sta.config(\"");
  print(_APN);
  print("\",\"");
  print(_password);
  println("\")");
  skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);

  println("wifi.sta.connect()");
  skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);

  return waitForIP(WIFI_CONNECT_TIMEOUT);
}

bool Sodaq_WifiBee::disconnect()
{
  println("wifi.sta.disconnect()");
  return skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);
}

bool Sodaq_WifiBee::getStatus(uint8_t& status)
{
  bool result;

  println(STATUS_CALLBACK);
  result = skipTillPrompt(STATUS_PROMPT, RESPONSE_TIMEOUT);

  char statusCode;

  if (result) {
    result = readChar(statusCode, RESPONSE_TIMEOUT);
  }

  if (result) {
    if ((statusCode >= '0') && (statusCode <= '5')) {
      status = statusCode - '0';
    } else {
      result = false;
    }
  }

  skipForTime(STATUS_DELAY);

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

  switch (status) {
  case 0:
    diagPrintLn("Failed to connect: Station idle");
    break;
  case 1:
    diagPrintLn("Failed to connect: Timeout");
    break;
  case 2:
    diagPrintLn("Failed to connect: Wrong credentials");
    break;
  case 3:
    diagPrintLn("Failed to connect: AP not found");
    break;
  case 4:
    diagPrintLn("Failed to connect: Connection failed");
    break;
  case 5:
    diagPrintLn("Success: IP received");
    result = true;
    break;
  }

  return result;
}

bool Sodaq_WifiBee::parseHTTPResponse(uint16_t& httpCode)
{
  bool result;

  uint8_t buffer[4];
  size_t stored;
  result = readTillPrompt(buffer, 3, stored, " ", RESPONSE_TIMEOUT);
  buffer[3] = '\0';

  if (result) {
    httpCode = atoi((char*) buffer);
  }

  return result;
}

inline void Sodaq_WifiBee::_delay(uint32_t ms)
{
  delay(ms);
}
