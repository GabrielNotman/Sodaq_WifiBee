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

#include "Sodaq_WifiBee.h"

#define ENABLE_RADIO_DIAG 1

#if ENABLE_RADIO_DIAG
#define diagPrint(...) { if (_diagStream) _diagStream->print(__VA_ARGS__); }
#define diagPrintLn(...) { if (_diagStream) _diagStream->println(__VA_ARGS__); }
#else
#define diagPrint(...)
#define diagPrintLn(...)
#endif

// Lua command size limit 
// Cannot be set to < 13
#define LUA_COMMAND_MAX 255

// Lua prompts
#define LUA_PROMPT "\r\n> "
#define OK_PROMPT "OK\r\n> "
#define CONNECT_PROMPT "|C|"
#define RECONNECT_PROMPT "|RC|"
#define DISCONNECT_PROMPT "|DC|"
#define SENT_PROMPT "|DS|"
#define RECEIVED_PROMPT "|DR|"
#define STATUS_PROMPT "|STS|"
#define SOF_PROMPT "|SOF|"
#define EOF_PROMPT "|EOF|" // Cannot start with a HEX character (0..9, A..F)

// Lua connection callback scripts
#define OK_COMMAND "uart.write(0, \"OK\\r\\n\")"
#define RECEIVED_CALLBACK "function(s, d) if lastData==nil then lastData=d end print(d:len()..\"|DR|\") end" // Max length 231
#define STATUS_CALLBACK "print(\"|\" .. \"STS|\" .. wifi.sta.status() .. \"|\")" // Max length 255
#define READ_BACK "uart.write(0, \"|\" .. \"SOF|\") for i=1, lastData:len(), 1 do uart.write(0, string.format(\"%02X\", lastData:byte(i))) tmr.wdclr() end lastData=nil uart.write(0, \"|EOF|\")" // Max length 255

// Timeout constants
#define RESPONSE_TIMEOUT 2000
#define WIFI_CONNECT_TIMEOUT 10000
#define SERVER_CONNECT_TIMEOUT 5000
#define SERVER_RESPONSE_TIMEOUT 5000
#define SERVER_DISCONNECT_TIMEOUT 2000
#define READBACK_TIMEOUT 2500
#define WAKE_DELAY 2000
#define STATUS_DELAY 1000
#define NEXT_PACKET_TIMEOUT 500

#define NIBBLE2BYTE(X) ((X >= 'A') ? X - 'A' + 10: X - '0')
#define HEX2BYTE(H, L) ((NIBBLE2BYTE(H) << 4) + NIBBLE2BYTE(L))

#define UINT_32_MAX 0xFFFFFFFF

// A specialized class to switch on/off the WifiBee module
// The VCC3.3 pin is switched by the Autonomo BEE_VCC pin
// The DTR pin is the actual ON/OFF pin, it is A13 on Autonomo, D20 on Tatu
class Sodaq_WifiBeeOnOff : public Sodaq_OnOffBee
{
public:
  Sodaq_WifiBeeOnOff();
  void init(int vcc33Pin, int onoffPin, int statusPin);
  void on();
  void off();
  bool isOn();
private:
  int8_t _vcc33Pin; /*!< The I/O pin to switch the 3V3 on or off. */
  int8_t _onoffPin; /*!< The I/O pin to switch the device on or off. */
  int8_t _statusPin; /*!< The I/O pin which indicates whether the device is on or off. */
};

static Sodaq_WifiBeeOnOff sodaq_wifibee_onoff;

/*!
* Initialises member variables to default values,
* including any pointers to NULL.
*/
Sodaq_WifiBee::Sodaq_WifiBee()
{
  _APN = "";
  _username = "";
  _password = "";

  _onoff = 0;

  _bufferSize = 0;
  _bufferUsed = 0;
  _buffer = NULL;

  _dataStream = NULL;
  _diagStream = NULL;
}

/*!
* Frees any memory allocated to the internal buffer.
*/
Sodaq_WifiBee::~Sodaq_WifiBee()
{
  if (_buffer) {
    free(_buffer);
  }
}

/*!
* This method initialises a Sodaq_WifiBee object.
* @param stream A reference to the stream object used for communicating with the WifiBee.
* @param vcc33Pin The I/O pin to switch the 3V3 on or off (-1 if not used).
* @param onoffPin The I/O pin to switch the device on or off.
* @param statusPin The I/O pin which indicates whether the device is on or off.
* @param bufferSize The amount of memory to allocate to the internal buffer.
*/
void Sodaq_WifiBee::init(Stream& stream, int vcc33Pin, int onoffPin, int statusPin,
  const size_t bufferSize)
{
  sodaq_wifibee_onoff.init(vcc33Pin, onoffPin, statusPin);
  _onoff = &sodaq_wifibee_onoff;

  _dataStream = &stream;

  _bufferSize = bufferSize;
  if (_buffer) {
    free(_buffer);
  }
  _buffer = (uint8_t*)malloc(_bufferSize);

  // TODO Do we want to do this here right now?
  off();
}

/*!
* This method sets the credentials for the Wifi network.
* @param APN The wifi network's SSID.
* @param username Unused.
* @param password The password for the wifi network.
*/
void Sodaq_WifiBee::connectionSettings(const char* APN, const char* username,
  const char* password)
{
  _APN = APN;
  _username = username;
  _password = password;
}

/*!
* \overload
*/
void Sodaq_WifiBee::connectionSettings(const String& APN, const String& username,
  const String& password)
{
  connectionSettings(APN.c_str(), username.c_str(), password.c_str());
}

/*!
* This method sets the stream object reference to use for debug/diagnostic purposes.
* @param stream The reference to the stream object.
*/
void Sodaq_WifiBee::setDiag(Stream& stream)
{
  _diagStream = &stream;
}

/*!
* This method can be used to identify the specific Bee module.
* @return The literal constant "WifiBee".
*/
const char* Sodaq_WifiBee::getDeviceType()
{
  return "WifiBee";
}

/*!
* This method switches the WifiBee on.
* It is called automatically, as required, by most methods.
* It attempts to call _onoff::on().
* @return `true` if the WifiBee is now on, `false` otherwise.
*/
bool Sodaq_WifiBee::on()
{
  diagPrintLn("\r\nPower ON");
  if (!isOn()) {
    if (_onoff) {
      _onoff->on();
    }
  }
  
  bool result = skipTillPrompt(LUA_PROMPT, WAKE_DELAY);
  // If it was already on, the above may have failed
  // so we try with the isAlive() method.
  if (!result) {
    result |= isAlive();
  }

  return result;
}

/*!
* This method switches the WifiBee off.
* It is called automatically, as required, by most methods.
* It attempts to call _onoff::off().
* @return `true` if the WifiBee is now off, `false` otherwise.
*/
bool Sodaq_WifiBee::off()
{
  diagPrintLn("\r\nPower OFF");

  // No matter if it is on or off, turn it off.
  if (_onoff) {
    _onoff->off();
  }

  // TODO _echoOff = false;
  return !isOn();
}

/*!
* This method checkes if the WifiBee is on by sending
* a status command which should return "OK".
* @return `true` if an "OK" response was received, `false` otherwise.
*/
bool Sodaq_WifiBee::isAlive()
{
  println(OK_COMMAND);
  return skipTillPrompt(OK_PROMPT, RESPONSE_TIMEOUT);
}

/*!
* This method replaces the default switching object for the WifiBee.
* @param onoff A reference to the new switching control object.
*/
void Sodaq_WifiBee::setOnOff(Sodaq_OnOffBee & onoff)
{ 
  _onoff = &onoff; 
}

/*!
*\overload
*/
void Sodaq_WifiBee::setOnOff(Sodaq_OnOffBee * onoff)
{ 
  _onoff = onoff; 
}

// HTTP methods
/*!
* This method constructs and sends a HTTP GET request.
* @param server The server/host to connect to (IP address or domain).
* @param port The port to connect to.
* @param URI The resource location on the server/host.
* @param headers Any additional headers, each must be followed by a CRLF.
* HOST header is added automatically.
* @param httpCode The HTTP response code is written to this parameter (if a response is received).
* @return `true` if a connection is established and the data is sent, `false` otherwise.
*/
bool Sodaq_WifiBee::HTTPGet(const char* server, const uint16_t port,
  const char* URI, const char* headers, uint16_t& httpCode)
{
  return HTTPAction(server, port, "GET", URI, headers, "", httpCode);
}

/*!
*\overload
*/
bool Sodaq_WifiBee::HTTPGet(const String& server, const uint16_t port,
  const String& URI, const String& headers, uint16_t& httpCode)
{
  return HTTPGet(server.c_str(), port, URI.c_str(), headers.c_str(), httpCode);
}

/*!
* This method constructs and sends a HTTP POST request.
* @param server The server/host to connect to (IP address or domain).
* @param port The port to connect to.
* @param URI The resource location on the server/host.
* @param headers Any additional headers, each must be followed by a CRLF.
* HOST & Content-Length headers are added automatically.
* @param body The body (can be blank) to send with the request. Must not start with a CRLF.
* @param httpCode The HTTP response code is written to this parameter (if a response is received).
* @return `true` if a connection is established and the data is sent, `false` otherwise.
*/
bool Sodaq_WifiBee::HTTPPost(const char* server, const uint16_t port,
  const char* URI, const char* headers, const char* body,
  uint16_t& httpCode)
{
  return HTTPAction(server, port, "POST", URI, headers, body, httpCode);
}

/*!
*\overload
*/
bool Sodaq_WifiBee::HTTPPost(const String& server, const uint16_t port,
  const String& URI, const String& headers, const String& body,
  uint16_t& httpCode)
{
  return HTTPPost(server.c_str(), port, URI.c_str(), headers.c_str(),
    body.c_str(), httpCode);
}

/*!
* This method constructs and sends a HTTP PUT request.
* @param server The server/host to connect to (IP address or domain).
* @param port The port to connect to.
* @param URI The resource location on the server/host.
* @param headers Any additional headers, each must be followed by a CRLF.
* HOST & Content-Length headers are added automatically.
* @param body The body (can be blank) to send with the request. Must not start with a CRLF.
* @param httpCode The HTTP response code is written to this parameter (if a response is received).
* @return `true` if a connection is established and the data is sent, `false` otherwise.
*/
bool Sodaq_WifiBee::HTTPPut(const char* server, const uint16_t port,
  const char* URI, const char* headers, const char* body,
  uint16_t& httpCode)
{
  return HTTPAction(server, port, "PUT", URI, headers, body, httpCode);
}

/*!
*\overload
*/
bool Sodaq_WifiBee::HTTPPut(const String& server, const uint16_t port,
  const String& URI, const String& headers, const String& body,
  uint16_t& httpCode)
{
  return HTTPPut(server.c_str(), port, URI.c_str(), headers.c_str(),
    body.c_str(), httpCode);
}

// TCP methods
/*!
* This method opens a TCP connection to a remote server.
* @param server The server/host to connect to (IP address or domain).
* @param port The port to connect to.
* @return `true` if the connection was successfully opened, otherwise `false`.
*/
bool Sodaq_WifiBee::openTCP(const char* server, uint16_t port)
{
  return openConnection(server, port, "net.TCP");
}

/*!
* \overload
*/
bool Sodaq_WifiBee::openTCP(const String& server, uint16_t port)
{
  return openTCP(server.c_str(), port);
}

/*!
* This method sends an ASCII chunk of data over an open TCP connection.
* @param data The buffer containing the data to be sent.
* @param waitForResponse Expect/wait for a reply from the server, default = true.
* @return `true` if the data was successfully sent, otherwise `false`.
*/
bool Sodaq_WifiBee::sendTCPAscii(const char* data, const bool waitForResponse)
{
  return transmitAsciiData(data, waitForResponse);
}

/*!
* \overload
*/
bool Sodaq_WifiBee::sendTCPAscii(const String& data, const bool waitForResponse)
{
  return sendTCPAscii(data.c_str(), waitForResponse);
}

/*!
* This method sends a binary chunk of data over an open TCP connection.
* @param data The buffer containing the data to be sent.
* @param length The number of bytes, contained in `data`, to send.
* @param waitForResponse Expect/wait for a reply from the server, default = true.
* @return `true` if the data was successfully sent, otherwise `false`.
*/
bool Sodaq_WifiBee::sendTCPBinary(const uint8_t* data, const size_t length, const bool waitForResponse)
{
  return transmitBinaryData(data, length, waitForResponse);
}

/*!
* This method closes an open TCP connection.
* @return `true` if the connection was closed, otherwise `false`.
* It will return `false` if the connection was already closed.
*/
bool Sodaq_WifiBee::closeTCP()
{
  return closeConnection();
}

// UDP methods
/*!
* This method opens a UDP connection to a remote server.
* @param server The server/host to connect to (IP address or domain).
* @param port The port to connect to.
* @return `true` if the connection was successfully opened, otherwise `false`.
*/
bool Sodaq_WifiBee::openUDP(const char* server, uint16_t port)
{
  return openConnection(server, port, "net.UDP");
}

/*!
* \overload
*/
bool Sodaq_WifiBee::openUDP(const String& server, uint16_t port)
{
  return openUDP(server.c_str(), port);
}

/*!
* This method sends an ASCII chunk of data over an open UDP connection.
* @param data The buffer containing the data to be sent.
* @param waitForResponse Expect/wait for a reply from the server, default = true.
* @return `true` if the data was successfully sent, otherwise `false`.
*/
bool Sodaq_WifiBee::sendUDPAscii(const char* data, const bool waitForResponse)
{
  return transmitAsciiData(data, waitForResponse);
}

/*!
* \overload
*/
bool Sodaq_WifiBee::sendUDPAscii(const String& data, const bool waitForResponse)
{
  return sendUDPAscii(data.c_str(), waitForResponse);
}

/*!
* This method sends a binary chunk of data over an open UDP connection.
* @param data The buffer containing the data to be sent.
* @param length The number of bytes, contained in `data`, to send.
* @param waitForResponse Expect/wait for a reply from the server, default = true.
* @return `true` if the data was successfully sent, otherwise `false`.
*/
bool Sodaq_WifiBee::sendUDPBinary(const uint8_t* data, const size_t length, const bool waitForResponse)
{
  return transmitBinaryData(data, length, waitForResponse);
}

/*!
* This method closes an open UDP connection.
* @return `true` if the connection was closed, otherwise `false`.
* It will return `false` if the connection was already closed.
*/
bool Sodaq_WifiBee::closeUDP()
{
  return closeConnection();
}

/*!
* This method copies the response data into a supplied buffer.
* The amount of data copied is limited by the size of the supplied buffer.
* Adds a terminating '\0'.
* @param buffer The buffer to copy the data into.
* @param size The size of `buffer`.
* @param bytesRead The number of bytes copied is written to this parameter.
* @return `false` if there is no data to copy, otherwise `true`.
*/
bool Sodaq_WifiBee::readResponseAscii(char* buffer, const size_t size, size_t& bytesRead)
{
  if (_bufferUsed == 0) {
    return false;
  }

  bytesRead = ((size - 1) < _bufferUsed) ? (size - 1) : _bufferUsed;

  memcpy(buffer, _buffer, bytesRead);
  buffer[bytesRead] = '\0';

  return true;
}

/*!
* This method copies the response data into a supplied buffer.
* The amount of data copied is limited by the size of the supplied buffer.
* Does not add a terminating '\0'.
* @param buffer The buffer to copy the data into.
* @param size The size of `buffer`.
* @param bytesRead The number of bytes copied is written to this parameter.
* @return `false` if there is no data to copy, otherwise `true`.
*/
bool Sodaq_WifiBee::readResponseBinary(uint8_t* buffer, const size_t size, size_t& bytesRead)
{
  if (_bufferUsed == 0) {
    return false;
  }

  bytesRead = (size < _bufferUsed) ? size : _bufferUsed;

  memcpy(buffer, _buffer, bytesRead);

  return true;
}

/*!
* This method copies the response data into a supplied buffer.
* It skips the response and header lines and only copies the response body.
* The amount of data copied is limited by the size of the supplied buffer.
* Adds a terminating '\0'.
* @param buffer The buffer to copy the data into.
* @param size The size of `buffer`.
* @param bytesRead The number of bytes copied is written to this parameter.
* @param httpCode The HTTP response code is written to this parameter.
* @return `false` if there is no data to copy, otherwise `true`.
* It will return `false` if the body is empty or cannot be determined.
*/
bool Sodaq_WifiBee::readHTTPResponse(char* buffer, const size_t size,
  size_t& bytesRead, uint16_t& httpCode)
{
  if (_bufferUsed == 0) {
    return false;
  }

  // Read HTTP response code
  parseHTTPResponse(httpCode);

  // Add 4 to start from after the double newline
  char* startPos = strstr((char*)_buffer, "\r\n\r\n") + 4;

  size_t startIndex = startPos - (char*)_buffer;

  if (startIndex < _bufferUsed) {
    bytesRead = ((size - 1) < (_bufferUsed - startIndex)) ? (size - 1) : (_bufferUsed - startIndex);
  }
  else {
    bytesRead = 0;
  }

  memcpy(buffer, startPos, bytesRead);
  buffer[bytesRead] = '\0';

  return true;
}

// Stream implementations
/*!
* Implementation of Stream::write(x) \n
* If `_dataStream != NULL` it calls `_dataStream->write(x)`.
* @param x Data to pass to `_dataStream->write(x)`.
* @return result of `_dataStream->write(x)` or 0 if `_dataStream == NULL`.
*/
size_t Sodaq_WifiBee::write(uint8_t x)
{
  if (_dataStream) {
    return _dataStream->write(x);
  }
  else {
    return 0;
  }
}

/*!
* Implementation of Stream::available() \n
* If `_dataStream != NULL` it calls `_dataStream->available()`.
* @return result of `_dataStream->available()` or 0 if `_dataStream == NULL`.
*/
int Sodaq_WifiBee::available()
{
  if (_dataStream) {
    return _dataStream->available();
  }
  else {
    return 0;
  }
}

/*!
* Implementation of Stream::peek() \n
* If `_dataStream != NULL` it calls `_dataStream->peek()`.
* @return result of `_dataStream->peek()` or -1 if `_dataStream == NULL`.
*/
int Sodaq_WifiBee::peek()
{
  if (_dataStream) {
    return _dataStream->peek();
  }
  else {
    return -1;
  }
}

/*!
* Implementation of Stream::read() \n
* If `_dataStream != NULL` it calls `_dataStream->read()`.
* @return result of `_dataStream->read()` or -1 if `_dataStream == NULL`.
*/
int Sodaq_WifiBee::read()
{
  if (_dataStream) {
    return _dataStream->read();
  }
  else {
    return -1;
  }
}

/*!
* Implementation of Stream::flush() \n
* If `_dataStream != NULL` it calls `_dataStream->flush()`.
*/
void Sodaq_WifiBee::flush() {
  if (_dataStream) {
    _dataStream->flush();
  }
}

// Private methods
/*!
* This method checkes if the WifiBee is on.
* It attempts to call _onoff::isOn().
* @return `true` if the WifiBee is on, `false` otherwise.
*/
bool Sodaq_WifiBee::isOn()
{
  if (_onoff) {
    return _onoff->isOn();
  }

  // No onoff. Let's assume it is on.
  return true;
}

/*!
* This method reads and empties the input buffer of `_dataStream`.
* It attempts to output the data it reads to `_diagStream`.
*/
void Sodaq_WifiBee::flushInputStream()
{
  while (available()) {
    diagPrint((char)read());
  }
}

/*!
* This method reads and empties the input buffer of `_dataStream`.
* It continues until the specified amount of time has elapsed.
* It attempts to output the data it reads to `_diagStream`.
* @param timeMS The time limit in milliseconds.
* @return The number of bytes it read.
*/
int Sodaq_WifiBee::skipForTime(const uint32_t timeMS)
{
  if (!_dataStream) {
    return 0;
  }

  int count = 0;
  uint32_t startTS = millis();

  while (!timedOut32(startTS, timeMS)) {
    if (available()) {
      char c = read();
      diagPrint(c);
      count++;
    }
    else {
      _delay(10);
    }
  }

  return count;
}

/*!
* This method reads and empties the input buffer of `_dataStream`.
* It continues until it finds the specified prompt or until
* the specified amount of time has elapsed.
* It attempts to output the data it reads to `_diagStream`.
* @param prompt The prompt to read until.
* @param timeMS The time limit in milliseconds.
* @return `true` if it found the specified prompt within the time
* limit, otherwise `false`.
*/
bool Sodaq_WifiBee::skipTillPrompt(const char* prompt, const uint32_t timeMS)
{
  if (!_dataStream) {
    return false;
  }

  bool result = false;

  uint32_t startTS = millis();

  size_t index = 0;
  size_t promptLen = strlen(prompt);

  while (!timedOut32(startTS, timeMS)) {
    if (available()) {
      char c = read();
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

/*!
* This method reads one character from the input buffer of `_dataStream`.
* It continues until it reads one character or until the specified amount
* of time has elapsed. It attempts to output the data it read to `_diagStream`.
* @param data The character read, is written to this parameter.
* @param timeMS The time limit in milliseconds.
* @return `true` if it successfully read one character within the
* time limit, otherwise `false`.
*/
bool Sodaq_WifiBee::readChar(char& data, const uint32_t timeMS)
{
  if (!_dataStream) {
    return false;
  }

  bool result = false;

  uint32_t startTS = millis();
  while ((!timedOut32(startTS, timeMS)) && (!result)) {
    if (available()) {
      data = read();
      diagPrint(data);
      result = true;
    }
    else {
      _delay(10);
    }
  }

  return result;
}

/*!
* This method reads and empties the input buffer of `_dataStream`.
* It continues until it finds the specified prompt or until
* the specified amount of time has elapsed.
* It copies the read data into the buffer supplied.
* It attempts to output the data it reads to `_diagStream`.
* @param buffer The buffer to copy the data into.
* @param size The size of `buffer`.
* @param bytesStored The number of bytes copied is written to this parameter.
* @param prompt The prompt to read until.
* @param timeMS The time limit in milliseconds.
* @return `true` if it found the specified prompt within the time
* limit, otherwise `false`.
*/
bool Sodaq_WifiBee::readTillPrompt(uint8_t* buffer, const size_t size,
  size_t& bytesStored, const char* prompt, const uint32_t timeMS)
{
  if (!_dataStream) {
    return false;
  }

  bool result = false;

  uint32_t startTS = millis();
  size_t promptIndex = 0;
  size_t promptLen = strlen(prompt);

  size_t bufferIndex = 0;
  size_t streamCount = 0;

  while (!timedOut32(startTS, timeMS)) {
    if (available()) {
      char c = read();
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
          bufferIndex = ((size - 1) < (streamCount - promptLen)) ? (size - 1) : (streamCount - promptLen);
          break;
        }
      }
      else {
        promptIndex = 0;
      }
    }
    else {
      _delay(10);
    }
  }

  bytesStored = bufferIndex;

  return result;
}

/*!
* This method reads and empties the input buffer of `_dataStream`.
* It continues until it finds the specified prompt or until
* the specified amount of time has elapsed.
* The source data is converted from HEX and copied to the
* buffer supplied.
* The first letter of the prompt cannot be a valid Hex char.
* It attempts to output the data it reads to `_diagStream`.
* @param buffer The buffer to copy the data into.
* @param size The size of `buffer`.
* @param bytesStored The number of bytes copied is written to this parameter.
* @param prompt The prompt to read until.
* @param timeMS The time limit in milliseconds.
* @return `true` if it found the specified prompt within the time
* limit, otherwise `false`.
*/
bool Sodaq_WifiBee::readHexTillPrompt(uint8_t* buffer, const size_t size,
  size_t& bytesStored, const char* prompt, const uint32_t timeMS)
{
  if (!_dataStream) {
    return false;
  }

  bool result = false;

  uint32_t startTS = millis();
  size_t promptIndex = 0;
  size_t promptLen = strlen(prompt);

  size_t bufferIndex = 0;
  size_t streamCount = 0;
  bool even = false;

  while (!timedOut32(startTS, timeMS)) {
    if (available()) {
      startTS = millis();
      char c = read();
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
          bufferIndex = ((size - 1) < ((streamCount - promptLen) / 2)) ? (size - 1) : (streamCount - promptLen) / 2;
          break;
        }
      }
      else {
        promptIndex = 0;

        if (even) {
          _buffer[bufferIndex - 2] = HEX2BYTE(_buffer[bufferIndex - 2], _buffer[bufferIndex - 1]);
          bufferIndex--;
        }
      }
      even = !even;
    }
    else {
      _delay(10);
    }
  }

  bytesStored = bufferIndex;

  return result;
}

/*!
* This method uploads data to the send buffer.
* The send buffer is stored on the NodeMCU and is transmitted
* once the data to be sent has been uploaded to it.
* It sends it in chunks so to comply with the LUA command limits.
*/
void Sodaq_WifiBee::sendAscii(const char* data)
{
  size_t length = strlen(data);
  size_t overhead = 9; // sb=sb.."" 
  size_t chunkSize = LUA_COMMAND_MAX - overhead - 1; //-1 for escape sequences

  size_t index = 0;
  size_t count;
  size_t slashCount;


  while (index < length) {
    count = 0;
    slashCount = 0;
    print("sb=sb..\"");
    while ((count < chunkSize) && (index < length)) {
      print(data[index]);

      //Keep track of the number of '\' symbols up to the index
      if (data[index] == '\\') {
        slashCount++;
      }
      else {
        slashCount = 0;
      }

      count++;
      index++;
    }

    //If we have an odd number of slashes send one more character
    //so that we don't divide an escape sequence.
    if (((slashCount % 2) == 1) && (index < length))
    {
      print(data[index]);
      index++;
    }

    println("\"");
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);
  }
}

/*!
* This method uploades escaped ASCII data to the send buffer.
* The send buffer is stored on the NodeMCU and is transmitted
* once the data to be sent has been uploaded to it.
* It only escapes specific LUA characters.
* @param data The buffer containing the ASCII data to send.
*/
void Sodaq_WifiBee::sendEscapedAscii(const char* data)
{
  size_t length = strlen(data);
  size_t overhead = 9; // sb=sb..""
  size_t chunkSize = LUA_COMMAND_MAX - overhead - 1; //-1 for space if final character is escaped

  size_t index = 0;
  size_t count;

  while (index < length) {
    count = 0;
    print("sb=sb..\"");
    while ((count < chunkSize) && (index < length)) {
      bool escaped = true;

      switch (data[index]) {
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
        print(data[index]);
        escaped = false;
        break;
      }

      //Add +1 for normal +2 for escaped.
      count++;
      if (escaped) {
        count++;
      }

      index++;
    }
    println("\"");
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);
  }
}

/*!
* This method uploades escaped binary data to the send buffer.
* The send buffer is stored on the NodeMCU and is transmitted
* once the data to be sent has been uploaded to it.
* It numerically escapes every byte.
* @param data The buffer containing the binary data to send.
* @param length The size of `data`.
*/
void Sodaq_WifiBee::sendEscapedBinary(const uint8_t* data, const size_t length)
{
  size_t overhead = 9; // sb=sb..""
  size_t chunkSize = (LUA_COMMAND_MAX - overhead) / 4; //Up to 4 characters per byte

  size_t index = 0;
  size_t count;

  while (index < length) {
    count = 0;
    print("sb=sb..\"");
    while ((count < chunkSize) && (index < length)) {
      print("\\");
      print(data[index]);

      count++;
      index++;
    }
    println("\"");
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);
  }
}

/*!
* This method opens a TCP or UDP connection to a remote server.
* @param server The server/host to connect to (IP address or domain).
* @param port The port to connect to.
* @param type The type of connection to establish, TCP or UDP.
* @return `true` if the connection was successfully established,
* otherwise `false`.
*/
bool Sodaq_WifiBee::openConnection(const char* server, const uint16_t port,
  const char* type)
{
  on();

  bool result;

  result = connect();

  if (result) {
    //Create the connection object
    print("wifiConn=net.createConnection(");
    print(type);
    println(", false)");
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);

    //Setup the callbacks
    setSimpleCallBack("connection", CONNECT_PROMPT);
    setSimpleCallBack("reconnection", RECONNECT_PROMPT);
    setSimpleCallBack("disconnection", DISCONNECT_PROMPT);
    setSimpleCallBack("sent", SENT_PROMPT);

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

/*!
* This method closes a TCP or UDP connection to a remote server.
* @return `true` if the connection was closed, otherwise `false`.
* It will return `false` if the connection was already closed.
*/
bool Sodaq_WifiBee::closeConnection()
{
  bool result;
  println("wifiConn:close()");
  result = skipTillPrompt(DISCONNECT_PROMPT, SERVER_DISCONNECT_TIMEOUT);

  off();

  return result;
}

/*!
* This method transmits ASCII data over an open TCP or UDP connection.
* @param data The data to transmit.
* @param waitForResponse Expect/wait for a reply from the server, default = true.
* @return `true` if the data was successfully transmitted,
* otherwise `false`.
*/
bool Sodaq_WifiBee::transmitAsciiData(const char* data, const bool waitForResponse)
{
  createSendBuffer();
  sendEscapedAscii(data);
  transmitSendBuffer();

  bool result;
  result = skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);

  if (result && waitForResponse) {
    if (skipTillPrompt(RECEIVED_PROMPT, SERVER_RESPONSE_TIMEOUT)) {
      readServerResponse();
    }
    else {
      clearBuffer();
    }
  }

  return result;
}

/*!
* This method transmits binary data over an open TCP or UDP connection.
* @param data The data to transmit.
* @param waitForResponse Expect/wait for a reply from the server, default = true.
* @return `true` if the data was successfully transmitted,
* otherwise `false`.
*/
bool Sodaq_WifiBee::transmitBinaryData(const uint8_t* data, const size_t length, const bool waitForResponse)
{
  createSendBuffer();
  sendEscapedBinary(data, length);
  transmitSendBuffer();

  bool result;
  result = skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);

  if (result && waitForResponse) {
    if (skipTillPrompt(RECEIVED_PROMPT, SERVER_RESPONSE_TIMEOUT)) {
      readServerResponse();
    }
    else {
      clearBuffer();
    }
  }

  return result;
}

/*!
* This method reads and stores the received response data.
* @return `true` on if it successfully reads the whole response,
* otherwise 'false'.
*/
bool Sodaq_WifiBee::readServerResponse()
{
  bool result;

  println(READ_BACK);
  result = skipTillPrompt(SOF_PROMPT, RESPONSE_TIMEOUT);

  if (result) {
    result = readHexTillPrompt(_buffer, _bufferSize, _bufferUsed, EOF_PROMPT,
      READBACK_TIMEOUT);
  }

  return result;
}

/*!
* This method joins the WifiBee to the network.
* @return `true` if the network was successfully joined,
* otherwise `false`.
*/
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

/*!
* This method disconnects the WifiBee from the network.
*/
void Sodaq_WifiBee::disconnect()
{
  println("wifi.sta.disconnect()");
  skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);
}

/*!
* This method checks the connection status of the WifiBee.
* @param status The status code (0..5) is written to this parameter.
* @return `true` if it successfully read the status code,
* otherwise `false`.
*/
bool Sodaq_WifiBee::getStatus(uint8_t& status)
{
  bool result;

  println(STATUS_CALLBACK);
  result = skipTillPrompt(STATUS_PROMPT, RESPONSE_TIMEOUT);

  char statusCode;

  if (result) {
    result = readChar(statusCode, RESPONSE_TIMEOUT);
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);
  }

  if (result) {
    if ((statusCode >= '0') && (statusCode <= '5')) {
      status = statusCode - '0';
    }
    else {
      result = false;
    }
  }

  return result;
}

/*!
* This method repeatedly calls getStatus() to check the connection status.
* It continues until it the network has been joined or until the
* specified time limit has elapsed.
* @param timeMS The time limit in milliseconds.
* @return `true` if the Wifi network was joined, otherwise `false`.
*/
bool Sodaq_WifiBee::waitForIP(const uint32_t timeMS)
{
  bool result = false;

  uint8_t status = 1;
  uint32_t startTS = millis();

  while ((!timedOut32(startTS, timeMS)) && (status == 1)) {
    skipForTime(STATUS_DELAY);
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

/*!
* This method constructs and sends a generic HTTP request.
* @param server The server/host to connect to (IP address or domain).
* @param port The port to connect to.
* @param The HTTP method to use. e.g. "GET", "POST" etc.
* @param location The resource location on the server/host.
* @param headers Any additional headers, each must be followed by a CRLF.
* HOST & Content-Length headers are added automatically.
* @param body The body (can be blank) to send with the request. Must not start with a CRLF.
* @param httpCode The HTTP response code is written to this parameter (if a response is received).
* @return `true` if a connection is established and the data is sent, `false` otherwise.
*/
bool Sodaq_WifiBee::HTTPAction(const char* server, const uint16_t port,
  const char* method, const char* location, const char* headers,
  const char* body, uint16_t& httpCode)
{
  bool result;

  // Open the connection
  result = openConnection(server, port, "net.TCP");

  if (result) {
    createSendBuffer();

    sendAscii(method);
    sendAscii(" ");
    sendAscii(location);
    sendAscii(" HTTP/1.1\\r\\n");

    sendAscii("HOST: ");
    sendAscii(server);
    sendAscii(":");

    char buff[11];
    itoa(port, buff, 10);
    sendAscii(buff);
    sendAscii("\\r\\n");

    if (strcmp(method, "GET") != 0) {
      sendAscii("Content-Length: ");
      itoa(strlen(body), buff, 10);
      sendAscii(buff);
      sendAscii("\\r\\n");
    }

    sendEscapedAscii(headers);
    sendAscii("\\r\\n");

    sendEscapedAscii(body);

    transmitSendBuffer();

    // Wait till we hear that it was sent
    result = skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);

    // Wait till we get the data received prompt
    if (result) {
      if (skipTillPrompt(RECEIVED_PROMPT, SERVER_RESPONSE_TIMEOUT)) {
        while (skipTillPrompt(RECEIVED_PROMPT, NEXT_PACKET_TIMEOUT)) {
        }

        readServerResponse();
        parseHTTPResponse(httpCode);
      }
      else {
        clearBuffer();
      }
    }

    // The connection might have closed automatically
    closeConnection();
  }

  return result;
}

/*!
* This method parses the HTTP response code from the data received.
* @param httpCode The response code is written into this parameter.
* @return `true` if the conversion returns a non-zero value,
* otherwise `false` .
*/
bool Sodaq_WifiBee::parseHTTPResponse(uint16_t& httpCode)
{
  bool result = false;

  // The HTTP response code should follow the first ' '
  if (_bufferUsed > 0) {
    char* codePos = strstr((char*)_buffer, " ");
    if (codePos) {
      httpCode = atoi(codePos);
      if (httpCode != 0) {
        result = true;
      }
    }
  }

  return result;
}

/*!
* This method checks if a number of milliseconds
* have elapsed. It is overflow safe.
* @param startTS The start timestamp
* @param ms The time out period in milliseconds
* @return `true` if elapsed time >= ms
* otherwise `false` .
*/
bool Sodaq_WifiBee::timedOut32(uint32_t startTS, uint32_t ms)
{
  uint32_t nowTS = millis();
  uint32_t diffTS = (nowTS >= startTS) ? nowTS - startTS : nowTS + (UINT_32_MAX - startTS);

  return (diffTS > ms);
}

/*!
* This inline method sets up simple callbacks.
* Callbacks which simply print a tag.
* @param eventName The event or callback name.
* @param tag The tag to print when the event is triggered.
*/
inline void Sodaq_WifiBee::setSimpleCallBack(const char* eventName, const char* tag)
{
  print("wifiConn:on(\"");
  print(eventName);
  print("\", function(s) print(\"");
  print(tag);
  println("\") end)");
  skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);
}

/*!
* This inline method clears the internal buffer.
*/
inline void Sodaq_WifiBee::clearBuffer()
{
  _bufferUsed = 0;
}

/*!
* This inline method is used throughout the class to add a delay.
* @param ms The delay in milliseconds.
*/
inline void Sodaq_WifiBee::_delay(uint32_t ms)
{
  delay(ms);
}

/*!
* This inline method creates a buffer on the NodeMCU to be
* loaded with data to be sent.
*/
inline void Sodaq_WifiBee::createSendBuffer()
{
  println("sb=\"\"");
  skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);
}

/*!
* This inline method transmits the send buffer on the NodeMCU
* over an open TCP or UDP connection.
*/
inline void Sodaq_WifiBee::transmitSendBuffer()
{
  println("wifiConn:send(sb) sb=\"\"");
  skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);
}

/*!
* Initialises member variables to default values.
* All pin variables are set to -1.
*/
Sodaq_WifiBeeOnOff::Sodaq_WifiBeeOnOff()
{
  _vcc33Pin = -1;
  _onoffPin = -1;
  _statusPin = -1;
}

/*!
* This method initialises a Sodaq_WifiBeeOnOff object.
* @param vcc33Pin The I/O pin to switch the 3V3 on or off (-1 if not used).
* @param onoffPin The I/O pin to switch the device on or off.
* @param statusPin The I/O pin which indicates whether the device is on or off.
*/
void Sodaq_WifiBeeOnOff::init(int vcc33Pin, int onoffPin, int statusPin)
{
  if (vcc33Pin >= 0) {
    _vcc33Pin = vcc33Pin;
    digitalWrite(_vcc33Pin, LOW);
    pinMode(_vcc33Pin, OUTPUT);
  }

  if (onoffPin >= 0) {
    _onoffPin = onoffPin;
    digitalWrite(_onoffPin, HIGH);
    pinMode(_onoffPin, OUTPUT);
  }

  if (statusPin >= 0) {
    _statusPin = statusPin;
    pinMode(_statusPin, INPUT);
  }
}


/*!
* This method switches the WifiBee device on.
* It is called by the Sodaq_WifiBee::on() method.
*/
void Sodaq_WifiBeeOnOff::on()
{
  // First VCC 3.3 HIGH
  if (_vcc33Pin >= 0) {
    digitalWrite(_vcc33Pin, HIGH);
  }
  // Wait a little
  // TODO Figure out if this is really needed
  delay(2);
  if (_onoffPin >= 0) {
    digitalWrite(_onoffPin, LOW);
  }
}

/*!
* This method switches the WifiBee device off.
* It is called by the Sodaq_WifiBee::off() method.
*/
void Sodaq_WifiBeeOnOff::off()
{
  if (_vcc33Pin >= 0) {
    digitalWrite(_vcc33Pin, LOW);
  }

  if (_onoffPin >= 0) {
    digitalWrite(_onoffPin, HIGH);
  }
}

/*!
* This method checks if the WifiBee device is on.
* It is called by the Sodaq_WifiBee::isOn() method.
* @return `true` if the WifiBee device is on, `false` otherwise.
*/
bool Sodaq_WifiBeeOnOff::isOn()
{
  if (_statusPin >= 0) {
    bool status = digitalRead(_statusPin);
    return status;
  }
  if (_onoffPin >= 0) {
    // Fall back. Use the onoff pin
    bool status = !digitalRead(_onoffPin);
    return status;
  }

  // No status pin, nothing. Let's assume it is on.
  return true;
}
