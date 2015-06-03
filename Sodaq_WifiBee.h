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

#include <Arduino.h>
#include <Stream.h>

class Sodaq_WifiBee
{
public:
  Sodaq_WifiBee();
  void init(HardwareSerial& stream, const uint8_t dtrPin);
  void connectionSettings(const String APN, const String username, const String password);
  void setDiag(Stream& stream);

  void sleep();
  void wake();

  // HTTP methods
  // These use HTTP/1.1 and add headers for HOST (all) and Content-Length (except HTTPGet())
  bool HTTPAction(const String server, const uint16_t port,
    const String method, const String location, const String headers,
    const String body, uint16_t& httpCode);

  bool HTTPGet(const String server, const uint16_t port,
    const String location, const String headers, uint16_t& httpCode);

  bool HTTPPost(const String server, const uint16_t port,
    const String location, const String headers, const String body,
    uint16_t& httpCode);

  // TCP methods
  bool openTCP(const String server, uint16_t port);
  bool sendTCPAscii(const String data);
  bool sendTCPBinary(const uint8_t* data, const size_t length);
  bool closeTCP();

  // UDP methods
  bool openUDP(const String server, uint16_t port);
  bool sendUDPAscii(const String data);
  bool sendUDPBinary(const uint8_t* data, const size_t length);
  bool closeUDP();

  // Read back
  bool readResponse(uint8_t& buffer, const size_t size);
  bool readHTTPResponse(uint8_t& buffer, const size_t size, 
    uint16_t& httpCode);

private:
  String _APN;
  String _username;
  String _password;

  Stream* _dataStream;
  Stream* _diagStream;
  uint8_t _dtrPin;

  void flushInputStream();
  int readForTime(const uint32_t timeMS);
  bool readChar(char& data, const uint32_t timeMS);
  bool readTillPrompt(const String prompt, const uint32_t timeMS);
  
  void send(const String data);
  bool sendWaitForPrompt(const String data, const String prompt,
    const uint32_t timeMS);

  bool openConnection(const String server, const uint16_t port,
    const String type);
  
  bool sendAsciiData(const String data);
  bool sendBinaryData(const uint8_t* data, const size_t length);
  bool closeConnection();

  bool connect();
  bool disconnect();

  bool getStatus(uint8_t& status);
  bool waitForIP(const uint32_t timeMS);

  void escapeString(const String input, String& output);
  inline void _delay(uint32_t ms);
};

#endif // SODAQ_WIFI_BEE_H_
