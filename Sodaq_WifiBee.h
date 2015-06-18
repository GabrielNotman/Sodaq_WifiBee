/*
* Copyright (c) 2015 Gabriel Notman & M2M4ALL BV.  All rights reserved.
*
* This file is part of Sodaq_WifiBee.
*
* Sodaq_WifiBee is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation, either version 3 of
* the License, or(at your option) any later version.
*
* Sodaq_WifiBee is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with GPRSbee.  If not, see
* <http://www.gnu.org/licenses/>.
*/

#ifndef SODAQ_WIFI_BEE_H_
#define SODAQ_WIFI_BEE_H_

#include <Arduino.h>
#include <Stream.h>

class Sodaq_WifiBee : public Stream
{
public:
  Sodaq_WifiBee();
  virtual ~Sodaq_WifiBee();

  void init(Stream& stream, const uint8_t dtrPin, 
    const size_t bufferSize);

  void connectionSettings(const char* APN, const char* username,
      const char* password);

  void connectionSettings(const String& APN, const String& username,
    const String& password);

  void setDiag(Stream& stream);

  const char* getDeviceType();

  void on();

  void off();

  // HTTP methods
  // These use HTTP/1.1 and add headers for HOST (all)
  // and Content-Length (except HTTPGet())
  bool HTTPAction(const char* server, const uint16_t port, const char* method,
      const char* location, const char* headers, const char* body,
      uint16_t& httpCode);

  bool HTTPAction(const String& server, const uint16_t port, const String& method,
    const String& location, const String& headers, const String& body,
    uint16_t& httpCode);

  bool HTTPGet(const char* server, const uint16_t port, const char* location,
      const char* headers, uint16_t& httpCode);

  bool HTTPGet(const String& server, const uint16_t port, const String& location,
    const String& headers, uint16_t& httpCode);

  bool HTTPPost(const char* server, const uint16_t port, const char* location,
      const char* headers, const char* body, uint16_t& httpCode);

  bool HTTPPost(const String& server, const uint16_t port, const String& location,
    const String& headers, const String& body, uint16_t& httpCode);

  // TCP methods
  bool openTCP(const char* server, uint16_t port);

  bool openTCP(const String& server, uint16_t port);

  bool sendTCPAscii(const char* data);

  bool sendTCPAscii(const String& data);

  bool sendTCPBinary(const uint8_t* data, const size_t length);

  bool closeTCP();

  // UDP methods
  bool openUDP(const char* server, uint16_t port);

  bool openUDP(const String& server, uint16_t port);

  bool sendUDPAscii(const char* data);

  bool sendUDPAscii(const String& data);

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
  String _APN;  /*!< The wifi network's SSID. */
  String _username;  /*!< Unused */
  String _password;  /*!< The password for the wifi network. */

  Stream* _dataStream;  /*!< A reference to the stream object used for communicating with the WifiBee. */
  Stream* _diagStream; /*!< A reference to an optional stream object used for debugging. */
  uint8_t _dtrPin;  /*!< The Bee socket's DTR pin. */

  size_t _bufferSize;  /*!< The allocated size of `_buffer`. */
  size_t _bufferUsed;  /*!< The current amount of `_buffer` which is in use. */
  uint8_t* _buffer;  /*!< The buffer used to store received data. */

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

  void disconnect();

  bool getStatus(uint8_t& status);

  bool waitForIP(const uint32_t timeMS);

  bool parseHTTPResponse(uint16_t& httpCode);

  inline void clearBuffer();

  inline void _delay(uint32_t ms);
};

#endif // SODAQ_WIFI_BEE_H_
