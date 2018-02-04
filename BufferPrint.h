#pragma once

class BufferPrint : public Print {
    char *buffer;
    const size_t bufferSize;
    size_t messageEnd = 0;
  public:
    BufferPrint(char *buffer, size_t bufferSize) : buffer(buffer), bufferSize(bufferSize) {}
    
    void clear() {
      messageEnd = 0;
    }
    
    virtual size_t write(uint8_t c) override {
      if( messageEnd < bufferSize - 1 ) {
        buffer[messageEnd++] = c;
        return 1;
      } else return 0;
    }
    
    size_t write(const uint8_t *stuff, size_t len) override {
      size_t maxLen = bufferSize - messageEnd;
      if( len > maxLen ) len = maxLen;
      while( len-- > 0 ) buffer[messageEnd++] = *stuff++;
    }
    
    virtual int availableForWrite() const {
      return this->bufferSize - this->messageEnd;
    }
    
    const char *getBuffer() const {
      return buffer;
    }
    size_t size() const {
      return messageEnd;
    }
};
