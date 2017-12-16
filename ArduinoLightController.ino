#include <ESP8266WiFi.h>
#include "config.h"

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
  char buffer[bufferSize];
  size_t bufferEnd = 0;

public:
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
};

struct Switch {
  unsigned int port;
  const char *name;
  unsigned int cycleLength;
  unsigned int cycleOnLength;
  //unsigned int cycleOffset;
  //unsigned int cycleLength;
  //unsigned int cycleDivisions;
  //bool cycleData[32];
};

const unsigned int switchCount = 8;

Switch switches[] = {
  { port: D1, name: "D1" },
  { port: D2, name: "D2" },
  { port: D3, name: "D3" },
  { port: D4, name: "D4" },
  { port: D5, name: "D5" },
  { port: D6, name: "D6" },
  { port: D7, name: "D7" },
  { port: D8, name: "D8" },
  //{ port: D9 },
  //{ port: D10 },
};

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
        Serial.print("Word '");
        Serial.print(text);
        Serial.println("' is not a compile-time word, so ignoring at compile-time.");
        // TODO: Add to definition
      } else {
        if( isNative ) {
          implementation.nativeFunction();
        } else {
          Serial.print("Word '");
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
    if (switchNumber >= switchCount) {
      Serial.print("Switch #");
      Serial.print(switchNumber);
      Serial.print(" out of range 0...");
      Serial.println(switchCount);
      return;
    }
    
    switches[switchNumber].cycleLength = denominator;
    switches[switchNumber].cycleOnLength = numerator;
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
      text: "set-switch-duty-cycle"
    },
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





#define FOREACH_SWITCH(iter) for (unsigned int iter=0; iter<switchCount; ++iter)

unsigned int tickNumber;

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
  FOREACH_SWITCH(i) {
    Switch &swich = switches[i];
    pinMode(swich.port, OUTPUT);
    swich.cycleLength = 1000;
    swich.cycleOnLength = (i+1)*100;
    //swich.cycleLength = i * 10;
    //swich.cycleDivisions = 2;
    //swich.cycleOffset = 0;
    //swich.cycleData[0] = true;
    //swich.cycleData[1] = false;
    Serial.print("# Switch ");
    Serial.print(i);
    Serial.print(" name:");
    Serial.print(swich.name);
    Serial.print(" port:");
    Serial.println(swich.port);
    needDelay = true;
  }
  tickNumber = 0;
}

void loop() {
  tickStartTime = millis();

  maintainWiFiConnection();
  if( Serial.available() > 0 ) {
    ArduForth::handleChar(Serial.read());
  }
  if(needDelay) {
    // Some things seem to not work without a delay somewhere.
    // So after kicking those things off, set needDelay = true.
    delay(1);
    needDelay = false;
  }

  FOREACH_SWITCH(i) {
    Switch &swich = switches[i];
    //unsigned int div = ((swich.cycleOffset + tick) % swich.cycleLength) * swich.cycleDivisions / swich.cycleLength;
    //digitalWrite(swich.port, swich.cycleData[div] ? HIGH : LOW);
    digitalWrite(swich.port, (tickNumber % swich.cycleLength) < swich.cycleOnLength ? HIGH : LOW);
  }
  ++tickNumber;
}


