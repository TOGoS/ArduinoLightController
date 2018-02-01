#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "config.h"

long tickStartTime = -10000;
uint8_t tickNumber = 0;
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

//// OutputController

class OutputController {
 public:
  using ChannelID = uint8_t;
  using OutputLevel = uint8_t;
  using StepIndex = uint8_t;
  using SequenceLength = uint8_t;
  static constexpr unsigned int MAX_SEQUENCE_LENGTH = 64;
  unsigned long startTime;
  uint16_t bpmHigh = 120;
  uint8_t bpmLow = 0;
  
  struct StepData {
    OutputLevel startLevel;
    OutputLevel endLevel;
  };
  struct OutputChannel {
    unsigned int port;
    const char *name;
    //unsigned int cycleLength;
    //unsigned int cycleOnLength;

    SequenceLength sequenceLength;
    uint8_t beatDivision; // steps per beat
    
    StepData stepData[MAX_SEQUENCE_LENGTH];
    //unsigned int cycleOffset;
    //unsigned int cycleLength;
    //unsigned int cycleDivisions;
    //bool cycleData[32];
  };
  
  OutputChannel channels[8] = {
    { port: D1, name: "D1", sequenceLength: 1, beatDivision: 1 },
    { port: D2, name: "D2", sequenceLength: 1, beatDivision: 1 },
    { port: D3, name: "D3", sequenceLength: 1, beatDivision: 1 },
    { port: D4, name: "D4", sequenceLength: 1, beatDivision: 1 },
    { port: D5, name: "D5", sequenceLength: 1, beatDivision: 1 },
    { port: D6, name: "D6", sequenceLength: 1, beatDivision: 1 },
    { port: D7, name: "D7", sequenceLength: 1, beatDivision: 1 },
    { port: D8, name: "D8", sequenceLength: 1, beatDivision: 1 },
  };
  static constexpr size_t channelCount = sizeof(channels)/sizeof(OutputChannel);
public:
  OutputController() {
    for (uint8_t i=0; i<channelCount; ++i) {
      OutputChannel &channel = channels[i];
      channel.sequenceLength = 8;
    }
  }

  void setStepData(ChannelID channelId, StepIndex stepIndex, OutputLevel startLevel, OutputLevel endLevel) {
    if( channelId >= channelCount ) {
      Serial.print("# Channel index out of bounds: ");
      Serial.println(channelId);
      return;
    }
    if( stepIndex >= MAX_SEQUENCE_LENGTH ) {
      Serial.print("# Step index out of bounds: ");
      Serial.println(stepIndex);
      return;
    }
    StepData &step = channels[channelId].stepData[stepIndex];
    step.startLevel = startLevel;
    step.endLevel = endLevel;
  }

  void setBpm( uint16_t bpm, uint8_t subBpm=0 ) {
    this->bpmHigh = bpm;
    this->bpmLow = subBpm;
  }

  void setBeatDivision( ChannelID channelId, uint8_t beatDivision ) {
    if( channelId >= channelCount ) {
      Serial.print("# Channel index out of bounds: ");
      Serial.println(channelId);
      return;
    }
    this->channels[channelId].beatDivision = beatDivision;
  }

  void setSequenceLength( ChannelID channelId, SequenceLength sequenceLength ) {
    if( channelId >= channelCount ) {
      Serial.print("# Channel index out of bounds: ");
      Serial.println(channelId);
      return;
    }
    if( sequenceLength > MAX_SEQUENCE_LENGTH ) {
      Serial.print("# Sequence length out of bounds: ");
      Serial.println(sequenceLength);
      Serial.print("# Max sequence length: ");
      Serial.println(MAX_SEQUENCE_LENGTH);
      return;
    }
    this->channels[channelId].sequenceLength = sequenceLength;
  }

  void begin() {
    this->startTime = tickStartTime;
    for( unsigned int i=0; i<channelCount; ++i ) {
      OutputChannel &channel = channels[i];
      channel.sequenceLength = i+1;
      channel.stepData[i].startLevel = 255;
      channel.stepData[i].endLevel = 0;
      pinMode(channel.port, OUTPUT);
    }
  }
  
  void printChannelInfo() {
    for( unsigned int i=0; i<channelCount; ++i ) {
      OutputChannel &channel = channels[i];
      pinMode(channel.port, OUTPUT);
      Serial.print("# Channel ");
      Serial.print(i);
      Serial.print(" name:");
      Serial.print(channel.name);
      Serial.print(" port:");
      Serial.print(channel.port);
      Serial.print(" data:");
      for( unsigned int j=0; j<channel.sequenceLength; ++j ) {
        if( j>0 ) Serial.print(";");
        Serial.print(channel.stepData[j].startLevel);
        Serial.print(",");
        Serial.print(channel.stepData[j].endLevel);
      }
      Serial.println("");
    }
  }
  
  void tick() {
    // Index in 256ths of a beat
    uint32_t index = (tickStartTime - this->startTime) * ((uint32_t(this->bpmHigh) << 8) + this->bpmLow) / 60000;
    uint8_t tickHash = uint8_t(tickNumber) ^ uint8_t(tickStartTime);
    for( unsigned int i=0; i<channelCount; ++i ) {
      OutputChannel &channel = channels[i];
      uint8_t stepIndex = ((index * channel.beatDivision) >> 8) % channel.sequenceLength;
      uint8_t subStepIndex = uint8_t(index * channel.beatDivision);
      StepData &step = channel.stepData[stepIndex];
      uint8_t level = (
        uint16_t(step.startLevel) * (255-subStepIndex) + 
        uint16_t(step.endLevel) * (subStepIndex)) >> 9;
      //unsigned int div = ((swich.cycleOffset + tick) % swich.cycleLength) * swich.cycleDivisions / swich.cycleLength;
      //digitalWrite(swich.port, swich.cycleData[div] ? HIGH : LOW);
      digitalWrite(channel.port, (level > tickHash ? HIGH : LOW));
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


//// ArduForth

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
    const char *text;
    const char *help;
    
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
  void popAndDiscard() {
    pop();
  }
  void subtractIntsFromStack() {
    int b = pop();
    int a = pop();
    push( a - b );
  }
  void setStepData() {
    OutputController::OutputLevel endLevel = (OutputController::OutputLevel)pop();
    OutputController::OutputLevel startLevel = (OutputController::OutputLevel)pop();
    OutputController::StepIndex stepIndex = (OutputController::StepIndex)pop();
    OutputController::ChannelID channelId = (OutputController::ChannelID)pop();
    outputController.setStepData(channelId, stepIndex, startLevel, endLevel);
  }
  void setSequenceLength() {
    OutputController::SequenceLength sequenceLength = (OutputController::SequenceLength)pop();
    OutputController::ChannelID channelId = (OutputController::ChannelID)pop();
    outputController.setSequenceLength(channelId, sequenceLength);
  }
  void setBpm() {
    //uint8_t beat256ths = (uint8_t)pop();
    uint16_t beats = (uint8_t)pop();
    outputController.setBpm( beats );
  }
  void setBeatDivision() {
    uint8_t notesPerBeat = (uint8_t)pop();
    OutputController::ChannelID channelId = (OutputController::ChannelID)pop();
    outputController.setBeatDivision( channelId, notesPerBeat );
  }
  void downbeat() {
    outputController.startTime = millis();
  }
  void printChannelInfo() {
    outputController.printChannelInfo();
  }
  void printHelp();
  
  const Word staticWords[] = {
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: pushStackFree
      },
      text: "stack-free",
      help: "( -- n ) where n is the amount of available space on the stack, in words"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: popAndDiscard
      },
      text: "pop",
      help: "( x -- )"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: printStack
      },
      text: ".s",
      help: "( -- ) print stack contents"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: printIntFromStack
      },
      text: ".",
      help: "( i -- ) prints out the int, in decimal"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: addIntsFromStack
      },
      text: "+",
      help: "( a b -- a+b )"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: subtractIntsFromStack
      },
      text: "-",
      help: "( a b -- a-b )"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: setStepData
      },
      text: "set-step-data",
      help: "( channelId stepIndex startValue endValue -- )"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: setSequenceLength
      },
      text: "set-sequence-length",
      help: "( channelId length -- ) set length of channel's sequence, in steps"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: setBpm
      },
      text: "set-bpm",
      help: "( bpm -- ) set beats per minute"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: setBeatDivision
      },
      text: "set-beat-division",
      help: "( stepsPerBeat -- )"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: downbeat
      },
      text: "downbeat",
      help: "( -- ) restart sequence NOW"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: printStatus
      },
      text: "status",
      help: "( -- ) print various information about this device"
    },
    {
      isCompileTime: false,
      isNative: true,
      implementation: {
        nativeFunction: printHelp
      },
      text: "help",
      help: "( -- ) print information about forth commands"
    }
  };
  
  ////
  
  const Dictionary<const Word> staticDict = {
    previous: NULL,
    begin: staticWords,
    end: staticWords + (int)(sizeof(staticWords)/sizeof(Word))
  };

  void printHelp() {
    Serial.println("# Forth commands:");
    for( const Word *w = staticDict.begin; w < staticDict.end; ++w ) {
      Serial.print("# ");
      Serial.print(w->text);
      Serial.print(" ");
      Serial.println(w->help);
    }
  }
  
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

  void printPrompt() {
    Serial.print("# Stack: ");
    printStack();
  }
  
  void handleChar( char c ) {
    // TODO: Handle double-quoted strings
    switch( c ) {
    case '\n':
      flushToken();
      printPrompt();
      break;
    case ' ': case '\t': case '\r':
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
  tickStartTime = millis();
  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);
  for( unsigned int d=0; d<5; ++d ) {
    digitalWrite(LED_BUILTIN, LOW); // Light on
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH); // Light off
    delay(200);
  }
  Serial.begin(115200);
  Serial.println("");
  Serial.println("# Henlo, this is ArduinoLightController, booting!");
  delay(500);
  outputController.begin();
  outputController.printChannelInfo();
  Serial.println("# Type 'help' for a command list.");
}

void loop() {
  tickStartTime = millis();

  maintainWiFiConnection();
  
  while( Serial.available() > 0 ) {
    ArduForth::handleChar(Serial.read());
  }
  
  if(needDelay) {
    // Some things seem to not work without a delay somewhere.
    // So after kicking those things off, set needDelay = true.
    delay(1);
    needDelay = false;
  }

  outputController.tick();
  ++tickNumber;
}


