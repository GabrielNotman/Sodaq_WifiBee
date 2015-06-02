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

//Other constants
#define RESPONSE_TIMEOUT 2000
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

void Sodaq_WifiBee::init(HardwareSerial& stream, const uint32_t baudrate)
{
  _dataStream = &stream;
  _dataStream->begin(DEFAULT_BAUD);

  //Pull bee CTS low
  pinMode(BEECTS, OUTPUT);
  digitalWrite(BEECTS, LOW);
  delay(2500);

  //We have to do this everytime as the settings persists until powered down
  //Send the command to change the rate on the bee
  String data = String("uart.setup(0,") + String(baudrate, DEC) + ",8,0,1,1)";
  send(data);
  readForTime(RESPONSE_TIMEOUT);
  delay(2500);

  //Start the port again at new rate
  _dataStream->begin(baudrate);

  //This is in case the Lua intpreter is confused
  sendWaitForPrompt("end", LUA_PROMPT);
  readForTime(RESPONSE_TIMEOUT);

  sleep();
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

void Sodaq_WifiBee::sleep()
{

}

void Sodaq_WifiBee::wake()
{

}

// HTTP methods
bool Sodaq_WifiBee::HTTPAction(const String server, const uint16_t port,
  const String method, const String location, const String headers,
  const String body, uint16_t& httpCode)
{
  return true;
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
  //Send command to dump 
  //Read size-1 or response size
  return true;
}

bool Sodaq_WifiBee::readHTTPResponse(uint8_t& buffer, const size_t size, 
  uint16_t& httpCode)
{
  //Send command to dump
  //Read until empty line
  //Save the rest until buffer is full or response is complete
  return true;
}

// Private methods
void Sodaq_WifiBee::flushInputStream()
{
  if (_dataStream) {
    while (_dataStream->available()) {
      diagPrint(_dataStream->read());
    }
  }
}

void Sodaq_WifiBee::readForTime(const uint16_t timeMS)
{
  if (!_dataStream) {
    return;
  }

  uint16_t time = 0;
  while (time < timeMS) {
    if (_dataStream->available()) {
      char c = _dataStream->read();
      diagPrint(c);
    } else {
      time += 10;
      delay(10);
    }
  }
}

bool Sodaq_WifiBee::readChar(char& data, const uint16_t timeMS)
{
  if (!_dataStream) {
    return false;
  }

  bool result = false;

  uint16_t time = 0;
  while ((time < timeMS) && (!result)) {
    if (_dataStream->available()) {
      data = _dataStream->read();
      diagPrint(data);
      result = true;
    }
    else {
      time += 10;
      delay(10);
    }
  }
  
  return result;
}

bool Sodaq_WifiBee::readTillPrompt(const String prompt, const uint16_t timeMS)
{
  if (!_dataStream) {
    return false;
  }

  bool result = false;

  uint16_t time = 0;
  uint16_t index = 0;
  uint16_t promptLen = prompt.length();
  
  while (time < timeMS) {
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
      time += 10;
      delay(10);
    }
  }

  return result; 
}

void Sodaq_WifiBee::send(const String data)
{
  if (_dataStream) {
    _dataStream->println(data);
  }
}

bool Sodaq_WifiBee::sendWaitForPrompt(const String data, const String prompt)
{
  send(data);
  return readTillPrompt(prompt, RESPONSE_TIMEOUT);
}

bool Sodaq_WifiBee::openConnection(const String server, const uint16_t port,
  const String type)
{
  wake();
  connect();

  String data;

  //Create the connection object
  data = String("wifiConn=net.createConnection(") + type + ", false)";
  sendWaitForPrompt(data, LUA_PROMPT);

  //Setup the callbacks
  data = String("wifiConn:on(\"connection\", ") + CONNECT_CALLBACK + ")";
  sendWaitForPrompt(data, LUA_PROMPT);

  data = String("wifiConn:on(\"reconnection\", ") + RECONNECT_CALLBACK + ")";
  sendWaitForPrompt(data, LUA_PROMPT);

  data = String("wifiConn:on(\"disconnection\", ") + DISCONNECT_CALLBACK + ")";
  sendWaitForPrompt(data, LUA_PROMPT);

  data = String("wifiConn:on(\"sent\", ") + SENT_CALLBACK + ")";
  sendWaitForPrompt(data, LUA_PROMPT);

  data = String("wifiConn:on(\"receive\", ") + RECEIVED_CALLBACK + ")";
  sendWaitForPrompt(data, LUA_PROMPT);

  //Open the connection to the server
  data = String("wifiConn:connect(") + port + ",\"" + server + "\")";
  return sendWaitForPrompt(data, CONNECT_PROMPT);
}

bool Sodaq_WifiBee::sendAsciiData(const String data)
{
  if (!_dataStream) {
    return false;
  }

  String output = escapeString(data);

  _dataStream->print("wifiConn:send(\"");
  _dataStream->write((uint8_t*)output.c_str(), output.length());
  _dataStream->println("\")");

  return readTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
}

bool Sodaq_WifiBee::sendBinaryData(const uint8_t* data, const size_t length)
{
  if (!_dataStream) {
    return false;
  }

  _dataStream->print("wifiConn:send(\"");
  _dataStream->write(data, length);
  _dataStream->println("\")");

  return readTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
}

bool Sodaq_WifiBee::closeConnection()
{
  String data;

  data = String("wifiConn:close()");
  bool result = sendWaitForPrompt(data, DISCONNECT_PROMPT);

  sleep();

  return result;
}

bool Sodaq_WifiBee::connect()
{
  String data;

  data = String("wifi.setmode(wifi.STATION)");
  sendWaitForPrompt(data, LUA_PROMPT);

  data = String("wifi.sta.config(\"") + _APN + "\",\"" + _password + "\")";
  sendWaitForPrompt(data, LUA_PROMPT);

  data = String("wifi.sta.connect()");
  return sendWaitForPrompt(data, LUA_PROMPT);
}

bool Sodaq_WifiBee::disconnect()
{
  String data;

  data = String("wifi.sta.disconnect()");
  return sendWaitForPrompt(data, LUA_PROMPT);
}

bool Sodaq_WifiBee::getStatus(uint8_t& status)
{
  bool result = false;
  String data;
  
  data = String("print(\"") + STATUS_CALLBACK + "\" .. wifi.sta.status())";

  if (sendWaitForPrompt(data, STATUS_PROMPT)) {
    char statusCode;    
    if (readChar(statusCode, RESPONSE_TIMEOUT)) {
      if ((statusCode > 47) && (statusCode < 54)) { //characters 0..5
        status = statusCode - 48;
        result = true;
      }
    }
  }

  return result;
}

String Sodaq_WifiBee::escapeString(const String input)
{
  String output;
  size_t length = input.length();

  //Todo add other lua escape characters?
  for (int i = 0; i < length; i++) {
    switch (input[i]) {
      case '\r': output += "\\r";
        break;
      case '\n': output += "\\n"; 
        break;
      default: output += (char)input[i];
    }
  }

  return output;
}
