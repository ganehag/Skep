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

static void callback(char* topic, byte* payload, unsigned int length);

File settingsFile;

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
byte ip[] = {192, 168, 254, 254};
byte netmask[] = {255, 255, 0, 0};
byte gw[] = {192, 168, 0, 1};
byte dns_ip[] = {192, 168, 0, 1};
byte mqtt_gateway[] = {192, 168, 0, 1};

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

  // publish/subscribe
  if (client.isConnected()) {
    client.subscribe(in_topic.c_str());
  }
}

static void Repeats() {
  char c[32];

  snprintf(c, sizeof(c), "{\"freemem\": %i}", freeMemory());

  if (client.isConnected()) {
    client.publish(out_topic.c_str(), c);
  }
}

void applySetting(String name, String value) {
  Serial.print(name); Serial.print(": "); Serial.print(value); Serial.println("");

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
  } else if(name == "intopic") {
    in_topic = value;
  } else if(name == "outtopic") {
    out_topic = value;
  }
}

void readSDSettings() {
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

  for (int i = 0; i < 100; i++) {
    mb.addCoil(MB_BASE_ADDR + i, false);
    mb.addHreg(MB_BASE_ADDR + i, 0);
    mb.addIsts(MB_BASE_ADDR + i, false);
    mb.addIreg(MB_BASE_ADDR + i, 0);
  }

  setTime(0, 0, 0, 1, 1, 10);
  Alarm.timerRepeat(15, Repeats);
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

