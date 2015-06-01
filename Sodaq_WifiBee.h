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

class Sodaq_WifiBee
{
public:
  Sodaq_WifiBee();
  void init(HardwareSerial& stream, const uint32_t baudrate);
  void connectionSettings(const String APN, const String username, const String password);
  void setDiag(Stream& stream);

  void sleep();
  void wake();

  // HTTP methods
  // These use HTTP/1.1 and add headers for HOST (all) and Content-Length (except HTTPGet())
  bool HTTPAction(const char* server, const uint16_t port,
      const char* method, const char* location, const char* headers,
      const char* body, uint16_t& httpCode);

  bool HTTPGet(const char* server, const uint16_t port,
    const char* location, const char* headers, uint16_t& httpCode);

  bool HTTPPost(const char* server, const uint16_t port,
    const char* location, const char* headers, const char* body, 
    uint16_t& httpCode);

  // TCP methods
  bool openTCP(const char* server, uint16_t port);
  bool sendTCPData(const uint8_t* data, const size_t length);
  bool closeTCP();

  // UDP methods
  bool openUDP(const char* server, uint16_t port);
  bool sendUDPData(const uint8_t* data, const size_t length);
  bool closeUDP();

  // Read back
  bool readResponse(uint8_t& buffer, const size_t size);
  bool readHTTPResponse(uint8_t& buffer, const size_t size, 
    uint16_t& httpCode);

private:
  String _APN;
  String _username;
  String _password;

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
