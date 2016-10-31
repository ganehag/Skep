#include "MessageParser.h"

ModbusIP mb;

MessageParser::MessageParser() {
  lastMethod = '\0';
  lastOffset = 0;
}

void MessageParser::whitespace(char c) {

}

void MessageParser::startDocument() {

}

void MessageParser::key(String key) {
  if(key == "addr") {
    kt = ADDR;
  } else if(key == "type") {
    kt = TYPE;
  } else if(key == "value") {
    kt = VALUE;
  } else {
    kt = NONE;
  }
}

void MessageParser::value(String value) {
  int32_t addr;

  switch(kt) {
    case ADDR:
      addr = value.toInt();
      if(value.startsWith("0") && addr > 0 && addr <= 65536) {
        msg.object = COIL;
      } else if(value.startsWith("1") && addr > 100000 && addr <= 165536) {
        msg.object = DISCRETE;
        addr -= 100001;
      } else if(value.startsWith("3") && addr > 300000 && addr <= 365536) {
        msg.object = IREG;
        addr -= 300001;
      } else if(value.startsWith("4") && addr > 400000 && addr <= 465536) {
        msg.object = HREG;
        addr -= 400001;
      }
      msg.addr = addr;
    break;
    case TYPE:
      if(value == "bool") {
        msg.data.s_type = BOOL;
      } else if(value == "int16") {
        msg.data.s_type = INT16;
      } else if(value == "int32") {
        msg.data.s_type = INT32;
      } else if(value == "float") {
        msg.data.s_type = FLOAT;
      } else {
        msg.data.s_type = UNDEF;
      }
    break;
    case VALUE:
      msg.data.temp_value = value;
    break;
  }
}

void MessageParser::endArray() {

}

void MessageParser::endObject() {
  if(msg.addr < 0) {
    return;
  }

  /*
   * MQTT can only write to discrete inputs and input registers
   * coils and holding registers are expected to be manipulated from
   * the Modbus side.
   * 
   * Fixme: add an argument in the JSON data to allow cols & holding regs
   */
  
  switch(msg.data.s_type) {
    case BOOL:
      if(msg.data.temp_value.equals("true")) {
        msg.data.s_int = 1;
      } else if(msg.data.temp_value.equals("false")) {
        msg.data.s_int = 0;
      } else {
        msg.data.s_int = msg.data.temp_value.toInt();
      }
      if(msg.object == NOOBJ) {
        msg.object = DISCRETE;
      }
    break;
    case INT16:
      msg.data.s_int = msg.data.temp_value.toInt();
    break;
    case INT32:
      msg.data.s_int = htonw(msg.data.temp_value.toInt());
    break;
    case FLOAT:
      msg.data.s_float = msg.data.temp_value.toFloat();
      msg.data.s_int = htonw(msg.data.s_int);
    break;
  }

  if(msg.object == NOOBJ) {
    msg.object = IREG;
  }

  switch(msg.object) {
    case COIL:
      mb.Coil(msg.addr, msg.data.s_int);
    break;
    case DISCRETE:
      mb.Ists(msg.addr, msg.data.s_int);
    break;
    case IREG:
      if(msg.data.s_type == BOOL || msg.data.s_type == INT16) {
        mb.Ireg(msg.addr, msg.data.s_int);
      } else {
        mb.Ireg(msg.addr, msg.data.s_shorts[0]);
        mb.Ireg(msg.addr + 1, msg.data.s_shorts[1]);
      }
    break;
    case HREG:
      if(msg.data.s_type == BOOL || msg.data.s_type == INT16) {
        mb.Hreg(msg.addr, msg.data.s_int);
      } else {
        mb.Hreg(msg.addr, msg.data.s_shorts[0]);
        mb.Hreg(msg.addr + 1, msg.data.s_shorts[1]);
      }
    break;
  }

  Serial.println("Addr:" + String(msg.addr));
  Serial.println("value: " + msg.data.temp_value);
}

void MessageParser::endDocument() {

}

void MessageParser::startArray() {

}

void MessageParser::startObject() {
  msg.addr = -1;
  msg.object = NOOBJ;
  msg.data.s_type = UNDEF;
  msg.data.temp_value = String("");
  msg.data.s_int = 0;
}

