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

#ifndef SODAQ_WIFI_BEE_H_
#define SODAQ_WIFI_BEE_H_

//#include <stdint.h>
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Stream.h>

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

// Lua connection callback scripts
#define CONNECT_CALLBACK "function(socket) print(\".:Connected:.\") end"
#define RECONNECT_CALLBACK "function(socket) print(\".:Reconnected:.\") end"
#define DISCONNECT_CALLBACK "function(socket) print(\".:Disconnected:.\") end"
#define SENT_CALLBACK "function(socket) print(\".:Data Sent:.\") end"
#define RECEIVED_CALLBACK "function(socket, data) lastResponse=data lastSize=data:len() uart.write(0, \".:Data Received:.\\r\\n\", \"Length: \", tostring(data:len()), \"\\r\\n\", data, \"\\r\\n\") end"

//Other constants
#define RESPONSE_TIMEOUT 2000
#define DEFAULT_BAUD 9600

// Lua constants for each connection type
#define TCP_CONNECTION "net.TCP"
#define UDP_CONNECTION "net.UDP"

class Sodaq_WifiBee
{
public:
  Sodaq_WifiBee();
  void init(HardwareSerial& stream, const uint32_t baudrate);
  void connectionSettings(char* APN, char* username, char* password);
  void setDiag(Stream& stream);

  void sleep();
  void wake();

  // HTTP methods
  // These use HTTP/1.1 and add headers for HOST (all) and Content-Length (except HTTPGet())
  uint16_t HTTPAction(const char* server, const uint16_t port,
      const char* method, const char* location, const char* headers,
      const char* body);

  uint16_t HTTPGet(const char* server, const uint16_t port,
      const char* location, const char* headers);

  uint16_t HTTPPost(const char* server, const uint16_t port,
      const char* location, const char* headers, const char* body);

  // TCP methods
  bool openTCP(const char* server, uint16_t port);
  bool sendTCPData(const uint8_t* data, const size_t length);
  bool closeTCP();

  // UDP methods
  bool openUDP(const char* server, uint16_t port);
  bool sendUDPData(const uint8_t* data, const size_t length);
  bool closeUDP();

  // Read back
  void readResponse(uint8_t buffer, const size_t size);
  void readHTTPResponse(uint8_t buffer, const size_t size);

private:
  char* _APN;
  char* _username;
  char* _password;

  HardwareSerial* _dataStream;
  Stream* _diagStream;

  void flushInputStream();
  void readForTime(const uint16_t timeMS);
  bool readTillPrompt(const char* prompt, const uint16_t timeMS);

  void send(const char* data);
  bool sendWaitForPrompt(const char* data, const char* prompt);

  bool openConnection(const char* server, const uint16_t port,
      const char* type);
  
  bool sendData(const uint8_t* data, const size_t length);
  bool closeConnection();

  bool connect();
  bool disconnect();
};

#endif // SODAQ_WIFI_BEE_H_
