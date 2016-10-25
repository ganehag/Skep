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
  // FIXME: better parser for invalid keys
  char *c = key.c_str();
  lastMethod = c[0];
  lastOffset = atoi(&c[1]);
}

void MessageParser::value(String value) {
  int intval;

  if(value.equals("true")) {
    intval = 1;
  } else if(value.equals("false")) {
    intval = 0;
  } else {
    intval = value.toInt();
  }

  switch(lastMethod) {
    case 'c':
      mb.Coil(lastOffset, intval);
    break;
    case 'h':
      mb.Hreg(lastOffset, intval);
    break;
    case 's':
      mb.Ists(lastOffset, intval);
    break;
    case 'r':
      mb.Ireg(lastOffset, intval);
    break;
  }

  Serial.print("key: ");
  Serial.print(lastMethod, HEX);
  Serial.println();
  Serial.println("value: " + value);
}

void MessageParser::endArray() {

}

void MessageParser::endObject() {

}

void MessageParser::endDocument() {

}

void MessageParser::startArray() {

}

void MessageParser::startObject() {

}

