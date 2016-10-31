#pragma once

#include <Modbus.h>
#include <ModbusIP2.h>

#include <JsonListener.h>


#define htons(x) ( ((x)<<8) | (((x)>>8)&0xFF) )
#define ntohs(x) htons(x)
#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )
#define ntohl(x) htonl(x)


#define htonw(x) ( ((x)<<16) | (((x)>>16)&0xFFFF) )

enum KeyType { NONE, ADDR, TYPE, VALUE };
enum ObjectType { NOOBJ, COIL, DISCRETE, IREG, HREG };
enum MType { UNDEF, BOOL, INT16, INT32, FLOAT };

struct Mreg {
  MType s_type;
  String temp_value;
  union {
    uint16_t s_shorts[2];
    uint32_t s_int;
    float s_float;
  };
};

struct Message {
  int32_t addr;
  ObjectType object;
  struct Mreg data;
};

extern ModbusIP mb;

class MessageParser: public JsonListener {
  private:
    char lastMethod;
    int lastOffset;
    struct Message msg;
    enum KeyType kt;

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
