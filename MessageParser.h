#pragma once

#include <Modbus.h>
#include <ModbusIP.h>

#include <JsonListener.h>

extern ModbusIP mb;

class MessageParser: public JsonListener {
  private:
    char lastMethod;
    int lastOffset;

  public:
    MessageParser();
    
    virtual void whitespace(char c);
  
    virtual void startDocument();

    virtual void key(String key);

    virtual void value(String value);

    virtual void endArray();

    virtual void endObject();

    virtual void endDocument();

    virtual void startArray();

    virtual void startObject();
};
