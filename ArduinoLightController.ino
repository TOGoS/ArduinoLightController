#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h> // As found at https://github.com/CNMAT/OSC
#include <OSCBundle.h> // ditto
#include "config.h"

void printHelp() {
  Serial.println("# Forth commands:");
  Serial.println("#   ( channel on-ticks total-ticks -- ) set-channel-duty-cycle");
  Serial.println("#   ( -- ) status");
  Serial.println("#   ( -- ) help");
}

long tickStartTime = -10000;
bool needDelay;

char hexDigit(int num) {
  num = num & 0xF;
  if( num < 10 ) return '0'+num;
  if( num < 16 ) return 'A'+num;
  return '?'; // Should be unpossible
}

template<size_t bufferSize>
class MessageBuffer {
public:
  char buffer[bufferSize];
  size_t bufferEnd = 0;

  void clear() {
    bufferEnd = 0;
  }
  void append(char c) {
    if( bufferEnd < bufferSize-1 ) {
      buffer[bufferEnd++] = c;
    }
  }
  void append(const char *str) {
    while( *str != 0 && bufferEnd < bufferSize-1 ) {
      buffer[bufferEnd++] = *str++;
    }
  }
  void separate(const char *separator) {
    if( bufferEnd > 0 ) append(separator);
  }
  void appendLabel(const char *label) {
    separate(" ");
    append(label);
    append(":");
  }
  void appendMacAddressHex(byte *macAddress, const char *octetSeparator) {
    for( int i=0; i<6; ++i ) {
      if( i > 0 ) append(octetSeparator);
      append(hexDigit(macAddress[i]>>4));
      append(hexDigit(macAddress[i]));
    }
  }
  void appendDecimal(long v) {
    if( v < 0 ) {
      append('-');
      v = -v;
    }
    appendDecimal((unsigned long)v);
  }
  void appendDecimal(unsigned long v) {
    int printed = snprintf(buffer+bufferEnd, bufferSize-bufferEnd, "%ld", v);
    if( printed > 0 ) bufferEnd += printed;
  }
  void appendDecimal(float v) {
    if( v < 0 ) {
      append('-');
      v = -v;
    }
    int hundredths = (v * 100) - ((int)v) * 100;
    int printed = snprintf(buffer+bufferEnd, bufferSize-bufferEnd, "%d.%02d", (int)v, hundredths);
    if( printed > 0 ) bufferEnd += printed;
  }
  const char *close() {
    buffer[bufferEnd] = 0;
    return buffer;
  }
  size_t getRemainingSpace() {
    return bufferSize-bufferEnd;
  }
};

class OutputController {
  struct OutputSwitch {
    unsigned int port;
    const char *name;
    unsigned int cycleLength;
    unsigned int cycleOnLength;
    //unsigned int cycleOffset;
    //unsigned int cycleLength;
    //unsigned int cycleDivisions;
    //bool cycleData[32];
  };
  
  unsigned int tickNumber = 0;
  
  OutputSwitch switches[8] = {
    { port: D1, name: "D1" },
    { port: D2, name: "D2" },
    { port: D3, name: "D3" },
    { port: D4, name: "D4" },
    { port: D5, name: "D5" },
    { port: D6, name: "D6" },
    { port: D7, name: "D7" },
    { port: D8, name: "D8" },
  };
  size_t switchCount = sizeof(switches)/sizeof(OutputSwitch);
public:
  OutputController() {
    for (unsigned int i=0; i<switchCount; ++i) {
      OutputSwitch &swich = switches[i];
      swich.cycleLength = 1000;
      swich.cycleOnLength = (i+1)*100;
    }
  }

  void setChannelDutyCycle(size_t channelId, unsigned int cycleOnLength, unsigned int cycleLength) {
    if( channelId >= switchCount ) {
      Serial.print("# Channel index out of bounds: ");
      Serial.println(channelId);
      return;
    }
    if( cycleLength == 0 ) {
      Serial.print("# Ack, someone tied to set channel ");
      Serial.print(channelId);
      Serial.println("'s duty cycle denominator to zero!  Ignoring.");
      return;
    }
    Serial.print("# Setting channel ");
    Serial.print(channelId);
    Serial.print(" duty cycle to ");
    Serial.print(cycleOnLength);
    Serial.print("/");
    Serial.println(cycleLength);
    switches[channelId].cycleOnLength = cycleOnLength;
    switches[channelId].cycleLength = cycleLength;
  }
  
  void setChannelDutyCycle(size_t channelId, float intensity) {
    if( intensity < 0 ) intensity = 0;
    if( intensity > 1 ) intensity = 1;
    setChannelDutyCycle(channelId, (unsigned int)(intensity*256), 256);
  }

  void begin() {
    for (unsigned int i=0; i<switchCount; ++i) {
      OutputSwitch &swich = switches[i];
      pinMode(swich.port, OUTPUT);
    }
  }
  
  void printChannelInfo() {
    for (unsigned int i=0; i<switchCount; ++i) {
      OutputSwitch &swich = switches[i];
      pinMode(swich.port, OUTPUT);
      swich.cycleLength = 1000;
      swich.cycleOnLength = (i+1)*100;
      Serial.print("# Channel ");
      Serial.print(i);
      Serial.print(" name:");
      Serial.print(swich.name);
      Serial.print(" port:");
      Serial.println(swich.port);
    }
  }
  
  void tick() {
    for (unsigned int i=0; i<switchCount; ++i) {
      OutputSwitch &swich = switches[i];
      //unsigned int div = ((swich.cycleOffset + tick) % swich.cycleLength) * swich.cycleDivisions / swich.cycleLength;
      //digitalWrite(swich.port, swich.cycleData[div] ? HIGH : LOW);
      digitalWrite(swich.port, (tickNumber % swich.cycleLength) < swich.cycleOnLength ? HIGH : LOW);
    }
    ++tickNumber;
  }
};
OutputController outputController;

MessageBuffer<96> messageBuffer;
byte macAddressBuffer[6];
long lastWiFiConnectAttempt = -10000;
int previousWiFiStatus = -1;

void reportWiFiStatus(int wiFiStatus) {
  Serial.print("# WiFi status: ");
  switch( wiFiStatus ) {
  case WL_CONNECTED:
    Serial.println("WL_CONNECTED");
    Serial.print("# IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("# MAC address: ");
    WiFi.macAddress(macAddressBuffer);
    messageBuffer.clear();
    messageBuffer.appendMacAddressHex(macAddressBuffer, ":");
    Serial.println(messageBuffer.close());
    Serial.print("# Listening for OSC messages on port ");
    Serial.println(OSC_LOCAL_PORT);
    break;
  case WL_NO_SHIELD:
    Serial.println("WL_NO_SHIELD");
    break;
  case WL_IDLE_STATUS:
    Serial.println("WL_IDLE_STATUS");
    break;
  case WL_NO_SSID_AVAIL:
    Serial.println("WL_NO_SSID_AVAIL");
    break;
  case WL_SCAN_COMPLETED:
    Serial.println("WL_SCAN_COMPLETED");
    break;
  case WL_CONNECT_FAILED:
    Serial.println("WL_CONNECT_FAILED");
    break;
  case WL_CONNECTION_LOST:
    Serial.println("WL_CONNECTION_LOST");
    break;
  case WL_DISCONNECTED:
    Serial.println("WL_DISCONNECTED");
    break;
  default:
    Serial.println(wiFiStatus);
  }
}

void printStatus() {
  reportWiFiStatus(WiFi.status());
  outputController.printChannelInfo();
}

int maintainWiFiConnection() {
  int wiFiStatus = WiFi.status();
  if( wiFiStatus != previousWiFiStatus ) {
    reportWiFiStatus(wiFiStatus);
    previousWiFiStatus = wiFiStatus;
  }
  if( wiFiStatus != WL_CONNECTED && tickStartTime - lastWiFiConnectAttempt >= 10000 ) {
    Serial.print("# Attempting to connect to ");
    Serial.print(WIFI_SSID);
    Serial.println("...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWiFiConnectAttempt = tickStartTime;
  }
  return wiFiStatus;
}

class UDPOSC {
  WiFiUDP& udp;
public:
  enum PacketType {
    NONE,
    MESSAGE,
    BUNDLE
  };

  UDPOSC(WiFiUDP& udp) : udp(udp) { }

  OSCMessage oscMessage;
  OSCBundle oscBundle;
  byte packetBuffer[1024];
  PacketType receive() {
    int cb = udp.parsePacket();
    if( !cb ) return NONE;
    int packetSize = udp.read(packetBuffer, 1024);

    Serial.print("# Received ");
    Serial.print(packetSize);
    Serial.print("-byte '");
    Serial.print(packetBuffer[0]);
    Serial.println("' packet");

    oscBundle.empty();
    oscMessage.empty();

    if (packetBuffer[0] == '#') {
      // Either this doesn't work,
      // or puredata's sending bad bundles
      oscBundle.fill(packetBuffer, packetSize);
      return BUNDLE;
    } else {
      oscMessage.fill(packetBuffer, packetSize);
      return MESSAGE;
    }
  }
  void send() {
    
  }
};

WiFiUDP udp;
UDPOSC udpOsc(udp);

namespace ArduForth {
  const byte MAX_TOKEN_LENGTH = 32;
  const byte MAX_STACK_DEPTH = 32;
  
  template <class Item>
  struct Pool {
    Item *begin;
    Item *end;
    
    Item *allocate( int count ) {
      if( begin + count <= end ) {
        Item *b = begin;
        begin += count;
        return b;
        return true;
      } else {
        return false;
      }
    };
  };
  
  char tokenBuffer[MAX_TOKEN_LENGTH];
  byte tokenBufferLength = 0;
  
  int stack[MAX_STACK_DEPTH];
  int stackBottom = MAX_STACK_DEPTH;
  
  struct Word {
    boolean isCompileTime;
    boolean isNative;
    union {
      void (*nativeFunction)();
      Word *forthFunction;
    } implementation;
    char *text;
    
    void run( boolean compileTime ) const {
      if( compileTime && !isCompileTime ) {
        Serial.print("# Word '");
        Serial.print(text);
        Serial.println("' is not a compile-time word, so ignoring at compile-time.");
        // TODO: Add to definition
      } else {
        if( isNative ) {
          implementation.nativeFunction();
        } else {
          Serial.print("# Word '");
          Serial.print(text);
          Serial.println("' is not native; ignoring for now.");
          // TODO: something with implementation.forthFunction
        }
      }
    }
  };
  
  template <class Item>
  struct Dictionary {
    Dictionary<Item> *previous;
    Item *begin;
    Item *end;
    
    Item *find( char *text ) const {
      for( Item *i = end-1; i>=begin; --i ) {
        /*
        Serial.print("Checking '");
        Serial.print(i->text);
        Serial.print("' == '");
        Serial.print(text);
        Serial.println("'");
        */
        if( strcmp(i->text, text) == 0 ) return i;
      }
      return NULL;
    }
  };
  
  void push(int v) {
    if( stackBottom == 0 ) {
      Serial.print("# Error: stack full at ");
      Serial.print(MAX_STACK_DEPTH);
      Serial.print(" items; cannot push value: ");
      Serial.println(v);
    } else {
      stack[--stackBottom] = v;
    }
  }
  
  int pop() {
    if( stackBottom == MAX_STACK_DEPTH ) {
      Serial.println("# Error: stack underflow");
      return 0;
    } else { 
      return stack[stackBottom++];
    }
  }
  
  //// Builtin words ////
  
  void pushStackFree() {
    push( stackBottom );
  }
  void printStack() {
    for( int i = MAX_STACK_DEPTH - 1; i >= stackBottom; --i ) {
      Serial.print( stack[i] );
      Serial.print( " " );
    }
    Serial.println();
  }
  void printIntFromStack() {
    Serial.println( pop() );
  }
  void addIntsFromStack() {
    push( pop() + pop() );
  }
  void subtractIntsFromStack() {
    int b = pop();
    int a = pop();
    push( a - b );
  }
  void setSwitchDutyCycle() {
    unsigned int denominator = (unsigned int)pop();
    unsigned int numerator = (unsigned int)pop();
    unsigned int switchNumber = (unsigned int)pop();
    outputController.setChannelDutyCycle(switchNumber, numerator, denominator);
  }
  void printChannelInfo() {
    outputController.printChannelInfo();
  }
    
  const Word staticWords[] = {
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: pushStackFree
      },
      text: "stack-free"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: printStack
      },
      text: "print-stack"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: printIntFromStack
      },
      text: "print-int"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: addIntsFromStack
      },
      text: "+"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: subtractIntsFromStack
      },
      text: "-"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: setSwitchDutyCycle
      },
      text: "set-channel-duty-cycle"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: printStatus
      },
      text: "status"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: printHelp
      },
      text: "help"
    }
  };
  
  ////
  
  const Dictionary<const Word> staticDict = {
    previous: NULL,
    begin: staticWords,
    end: staticWords + (int)(sizeof(staticWords)/sizeof(Word))
  };
  
  void handleWord( char *buffer, int len ) {
    boolean isInteger = true;
    int intVal = 0;
    for( int i=0; i<len; ++i ) {
      if( buffer[i] < '0' || buffer[i] > '9' ) {
        isInteger = false;
        break;
      }
      intVal *= 10;
      intVal += buffer[i] - '0';
    }
    
    if( isInteger ) {
      push(intVal);
      return;
    }
    
    const Word *word = staticDict.find(buffer);
    if( word != NULL ) {
      word->run( false );
      return;
    }
    
    Serial.print("# Error: unrecognised word: ");
    Serial.println(tokenBuffer);
  }
  
  void flushToken() {
    tokenBuffer[tokenBufferLength] = 0;
    if( tokenBufferLength > 0 ) {
      handleWord( tokenBuffer, tokenBufferLength );
    }
    tokenBufferLength = 0;
  }
  
  void handleChar( char c ) {
    // TODO: Handle double-quoted strings
    switch( c ) {
    case '\n': case ' ': case '\t': case '\r':
      flushToken();
      break;
    default:
      tokenBuffer[tokenBufferLength++] = c;
    }
    if( tokenBufferLength >= MAX_TOKEN_LENGTH ) {
      Serial.println("Token too long.");
    }
  }
}

/*
class CommandProcessor {
public:
  char buffer[128];
  size_t bufferEnd = 0;

  void flush() {
    this->buffer[this->bufferEnd] = 0;
    unsigned int switchNumber = 0, numerator = 1000, denominator = 500;
    if( this->buffer[0] == 's' ) {
      if( switchNumber >= switchCount ) {
        Serial.print("# Switch number out of range: ");
        Serial.println(switchNumber);
        return;
      }
      switches[switchNumber].cycleLength = numerator;
      switches[switchNumber].cycleOnLength = denominator;
    } else {
      Serial.print("# Unrecognized command: ");
      Serial.println(this->buffer);
    }
    this->bufferEnd = 0;
  }
  
  void data(char b) {
    if(b == '\n') {
      this->flush();
      return;
    } else if( this->bufferEnd < 128 ) {
      this->buffer[this->bufferEnd++] = b;
    }
  }
  void update() {
    int inByte = Serial.read();
    if(inByte != -1) this->data((char)inByte);
  }
};

CommandProcessor commandProcessor;
*/

void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("# Henlo, this is ArduinoLightController, booting!");
  udp.begin(OSC_LOCAL_PORT);
  outputController.begin();
  outputController.printChannelInfo();
  Serial.println("# Type 'help' for a command list.");
}

void handleOscMessage(OSCMessage &oscMessage){
  Serial.print("# Received OSC message: ");
  oscMessage.getAddress(messageBuffer.buffer);
  Serial.print(messageBuffer.buffer);
  messageBuffer.clear();
  for( int i=0; i<oscMessage.size(); ++i ) {
    Serial.print(" ");
    char type = oscMessage.getType(i);
    switch(type) {
    case 'i': Serial.print(oscMessage.getInt(i)); break;
    case 'f': Serial.print(oscMessage.getFloat(i)); break;
    default:
    Serial.print("<");
    Serial.print(type);
    Serial.print(">");
    }
  }
  Serial.println("");
  if( oscMessage.fullMatch("/channels/*/value") ) {
    unsigned int channelId = messageBuffer.buffer[10] - '0';
    float newStatus = 1;
    if( oscMessage.isInt(0) ) {
      newStatus = oscMessage.getInt(0);
    } else if( oscMessage.isBoolean(0) ) {
      newStatus = oscMessage.getBoolean(0) ? 1 : 0;
    } else if( oscMessage.isFloat(0) ) {
      newStatus = oscMessage.getFloat(0);
    }
    outputController.setChannelDutyCycle(channelId, newStatus);
  }
}

void loop() {
  tickStartTime = millis();

  maintainWiFiConnection();
  
  if( Serial.available() > 0 ) {
    ArduForth::handleChar(Serial.read());
  }

  UDPOSC::PacketType received = udpOsc.receive();
  switch( received ) {
  case UDPOSC::BUNDLE:
    {
      int messageIndex = 0;
      OSCMessage *oscMessage;
      while( (oscMessage = udpOsc.oscBundle.getOSCMessage(messageIndex)) != NULL ) {
        handleOscMessage(*oscMessage);
        ++messageIndex;
      }
      Serial.print("# OSC bundle contained ");
      Serial.print(messageIndex);
      Serial.println(" messages");
      break;
    }
  case UDPOSC::MESSAGE:
    handleOscMessage(udpOsc.oscMessage);
    break;
  default: break;
  }
  
  if(needDelay) {
    // Some things seem to not work without a delay somewhere.
    // So after kicking those things off, set needDelay = true.
    delay(1);
    needDelay = false;
  }

  outputController.tick();
}


