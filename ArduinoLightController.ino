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

unsigned int tick;
bool needDelay;

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
  tick = 0;
}

void loop() {
  while( Serial.available() > 0 ) {
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
    digitalWrite(swich.port, (tick % swich.cycleLength) < swich.cycleOnLength ? HIGH : LOW);
  }
  ++tick;
}


