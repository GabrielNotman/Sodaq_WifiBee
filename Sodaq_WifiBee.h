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

class Sodaq_WifiBee : public Stream
{
public:
  Sodaq_WifiBee();
  ~Sodaq_WifiBee();

  void init(Stream& stream, const uint8_t dtrPin, 
    const size_t bufferSize);

  void connectionSettings(const String& APN, const String& username,
      const String& password);
  void connectionSettings(const char* APN, const char* username,
      const char* password);

  void setDiag(Stream& stream);

  const char* getDeviceType();

  void on();
  void off();

  // HTTP methods
  // These use HTTP/1.1 and add headers for HOST (all)
  // and Content-Length (except HTTPGet())
  bool HTTPAction(const String& server, const uint16_t port, const String& method,
      const String& location, const String& headers, const String& body,
      uint16_t& httpCode);

  bool HTTPAction(const char* server, const uint16_t port, const char* method,
      const char* location, const char* headers, const char* body,
      uint16_t& httpCode);

  bool HTTPGet(const String& server, const uint16_t port, const String& location,
      const String& headers, uint16_t& httpCode);

  bool HTTPGet(const char* server, const uint16_t port, const char* location,
      const char* headers, uint16_t& httpCode);

  bool HTTPPost(const String& server, const uint16_t port, const String& location,
      const String& headers, const String& body, uint16_t& httpCode);

  bool HTTPPost(const char* server, const uint16_t port, const char* location,
      const char* headers, const char* body, uint16_t& httpCode);

  // TCP methods
  bool openTCP(const String& server, uint16_t port);

  bool openTCP(const char* server, uint16_t port);

  bool sendTCPAscii(const String& data);

  bool sendTCPAscii(const char* data);

  bool sendTCPBinary(const uint8_t* data, const size_t length);

  bool closeTCP();

  // UDP methods
  bool openUDP(const String& server, uint16_t port);

  bool openUDP(const char* server, uint16_t port);

  bool sendUDPAscii(const String& data);

  bool sendUDPAscii(const char* data);

  bool sendUDPBinary(const uint8_t* data, const size_t length);

  bool closeUDP();

  // Read back
  bool readResponseAscii(char* buffer, const size_t size, size_t& bytesRead);

  bool readResponseBinary(uint8_t* buffer, const size_t size, size_t& bytesRead);

  bool readHTTPResponse(char* buffer, const size_t size, size_t& bytesRead, uint16_t& httpCode);

  // Stream implementations
  size_t write(uint8_t x);
  
  int available();
  
  int peek();

  int read();

  void flush();

private:
  String _APN;
  String _username;
  String _password;

  Stream* _dataStream;
  Stream* _diagStream;
  uint8_t _dtrPin;

  size_t _bufferSize;
  size_t _bufferUsed;
  uint8_t* _buffer;

  void flushInputStream();

  int skipForTime(const uint32_t timeMS);
    
  bool skipTillPrompt(const char* prompt, const uint32_t timeMS);

  bool readChar(char& data, const uint32_t timeMS);

  bool readTillPrompt(uint8_t* buffer, const size_t size, size_t& bytesStored,
      const char* prompt, const uint32_t timeMS);

  void sendEscapedAscii(const char* data);

  void sendEscapedBinary(const uint8_t* data, const size_t length);

  bool openConnection(const char* server, const uint16_t port,
      const char* type);

  bool closeConnection();

  bool transmitAsciiData(const char* data);

  bool transmitBinaryData(const uint8_t* data, const size_t length);

  bool readServerResponse();

  bool connect();

  bool disconnect();

  bool getStatus(uint8_t& status);

  bool waitForIP(const uint32_t timeMS);

  bool parseHTTPResponse(uint16_t& httpCode);

  inline void _delay(uint32_t ms);
};

#endif // SODAQ_WIFI_BEE_H_
