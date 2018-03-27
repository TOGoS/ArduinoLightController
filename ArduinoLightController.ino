#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "BufferPrint.h"
#include "hexDigit.h"
#include "printMacAddressHex.h"
#include "config.h"
#include "version.h"
#include "WiFiMaintainer.h"

long tickStartTime = -10000;
uint8_t tickNumber = 0;
bool needDelay;

char printBuffer[1024];
BufferPrint bufferPrinter(printBuffer, sizeof(printBuffer));

Print *defaultPrinter = &Serial;

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
    static constexpr size_t channelCount = sizeof(channels) / sizeof(OutputChannel);
  public:
    OutputController() {
      for (uint8_t i = 0; i < channelCount; ++i) {
        OutputChannel &channel = channels[i];
        channel.sequenceLength = 8;
      }
    }
    
    void setStepData(ChannelID channelId, StepIndex stepIndex, OutputLevel startLevel, OutputLevel endLevel) {
      if( channelId >= channelCount ) {
        defaultPrinter->print("# Channel index out of bounds: ");
        defaultPrinter->println(channelId);
        return;
      }
      if( stepIndex >= MAX_SEQUENCE_LENGTH ) {
        defaultPrinter->print("# Step index out of bounds: ");
        defaultPrinter->println(stepIndex);
        return;
      }
      StepData &step = channels[channelId].stepData[stepIndex];
      step.startLevel = startLevel;
      step.endLevel = endLevel;
    }
    
    void setBpm( uint16_t bpm, uint8_t subBpm = 0 ) {
      this->bpmHigh = bpm;
      this->bpmLow = subBpm;
    }
    
    void setBeatDivision( ChannelID channelId, uint8_t beatDivision ) {
      if( channelId >= channelCount ) {
        defaultPrinter->print("# Channel index out of bounds: ");
        defaultPrinter->println(channelId);
        return;
      }
      this->channels[channelId].beatDivision = beatDivision;
    }
    
    void setSequenceLength( ChannelID channelId, SequenceLength sequenceLength ) {
      if( channelId >= channelCount ) {
        defaultPrinter->print("# Channel index out of bounds: ");
        defaultPrinter->println(channelId);
        return;
      }
      if( sequenceLength > MAX_SEQUENCE_LENGTH ) {
        defaultPrinter->print("# Sequence length out of bounds: ");
        defaultPrinter->println(sequenceLength);
        defaultPrinter->print("# Max sequence length: ");
        defaultPrinter->println(MAX_SEQUENCE_LENGTH);
        return;
      }
      this->channels[channelId].sequenceLength = sequenceLength;
    }
    
    void loadDefaultSequence() {
      // Long bass drum
      channels[0].sequenceLength = 1;
      channels[0].beatDivision = 1;
      setStepData(0, 0, 255, 0);
      // Hihats
      channels[1].sequenceLength = 4;
      channels[1].beatDivision = 4;
      setStepData(1, 0, 0, 0);
      setStepData(1, 1, 0, 0);
      setStepData(1, 2, 255, 0);
      setStepData(1, 3, 255, 0);
      // Different hihat
      channels[2].sequenceLength = 4;
      channels[2].beatDivision = 4;
      setStepData(2, 0, 0, 0);
      setStepData(2, 1, 0, 0);
      setStepData(2, 2, 255, 255);
      setStepData(2, 3, 0, 0);
      // 3 = solid off
      channels[3].sequenceLength = 1;
      channels[3].beatDivision = 1;
      setStepData(3, 0, 0, 0);
      // thing that goes every 3
      channels[4].sequenceLength = 3;
      channels[4].beatDivision = 4;
      setStepData(4, 0, 0, 0);
      setStepData(4, 1, 0, 0);
      setStepData(4, 2, 255, 128);
      // thing that goes every 5
      channels[5].sequenceLength = 5;
      channels[5].beatDivision = 4;
      setStepData(5, 0, 0, 0);
      setStepData(5, 1, 0, 0);
      setStepData(5, 2, 0, 0);
      setStepData(5, 3, 0, 0);
      setStepData(5, 4, 255, 128);
      // snare drum!
      channels[6].sequenceLength = 8;
      channels[6].beatDivision = 4;
      setStepData(4, 0, 0, 0);
      setStepData(4, 1, 0, 0);
      setStepData(4, 2, 0, 0);
      setStepData(4, 3, 0, 0);
      setStepData(4, 4, 255, 0);
      setStepData(4, 5, 0, 0);
      setStepData(4, 6, 0, 0);
      setStepData(4, 7, 0, 0);
      // 7 = solid on
      channels[7].sequenceLength = 1;
      channels[7].beatDivision = 1;
      setStepData(7, 0, 255, 255);
    }

    void begin() {
      this->startTime = tickStartTime;
      for( unsigned int i = 0; i < channelCount; ++i ) {
        OutputChannel &channel = channels[i];
        pinMode(channel.port, OUTPUT);
      }
    }
    
    void printChannelInfo() {
      defaultPrinter->print("# LED_BUILTIN port:");
      defaultPrinter->println(LED_BUILTIN);
      for( unsigned int i = 0; i < channelCount; ++i ) {
        OutputChannel &channel = channels[i];
        pinMode(channel.port, OUTPUT);
        defaultPrinter->print("# Channel ");
        defaultPrinter->print(i);
        defaultPrinter->print(" name:");
        defaultPrinter->print(channel.name);
        defaultPrinter->print(" port:");
        defaultPrinter->print(channel.port);
        defaultPrinter->print(" data:");
        for( unsigned int j = 0; j < channel.sequenceLength; ++j ) {
          if( j > 0 ) defaultPrinter->print(";");
          defaultPrinter->print(channel.stepData[j].startLevel);
          defaultPrinter->print(",");
          defaultPrinter->print(channel.stepData[j].endLevel);
        }
        defaultPrinter->println("");
      }
    }
    
    void tick() {
      // Index in 256ths of a beat
      uint32_t index = (tickStartTime - this->startTime) * ((uint32_t(this->bpmHigh) << 8) + this->bpmLow) / 60000;
      uint8_t tickHash = uint8_t(tickNumber) ^ uint8_t(tickStartTime);
      for( unsigned int i = 0; i < channelCount; ++i ) {
        OutputChannel &channel = channels[i];
        uint8_t stepIndex = ((index * channel.beatDivision) >> 8) % channel.sequenceLength;
        uint8_t subStepIndex = uint8_t(index * channel.beatDivision);
        StepData &step = channel.stepData[stepIndex];
        uint8_t level = (
                          uint16_t(step.startLevel) * (255 - subStepIndex) +
                          uint16_t(step.endLevel) * (subStepIndex)) >> 9;
        //unsigned int div = ((swich.cycleOffset + tick) % swich.cycleLength) * swich.cycleDivisions / swich.cycleLength;
        //digitalWrite(swich.port, swich.cycleData[div] ? HIGH : LOW);
        digitalWrite(channel.port, (level > tickHash ? HIGH : LOW));
      }
      ++tickNumber;
    }
};
OutputController outputController;

byte macAddressBuffer[6];

void reportWiFiStatusTo(int wiFiStatus, class Print &printer = *defaultPrinter) {
  printer.print("# WiFi status: ");
  switch( wiFiStatus ) {
  case WL_CONNECTED:
    printer.println("WL_CONNECTED");
    printer.print("# SSID: ");
    printer.println(WiFi.SSID());
    printer.print("# IP address: ");
    printer.println(WiFi.localIP());
    printer.print("# Subnet mask: ");
    printer.println(WiFi.subnetMask());
    printer.print("# MAC address: ");
    WiFi.macAddress(macAddressBuffer);
    printMacAddressHex(macAddressBuffer, ":", printer);
    printer.println();
    break;
  case WL_NO_SHIELD:
    printer.println("WL_NO_SHIELD");
    break;
  case WL_IDLE_STATUS:
    printer.println("WL_IDLE_STATUS");
    break;
  case WL_NO_SSID_AVAIL:
    printer.println("WL_NO_SSID_AVAIL");
    break;
  case WL_SCAN_COMPLETED:
    printer.println("WL_SCAN_COMPLETED");
    break;
  case WL_CONNECT_FAILED:
    printer.println("WL_CONNECT_FAILED");
    break;
  case WL_CONNECTION_LOST:
    printer.println("WL_CONNECTION_LOST");
    break;
  case WL_DISCONNECTED:
    printer.println("WL_DISCONNECTED");
    break;
  default:
    printer.println(wiFiStatus);
  }
}

void reportWiFiStatus(int wiFiStatus) {
  reportWiFiStatusTo(wiFiStatus, *defaultPrinter);
}

void printStatus() {
  reportWiFiStatus(WiFi.status());
  outputController.printChannelInfo();
}

WiFiUDP udp;
long lastBroadcastTime = -10000;

void broadcastPresence() {
  IPAddress broadcastIp = WiFi.localIP() | ~WiFi.subnetMask();
  bufferPrinter.clear();
  bufferPrinter.print("# Hello from ");
  printMacAddressHex(macAddressBuffer, ":", bufferPrinter);
  bufferPrinter.println();
  bufferPrinter.print("# I'm running ");
  bufferPrinter.print(ALC_NAME);
  bufferPrinter.print(" v");
  bufferPrinter.println(ALC_VERSION);
  if( FORTH_UDP_PORT ) {
    bufferPrinter.print("# Listening for forth commands on ");
    bufferPrinter.print(WiFi.localIP());
    bufferPrinter.print(":");
    bufferPrinter.println(FORTH_UDP_PORT);
  }
  //Serial.print("# Broadcasting presense packet to ");
  //Serial.print(broadcastIp);
  //Serial.print(":");
  //Serial.println(PRESENCE_BROADCAST_PORT);
  udp.beginPacketMulticast(broadcastIp, PRESENCE_BROADCAST_PORT, WiFi.localIP());
  udp.write(bufferPrinter.getBuffer(), bufferPrinter.size());
  udp.endPacket();
  lastBroadcastTime = tickStartTime;
}

void maybeBroadcastPresence() {
  if( WiFi.status() == WL_CONNECTED && tickStartTime - lastBroadcastTime > 10000 && PRESENCE_BROADCAST_PORT ) {
    broadcastPresence();
  }
}


//// ArduForth

class ArduForth {
  public:
    static const byte MAX_TOKEN_LENGTH = 32;
    static const byte MAX_STACK_DEPTH = 32;
    
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
    
    void reset() {
      this->tokenBufferLength = 0;
      this->stackBottom = MAX_STACK_DEPTH;
    }
    
    struct Word {
      boolean isCompileTime;
      boolean isNative;
      union {
        void (ArduForth::*nativeFunction)();
        Word *forthFunction;
      } implementation;
      const char *text;
      const char *help;
      
      void run( class ArduForth& arduForth, boolean compileTime ) const {
        if( compileTime && !isCompileTime ) {
          Serial.print("# Word '");
          Serial.print(text);
          Serial.println("' is not a compile-time word, so ignoring at compile-time.");
          // TODO: Add to definition
        } else {
          if( isNative ) {
            (arduForth.*(implementation.nativeFunction))();
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
        for( Item *i = end - 1; i >= begin; --i ) {
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
        defaultPrinter->print("# Error: stack full at ");
        defaultPrinter->print(MAX_STACK_DEPTH);
        defaultPrinter->print(" items; cannot push value: ");
        defaultPrinter->println(v);
      } else {
        stack[--stackBottom] = v;
      }
    }
    
    int pop() {
      if( stackBottom == MAX_STACK_DEPTH ) {
        defaultPrinter->println("# Error: stack underflow");
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
        defaultPrinter->print( stack[i] );
        defaultPrinter->print( " " );
      }
      defaultPrinter->println();
    }
    void printIntFromStack() {
      defaultPrinter->println( pop() );
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
    void loadDefaultSequence() {
      outputController.loadDefaultSequence();
      defaultPrinter->println("# Reset sequence to 'factory' default");
    }
    void printChannelInfo() {
      outputController.printChannelInfo();
    }
    void _broadcastPresence() { broadcastPresence(); }
    void _printStatus() { printStatus(); }
    void printHelp();
    
    static const Word staticWords[];
    
    ////
    
    static const Dictionary<const Word> staticDict;
    
    void handleWord( char *buffer, int len ) {
      boolean isInteger = true;
      int intVal = 0;
      for( int i = 0; i < len; ++i ) {
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
        word->run(*this, false);
        return;
      }
      
      defaultPrinter->print("# Error: unrecognised word: ");
      defaultPrinter->println(tokenBuffer);
    }
    
    void flushToken() {
      tokenBuffer[tokenBufferLength] = 0;
      if( tokenBufferLength > 0 ) {
        handleWord( tokenBuffer, tokenBufferLength );
      }
      tokenBufferLength = 0;
    }
    
    void printPrompt() {
      defaultPrinter->print("# Stack: ");
      printStack();
    }
    
    void handleChar( char c ) {
      // TODO: Handle double-quoted strings
      switch( c ) {
        case '\n':
          flushToken();
          // printPrompt(); // Don't want infinite chat when 2 get talking!
          break;
        case ' ': case '\t': case '\r':
          flushToken();
          break;
        default:
          tokenBuffer[tokenBufferLength++] = c;
      }
      if( tokenBufferLength >= MAX_TOKEN_LENGTH ) {
        defaultPrinter->println("Token too long.");
      }
    }
};

void ArduForth::printHelp() {
  defaultPrinter->println("# Forth commands:");
  for( const Word *w = staticDict.begin; w < staticDict.end; ++w ) {
    defaultPrinter->print("# ");
    defaultPrinter->print(w->text);
    defaultPrinter->print(" ");
    defaultPrinter->println(w->help);
  }
}

const ArduForth::Word ArduForth::staticWords[] = {
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::pushStackFree
    },
    text: "stack-free",
    help: "( -- n ) where n is the amount of available space on the stack, in words"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::popAndDiscard
    },
    text: "pop",
    help: "( x -- )"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::printStack
    },
    text: ".s",
    help: "( -- ) print stack contents"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::printIntFromStack
    },
    text: ".",
    help: "( i -- ) prints out the int, in decimal"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::addIntsFromStack
    },
    text: "+",
    help: "( a b -- a+b )"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::subtractIntsFromStack
    },
    text: "-",
    help: "( a b -- a-b )"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::setStepData
    },
    text: "set-step-data",
    help: "( channelId stepIndex startValue endValue -- )"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::setSequenceLength
    },
    text: "set-sequence-length",
    help: "( channelId length -- ) set length of channel's sequence, in steps"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::loadDefaultSequence
    },
    text: "default>sequence",
    help: "( -- ) reset to default sequence"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::setBpm
    },
    text: "set-bpm",
    help: "( bpm -- ) set beats per minute"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::setBeatDivision
    },
    text: "set-beat-division",
    help: "( stepsPerBeat -- )"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::downbeat
    },
    text: "downbeat",
    help: "( -- ) restart sequence NOW"
  },
  {
    isCompileTime: true,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::_broadcastPresence
    },
    text: "broadcast-presence",
    help: "( -- ) send a broadcast UDP packet (port ""FORTH_UDP_PORT"" to announce the existence of this node"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::_printStatus
    },
    text: "status",
    help: "( -- ) print various information about this device"
  },
  {
    isCompileTime: false,
    isNative: true,
    implementation: {
      nativeFunction: &ArduForth::printHelp
    },
    text: "help",
    help: "( -- ) print information about forth commands"
  }
};
const ArduForth::Dictionary<const ArduForth::Word> ArduForth::staticDict = {
  previous: NULL,
  begin: ArduForth::staticWords,
  end: ArduForth::staticWords + (int)(sizeof(ArduForth::staticWords) / sizeof(ArduForth::Word))
};

ArduForth serialInterpreter;
ArduForth udpInterpreter;

struct WiFiConfig staticWiFiNetworkConfigs[] = WIFI_NETWORKS;
class WiFiMaintainer wiFiMaintainer(staticWiFiNetworkConfigs, sizeof(staticWiFiNetworkConfigs)/sizeof(WiFiConfig));

void setup() {
  tickStartTime = millis();
  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);
  for( unsigned int d = 0; d < 5; ++d ) {
    digitalWrite(LED_BUILTIN, LOW); // Light on
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH); // Light off
    delay(200);
  }
  Serial.begin(115200);
  Serial.println("");
  Serial.print("# Henlo, this is ");
  Serial.print(ALC_NAME);
  Serial.print(" v");
  Serial.print(ALC_VERSION);
  Serial.println(", booting!");
  delay(500);
  outputController.begin();
  outputController.printChannelInfo();
  Serial.println("# Type 'help' for a command list.");
  if( FORTH_UDP_PORT )
    udp.begin(FORTH_UDP_PORT);
}

void loop() {
  tickStartTime = millis();
  defaultPrinter = &Serial;
  
  wiFiMaintainer.maintainWiFiConnection(tickStartTime);
  maybeBroadcastPresence();
  
  while( Serial.available() > 0 ) {
    serialInterpreter.handleChar(Serial.read());
  }
  
  unsigned int packetSize;
  if( FORTH_UDP_PORT && (packetSize = udp.parsePacket()) ) {
    Serial.print("# Received ");
    Serial.print(packetSize);
    //Serial.print("-byte UDP packet on port ");
    //Serial.print(FORTH_UDP_PORT);
    //Serial.print(" from ");
    Serial.print(" bytes from ");
    Serial.print(udp.remoteIP());
    Serial.print(":");
    Serial.print(udp.remotePort());
    Serial.print("...");

    // Handle commands, writing response into the buffer
    defaultPrinter = &bufferPrinter;
    bufferPrinter.clear();
    udpInterpreter.reset();
    while( udp.available() ) {
      char c = udp.read();
      udpInterpreter.handleChar(c);
    }
    udpInterpreter.flushToken();
    udp.flush();
    
    // Write response
    if( bufferPrinter.size() ) {
      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      //if( !bufferPrinter.size() ) bufferPrinter.println("# OK");
      udp.write(bufferPrinter.getBuffer(), bufferPrinter.size());
      udp.endPacket(); // Send it back whence it came!
      Serial.print("responded with ");
      Serial.print(bufferPrinter.size());
      Serial.println("-byte packet");
    } else {
      Serial.println("did not respond");
    }
    
    bufferPrinter.clear();
  }

  if (needDelay) {
    // Some things seem to not work without a delay somewhere.
    // So after kicking those things off, set needDelay = true.
    delay(1);
    needDelay = false;
  }
  
  outputController.tick();
  ++tickNumber;
}
