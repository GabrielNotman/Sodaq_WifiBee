#define SerialMonitor Serial
#define BeeSerial Serial1

#define PING_ATTEMPTS 2
#define PING_TIMEOUT 1000
#define SKIP_TIME 500
#define RESTART_TIME 2000

//#define DEBUG

const uint8_t numRates = 4;
const uint32_t baudRates[numRates] = {9600, 19200, 38400, 57600};

// Predeclarations
void skipForTime(uint32_t timeMS = SKIP_TIME);
bool ping();

void setup() 
{
  SerialMonitor.begin(57600);
  SerialMonitor.println("----Starting...----");
  SerialMonitor.print("----Attempting to connect at rates:");
  
  for (uint8_t i=0; i <numRates; i++) {
    SerialMonitor.print(" " + String(baudRates[i], DEC));
  }
  SerialMonitor.println("----");
  SerialMonitor.println("----Using " + String(PING_ATTEMPTS, DEC) + " ping attempts----");
  
  bool connectSuccess = false;
  uint8_t rateCount = 0;
  
  while ((rateCount < numRates) && (!connectSuccess)) {
    SerialMonitor.println("----Attempting to connect to the NodeMCU at " + String(baudRates[rateCount], DEC) + " baudrate----");
    BeeSerial.begin(baudRates[rateCount]);
  
    if (ping())
    {
      connectSuccess = true;
      SerialMonitor.println("----NodeMCU connection established at " + String(baudRates[rateCount], DEC) + "----");
      SerialMonitor.println("----Removing \"init.lua\"----");
      BeeSerial.println("file.remove(\"init.lua\")");
      skipForTime();
      
      SerialMonitor.println("----Restarting...----");
      BeeSerial.println("node.restart()");
      BeeSerial.flush();
      BeeSerial.begin(9600);
      skipForTime(RESTART_TIME);
    
      SerialMonitor.println("----Testing default baudrate...----");
      if (ping()) {
        SerialMonitor.println("----Success, connection re-established at default 9600 baudrate, NodeMCU will now run at the default rate----");
      } else {
        SerialMonitor.println("----Process failed, could not re-establish connection----");
      }  
    }
    rateCount++;
  }
  
  if (!connectSuccess) {
    SerialMonitor.println("----Process failed, could not establish connection to NodeMCU----");
  }
}

void loop() 
{
}

void skipForTime(uint32_t timeMS)
{
  uint32_t maxTS = millis() + timeMS;

  while (millis() < maxTS) {
    if (BeeSerial.available()) {
      char c = BeeSerial.read();
#ifdef DEBUG
      SerialMonitor.print(c);
#endif
    } else {
      delay(10);
    }
  }
#ifdef DEBUG
  SerialMonitor.println();
#endif
}

bool ping()
{
  bool result = false;
  
#ifdef DEBUG
  SerialMonitor.println("Ping prelude");
#endif
  BeeSerial.println(" ");
  skipForTime();
  
  uint8_t pingCount = 0;

  while ((pingCount < PING_ATTEMPTS) && (!result)) {
    pingCount++;
    SerialMonitor.println("Ping attempt " + String(pingCount, DEC));
    BeeSerial.println("print(\"|P\" .. \"ING|\")");  
 
    char* prompt = "|PING|";
    uint32_t maxTS = millis() + PING_TIMEOUT;
    size_t index = 0;
    size_t promptLen = strlen(prompt);

    while ((millis() < maxTS) && (!result)){
      if (BeeSerial.available()) {
        char c = BeeSerial.read();
#ifdef DEBUG
        SerialMonitor.print(c);
#endif
        if (c == prompt[index]) {
          index++;
          if (index == promptLen) {
            result = true;
          }
        } else {
          index = 0;
        }
      } else {
        delay(10);
      }
    }
#ifdef DEBUG
    SerialMonitor.println();
#endif
  }
  return result;
}

