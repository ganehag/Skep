#include <SPI.h>
#include <Ethernet2.h>
#include <JsonStreamingParser.h>
#include <JsonListener.h>
#include <MemoryFree.h>
#include <MQTT.h>
#include <TimeAlarms.h>
#include <SD.h>

#include "MessageParser.h"

#define MB_BASE_ADDR 0x00

struct PubRegs {
  int32_t start;
  int32_t stop;
  enum ObjectType type;
  enum MType size;
  int loaded;
};

struct PubRegs pubregs[10];

static void callback(char* topic, byte* payload, unsigned int length);

File settingsFile;

int num_coil = 10;
int num_discrete = 10;
int num_ireg = 10;
int num_hreg = 10;
byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
byte ip[] = {192, 168, 254, 254};
byte netmask[] = {255, 255, 0, 0};
byte gw[] = {192, 168, 0, 1};
byte dns_ip[] = {192, 168, 0, 1};
byte mqtt_gateway[] = {192, 168, 0, 1};
int publish_interval = 60;

String in_topic = "intopic/message";
String out_topic = "outtopic/message";

EthernetClient ethclient;
MQTT client(mqtt_gateway, 1883, callback, ethclient);

MessageParser listener;


static void callback(char* topic, byte* payload, unsigned int length) {
  JsonStreamingParser parser;
  parser.setListener(&listener);

  for (int i = 0; i < length; i++) {
    parser.parse(payload[i]);
  }

  Alarm.delay(10);
}

void parseBytes(const char* str, char sep, byte* bytes, int maxBytes, int base) {
    for (int i = 0; i < maxBytes; i++) {
        bytes[i] = strtoul(str, NULL, base);  // Convert byte
        str = strchr(str, sep);               // Find next separator
        if (str == NULL || *str == '\0') {
            break;                            // No more separators, exit
        }
        str++;                                // Point to next character after separator
    }
}

static void connectToMQTT() {
  // connect to the server
  client.connect("mqtt2modbus");

  if (client.isConnected()) {
    client.subscribe(in_topic.c_str());
  }
}

static void IntervalPublish() {
  for(int i=0; i < sizeof(pubregs)/sizeof(struct PubRegs); i++) {
    if(pubregs[i].loaded == 0) {
      continue;
    }

    String data = "[";
    char sAddr[8];
    int addrIncr = 1;

    switch(pubregs[i].type) {
      case COIL:
        snprintf(sAddr, sizeof(sAddr), "0%05d", pubregs[i].start+1);
        for(int addr=pubregs[i].start; addr <= pubregs[i].stop; addr++) {
          data.concat(mb.Coil(addr));
          if(addr < pubregs[i].stop) {
            data.concat(",");
          }
        }
      break;
      case DISCRETE:
        snprintf(sAddr, sizeof(sAddr), "1%05d", pubregs[i].start+1);
        for(int addr=pubregs[i].start; addr <= pubregs[i].stop; addr++) {
          data.concat(mb.Ists(addr));
          if(addr < pubregs[i].stop) {
            data.concat(",");
          }
        }
      break;
      case IREG:
        snprintf(sAddr, sizeof(sAddr), "3%05d", pubregs[i].start+1);
        if(pubregs[i].size == INT32 || pubregs[i].size == FLOAT) {
          addrIncr = 2;
        }

        for(int addr = pubregs[i].start; addr <= pubregs[i].stop; addr += addrIncr) {
          if(addrIncr == 1) {
            data.concat(mb.Ireg(addr));
          } else if(addrIncr == 2) {
            struct Mreg mrg;
            mrg.s_shorts[0] = mb.Ireg(addr);
            mrg.s_shorts[1] = mb.Ireg(addr + 1);
            mrg.s_int = htonw(mrg.s_int);
            if(pubregs[i].size == INT32) {
              data.concat(mrg.s_int);
            } else {
              data.concat(mrg.s_float);
            }
          }
          if(addr + addrIncr < pubregs[i].stop) {
            data.concat(",");
          }
        }
      break;
      case HREG:
        snprintf(sAddr, sizeof(sAddr), "4%05d", pubregs[i].start+1);
        if(pubregs[i].size == INT32 || pubregs[i].size == FLOAT) {
          addrIncr = 2;
        }

        for(int addr = pubregs[i].start; addr <= pubregs[i].stop; addr += addrIncr) {
          if(addrIncr == 1) {
            data.concat(mb.Hreg(addr));
          } else if(addrIncr == 2) {
            struct Mreg mrg;
            mrg.s_shorts[0] = mb.Hreg(addr);
            mrg.s_shorts[1] = mb.Hreg(addr + 1);
            mrg.s_int = htonw(mrg.s_int);
            if(pubregs[i].size == INT32) {
              data.concat(mrg.s_int);
            } else {
              data.concat(mrg.s_float);
            }
          }
          if(addr + addrIncr < pubregs[i].stop) {
            data.concat(",");
          }
        }
      break;
    }

    data.concat("]");

    String message = "{\"addr\": \"" + String(sAddr) + "\",\"data\": " + data + "}";

    if (client.isConnected()) {
      client.publish(out_topic.c_str(), message.c_str(), MQTT::QOS1);
    }
  }
}

static void Repeats() {
  char c[32];

  snprintf(c, sizeof(c), "{\"freemem\": %i}", freeMemory());

  if (client.isConnected()) {
    client.publish(out_topic.c_str(), c);
  }
}

static void applySetting(String name, String value) {
  // Serial.print(name); Serial.print(": "); Serial.print(value); Serial.println("");

  /* Network */
  if(name == "mac") {
    parseBytes(value.c_str(), '-', mac, 6, 16);
  } else if(name == "ip") {
    parseBytes(value.c_str(), '.', ip, 4, 10);
  } else if(name == "netmask") {
    parseBytes(value.c_str(), '.', netmask, 4, 10);
  } else if(name == "gw") {
    parseBytes(value.c_str(), '.', gw, 4, 10);
  } else if(name == "mqtt_gw") {
    parseBytes(value.c_str(), '.', mqtt_gateway, 4, 10);
  } else if(name == "dns") {
    parseBytes(value.c_str(), '.', dns_ip, 4, 10);
  }
  /* Records */
  else if(name == "coils") {
    num_coil = value.toInt();
  } else if(name == "discretes") {
    num_discrete = value.toInt();
  } else if(name == "iregs") {
    num_ireg = value.toInt();
  } else if(name == "hregs") {
    num_hreg = value.toInt();
  }
  /* Topics */
  else if(name == "intopic") {
    in_topic = value;
  } else if(name == "outtopic") {
    out_topic = value;
  } else if(name == "pub_interval") {
    publish_interval = value.toInt();
  } else if(name == "publish") {
    int ret;
    int pubr = 0;
    char val1[8];
    char val2[8];
    char sval[16];
    char *str_p = strchr(value.c_str(), '\n');

    while(str_p[0] == '\n') {
      str_p++;
    }

    while((ret = sscanf(str_p, "%[^-]-%[^,],%s", &val1, &val2, &sval)) == 3) {
      struct PubRegs *pr = &pubregs[pubr];

      // Serial.println(String(val1) + " " + String(val2) + " " + String(sval));

      if(ret == 3 && pubr < sizeof(pubregs)/sizeof(struct PubRegs)) {
        pubr++;
        pr->loaded = 1;
        pr->start = String(val1).toInt();
        pr->stop = String(val2).toInt();

        switch(val1[0]) {
          case '0':
            pr->type = COIL;
            pr->start -= 1;
            pr->stop -= 1;
          break;
          case '1':
            pr->type = DISCRETE;
            pr->start -= 100001;
            pr->stop -= 100001;
          break;
          case '3':
            pr->type = IREG;
            pr->start -= 300001;
            pr->stop -= 300001;
          break;
          case '4':
            pr->type = HREG;
            pr->start -= 400001;
            pr->stop -= 400001;
          break;
        }

        if(strcmp(sval, "bool") == 0) {
          pr->size = BOOL;
        } else if(strcmp(sval, "int16") == 0) {
          pr->size = INT16;
        } else if(strcmp(sval, "int32") == 0) {
          pr->size = INT32;
        } else if(strcmp(sval, "float") == 0) {
          pr->size = FLOAT;
        }
      }

      str_p = strchr(str_p, '\n');
      if(str_p != NULL) {
        str_p++;
      }
    }
  }
}

static void readSDSettings() {
  char character;
  String settingName;
  String settingValue;
  settingsFile = SD.open("settings.txt");

  if (settingsFile) {
    Serial.println("Parsing settings...");
    
    while (settingsFile.available()) {
      character = settingsFile.read();
      while ((settingsFile.available()) && (character != '[')) {
        character = settingsFile.read();
      }
      character = settingsFile.read();
      while ((settingsFile.available()) && (character != '=')) {
        settingName = settingName + character;
        character = settingsFile.read();
      }
      character = settingsFile.read();
      while ((settingsFile.available()) && (character != ']')) {
        settingValue = settingValue + character;
        character = settingsFile.read();
      }
      if (character == ']') {
        applySetting(settingName, settingValue);
        settingName = "";
        settingValue = "";
      }
    }

    settingsFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening settings.txt");
  }
}

void setup() {
  memset(pubregs, sizeof(pubregs), 0);
  
  Serial.begin(9600);

  Serial.println("Initializing SD card...");
  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output
  // or the SD library functions will not work.
  pinMode(53, OUTPUT);
  if (!SD.begin(4)) {
    Serial.println("initialization failed!");
    return;
  }

  readSDSettings();

  // Modbus Code handles Ethernet.begin
  mb.config(mac, ip, dns, gw, netmask);

  Serial.println("Adding..");
  int i;
  for (i = 0; i < num_coil; i++) {
    mb.addCoil(MB_BASE_ADDR + i, false);
  }
  Serial.print(i);
  Serial.println(" coils");
    
  for (i = 0; i < num_discrete; i++) {
    mb.addIsts(MB_BASE_ADDR + i, false);
  }
  Serial.print(i);
  Serial.println(" discretes");

  for (i = 0; i < num_ireg; i++) {
    mb.addIreg(MB_BASE_ADDR + i, 0);
    
  }
  Serial.print(i);
  Serial.println(" input registers");

  for (i = 0; i < num_hreg; i++) {
    mb.addHreg(MB_BASE_ADDR + i, 0);    

  }
  Serial.print(i);
  Serial.println(" holding registers");
    
  setTime(0, 0, 0, 1, 1, 10);
  Alarm.timerRepeat(15, Repeats);
  Alarm.timerRepeat(publish_interval, IntervalPublish);
}

void loop() {
  mb.task();

  if (client.isConnected()) {
    client.loop();
  } else {
    Serial.println("MQTT not connected. Reconnecting");
    connectToMQTT();
    Alarm.delay(1000);
  }

  Alarm.delay(1);
}

