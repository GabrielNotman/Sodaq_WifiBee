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
#define LUA_COMMAND_MAX 255

// Lua prompts
#define LUA_PROMPT "\r\n> "
#define CONNECT_PROMPT "|C|"
#define RECONNECT_PROMPT "|RC|"
#define DISCONNECT_PROMPT "|DC|"
#define SENT_PROMPT "|DS|"
#define RECEIVED_PROMPT "|DR|"
#define STATUS_PROMPT "|STS|"
#define SOF_PROMPT "|SOF|"
#define EOF_PROMPT "|EOF|"

// Lua connection callback scripts
#define CONNECT_CALLBACK "function(s) print(\"|C|\") end" // Max length 228
#define RECONNECT_CALLBACK "function(s) print(\"|RC|\") end" // Max length 226
#define DISCONNECT_CALLBACK "function(s) print(\"|DC|\") end" // Max length 225
#define SENT_CALLBACK "function(s) print(\"|DS|\") end" // Max length 234
#define RECEIVED_CALLBACK "function(s, d) lastData=d print(\"|DR|\") end" // Max length 231

#define STATUS_CALLBACK "print(\"|\" .. \"STS|\" .. wifi.sta.status() .. \"|\")" // Max length 255
#define READ_BACK "uart.write(0, \"|\" .. \"SOF|\" .. lastData .. \"|EOF|\")" // Max length 255

// Timeout constants
#define RESPONSE_TIMEOUT 2000
#define WIFI_CONNECT_TIMEOUT 4000
#define SERVER_CONNECT_TIMEOUT 5000
#define SERVER_RESPONSE_TIMEOUT 5000
#define SERVER_DISCONNECT_TIMEOUT 2000
#define READBACK_TIMEOUT 2500
#define WAKE_DELAY 1000
#define STATUS_DELAY 1000

/*! 
* Initialises member variables to default values,
* including any pointers to NULL.
*/
Sodaq_WifiBee::Sodaq_WifiBee()
{
  _APN = "";
  _username = "";
  _password = "";

  _bufferSize = 0;
  _bufferUsed = 0;
  _buffer = NULL;

  _dataStream = NULL;
  _diagStream = NULL;

  // Initialize to some unlikely value
  _dtrPin = 0xFF;               
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
* @param dtrPin The I/O pin connected to the Bee socket's DTR pin.
* @param bufferSize The amount of memory to allocate to the internal buffer.
*/
void Sodaq_WifiBee::init(Stream& stream, const uint8_t dtrPin,
  const size_t bufferSize)
{
  _dataStream = &stream;
  _dtrPin = dtrPin;

  _bufferSize = bufferSize;
  if (_buffer) {
    free(_buffer);
  }
  _buffer = (uint8_t*)malloc(_bufferSize);

  pinMode(_dtrPin, OUTPUT);

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
* This method switches on the WifiBee. 
* It is called automatically, as required, by most methods.
*/
void Sodaq_WifiBee::on()
{
  diagPrintLn("\r\nPower ON");
  digitalWrite(_dtrPin, LOW);
  skipForTime(WAKE_DELAY);
}

/*! 
* This method switches off the WifiBee. 
* It is called automatically, as required, by most methods.
*/
void Sodaq_WifiBee::off()
{
  diagPrintLn("\r\nPower OFF");
  digitalWrite(_dtrPin, HIGH);
}

// HTTP methods
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
    print("\\r\\n");

    sendEscapedAscii(body);

    println("\")");
  }

  // Wait till we hear that it was sent
  if (result) {
    result = skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
  }

  // Wait till we get the data received prompt
  if (result) {
    if (skipTillPrompt(RECEIVED_PROMPT, SERVER_RESPONSE_TIMEOUT)) {
      readServerResponse();
      parseHTTPResponse(httpCode);
    } else {
      clearBuffer();
    }
  }
  
  // The connection might have closed automatically
  // or it failed to open
  closeConnection();

  return result;
}

/*!
*\overload
*/
bool Sodaq_WifiBee::HTTPAction(const String& server, const uint16_t port,
  const String& method, const String& location, const String& headers,
  const String& body, uint16_t& httpCode)
{
  return HTTPAction(server.c_str(), port, method.c_str(), location.c_str(),
    headers.c_str(), body.c_str(), httpCode);
}

/*!
* This method constructs and sends a HTTP GET request.
* @param server The server/host to connect to (IP address or domain).
* @param port The port to connect to.
* @param location The resource location on the server/host.
* @param headers Any additional headers, each must be followed by a CRLF.
* HOST header is added automatically.
* @param httpCode The HTTP response code is written to this parameter (if a response is received).
* @return `true` if a connection is established and the data is sent, `false` otherwise.
*/
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
    print("\\r\\n");

    println("\")");
  }

  // Wait till we hear that it was sent
  if (result) {
    result = skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
  }

  // Wait till we get the data received prompt
  if (result) {
    if (skipTillPrompt(RECEIVED_PROMPT, SERVER_RESPONSE_TIMEOUT)) {
      readServerResponse();
      parseHTTPResponse(httpCode);
    } else {
      clearBuffer();
    }
  }

  // The connection might have closed automatically
  // or it failed to open
  closeConnection();

  return result;
}

/*!
*\overload
*/
bool Sodaq_WifiBee::HTTPGet(const String& server, const uint16_t port,
  const String& location, const String& headers, uint16_t& httpCode)
{
  return HTTPGet(server.c_str(), port, location.c_str(), headers.c_str(), httpCode);
}

/*!
* This method constructs and sends a HTTP POST request.
* @param server The server/host to connect to (IP address or domain).
* @param port The port to connect to.
* @param location The resource location on the server/host.
* @param headers Any additional headers, each must be followed by a CRLF. 
* HOST & Content-Length headers are added automatically.
* @param body The body (can be blank) to send with the request. Must not start with a CRLF.
* @param httpCode The HTTP response code is written to this parameter (if a response is received).
* @return `true` if a connection is established and the data is sent, `false` otherwise.
*/
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
    print("\\r\\n");

    sendEscapedAscii(body);

    println("\")");
  }

  // Wait till we hear that it was sent
  if (result) {
    result = skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);
  }

  // Wait till we get the data received prompt
  if (result) {
    if (skipTillPrompt(RECEIVED_PROMPT, SERVER_RESPONSE_TIMEOUT)) {
      readServerResponse();
      parseHTTPResponse(httpCode);
    }
    else {
      clearBuffer();
    }
  }

  // The connection might have closed automatically
  // or it failed to open
  closeConnection();

  return result;
}

/*!
*\overload
*/
bool Sodaq_WifiBee::HTTPPost(const String& server, const uint16_t port,
  const String& location, const String& headers, const String& body,
  uint16_t& httpCode)
{
  return HTTPPost(server.c_str(), port, location.c_str(), headers.c_str(),
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

  bytesRead = min((size - 1), _bufferUsed);

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

  bytesRead = min(size, _bufferUsed);

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
  bytesRead = min((size - 1), (_bufferUsed - startIndex));

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
  } else {
    return 0;
  }
}

/*!
* Implementation of Stream::write(buffer, size) \n
* If `_dataStream != NULL` it calls `_dataStream->write(buffer, size)`.
* @param buffer Data to pass to `_dataStream->write()`.
* @param size Size of `buffer\ to pass to `_dataStream->write()`.
* @return result of `_dataStream->write(buffer, size)` or 0 if `_dataStream == NULL`.
*/
size_t Sodaq_WifiBee::write(const uint8_t *buffer, size_t size)
{
  if (_dataStream) {
    return _dataStream->write(buffer, size);
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
  } else {
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
  } else {
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
  } else {
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
* This method reads and empties the input buffer of `_dataStream`. 
* It attempts to output the data it reads to `_diagStream`. 
*/
void Sodaq_WifiBee::flushInputStream()
{
  while (available()) {
    diagPrint((char )read());
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
  uint32_t maxTS = millis() + timeMS;

  while (millis() < maxTS) {
    if (available()) {
      char c = read();
      diagPrint(c);
      count++;
    } else {
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

  uint32_t maxTS = millis() + timeMS;
  size_t index = 0;
  size_t promptLen = strlen(prompt);

  while (millis() < maxTS) {
    if (available()) {
      char c = read();
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

  uint32_t maxTS = millis() + timeMS;
  while ((millis() < maxTS) && (!result)) {
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

  uint32_t maxTS = millis() + timeMS;
  size_t promptIndex = 0;
  size_t promptLen = strlen(prompt);

  size_t bufferIndex = 0;
  size_t streamCount = 0;
  

  while (millis() < maxTS) {
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

/*!
* This method writes escaped ASCII data to `_dataStream`.
* It only escapes specific LUA characters.
* @param data The buffer containing the ASCII data to send.
*/
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

/*!
* This method writes escaped binary data to `_dataStream`.
* It numerically escapes every byte.
* @param data The buffer containing the binary data to send.
* @param length The size of `data`.
*/
void Sodaq_WifiBee::sendEscapedBinary(const uint8_t* data, const size_t length)
{
  for (size_t i = 0; i < length; i++) {
    print("\\");
    print(data[i]);
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
  print("wifiConn:send(\"");
  sendEscapedAscii(data);
  println("\")");

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
  print("wifiConn:send(\"");
  sendEscapedBinary(data, length);
  println("\")");

  bool result;
  result = skipTillPrompt(SENT_PROMPT, RESPONSE_TIMEOUT);

  if (result && waitForResponse) {
    if (skipTillPrompt(RECEIVED_PROMPT, SERVER_RESPONSE_TIMEOUT)) {
      readServerResponse();
    } else {
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
    result = readTillPrompt(_buffer, _bufferSize, _bufferUsed, EOF_PROMPT,
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
  }

  if (result) {
    if ((statusCode >= '0') && (statusCode <= '5')) {
      status = statusCode - '0';
    } else {
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
  uint32_t maxTS = millis() + timeMS;

  while ((millis() < maxTS) && (status == 1)) {
    skipForTime(STATUS_DELAY);
    getStatus(status);
  }

  // Without this small delay the lua interpreter sometimes
  // gets confused. This also flushes the incoming buffer
  skipForTime(100);

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
* This method uploads data to the send buffer.
* It sends it in chunks so to comply with the LUA command limits.
*/
void Sodaq_WifiBee::sendChunkedData(const char* data)
{
  size_t length = strlen(data);
  size_t overhead = 9; // sb=sb..""
  size_t chunkSize = LUA_COMMAND_MAX - overhead;

  //Upload the whole chunks
  for (size_t i = 0; i < (length / chunkSize); i++) {
    print("sb=sb..\"");
    write((uint8_t*)&data[i * chunkSize], chunkSize);
    println("\"");
    skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);
  }

  //Upload the remainder
  size_t remainder = length % chunkSize;
  print("sb=sb..\"");
  write((uint8_t*)&data[length - remainder], remainder);
  println("\"");
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
  println("wifiConn:send(sb)");
  skipTillPrompt(LUA_PROMPT, RESPONSE_TIMEOUT);
}
