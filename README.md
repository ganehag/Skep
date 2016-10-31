# Arduino MQTT to Modbus (slave)
Uses <a href="https://mqtt.org/" target=_blank>MQTT</a> to read and write values in an internal <a href="https://en.wikipedia.org/wiki/Modbus">Modbus</a> slave.

## Hardware requirements
This software is written for an Arduino Mega 2560 with the rev2 Ethernet Shield (Wiznet W5500) from Arduino.org.

## Dependencies
- Arduino 1.6.12 environment (other versions might work/probably works to)
- <a href="https://github.com/adafruit/Ethernet2.git">Ethernet2</a> library
- <a href="https://github.com/hirotakaster/MQTT/tree/arduino">MQTT library for Arduino</a>
- <a href="https://github.com/squix78/json-streaming-parser">json-streaming-parser library
- <a href="https://github.com/McNeight/MemoryFree.git">MemoryFree</a> library
- <a href="https://github.com/wvengen/modbus-arduino">Modbus-Arduino</a> library with Ethernet2 support
- <a href="https://github.com/PaulStoffregen/Time">Time</a> library
- <a href="https://github.com/PaulStoffregen/TimeAlarms">TimeAlarms</a> library

If you want `TCP keep-alive` for the Modbus side you will need to manualy modify the Modbus-Arduino library by going into the ModbusIP2 folder and uncomment the `#define TCP_KEEP_ALIVE` line in the `ModbusIP2.h` file. *It is highly recommended to do this*.

## The software
Acting in one part as a MQTT client this software subscribes to a topic waiting for messages with values to write to its internal Modbus memory. The second part acts as a Modbus slave (server) exposing the values to a Modbus masters (client).

With a set interval, data from the Modbus memory can be published back to the MQTT network to achieve two way communication. Currently this is done using a timer and *all* the data is publishes even if has changed or not. In the future this might change to achieve a more event based solution.

The MQTT side has access to read/write on all Modbus objects in the internal memory. Discretes and Input Registers can thus be used to represent read only values towards the Modbus interface.

## Settings
Settings are stored on a uSD on the Ethernet Shield. Allowing for some changes to be made without having to compile and upload a new firmware.

This is the settings format `[key=value]` and the following keys are allowed:
**Network**
- mac: MAC address of the Ethernet Shield
- ip: IPv4 number
- netmask: IPV4 netmask
- gw: IPv4 gateway
- dns: IPV4 DNS server
- mqtt_gw: IPv4 MQTT server

**Records**
- coils: integer number of coils
- discretes: integer number of discretes
- ireg: integer number of input registers
- hreg: integer number of holding registers

**MQTT stuff**
- intopic: topic to subscribe to
- outtopic: topic to publish messages on
- pub_interval: integer number of seconds for publish interval
- publish: list of Modbus memory addresses separated by `'\n'` on the format:
    + 000001-000029,bool
    + 400003-400004,float
    + 400005-400005,int16
    + 400006-400007,int32

## Data encoding and memory explaination
- coil numbers span from 000001 to 065536
- discrete input numbers span from 100001 to 165536
- input register numbers span from 300001 to 365536
- holding register numbers span from 400001 to 465536

Both `int32` and `float` are encoded as BigEndian in the Modbus memory and are expected to only be in this format when read and published over MQTT.

## MQTT messages
The expected format of a MQTT message is JSON encoded data with the following structure.

**Subscribe**
```json
    {
      "addr": integer or string like "300001",
      "value": true, false, a float value or an integer,
      "type": "bool", "int16", "int32" or "float"
    }
```

The values `true` and `false` are only allowed when the type is set to `bool`.

**Publish**
To save space and to not continously publish a lot of messages. The ranges specified in the `publish` setting will group values together into an array. 

If the publish declaration specified `300001-300008,int32` then four integer values like `[100,200,300,400]` will be published in the message. Only four will be parsed since each Modbus register is 16bit long and a `int32` is 32bit long we read two registers at a time to extract the `ìnt32` value.

> Sending 32 coils as an `int32` is currently not supported. But would be much more optimal from a space saving viewpoint.

If the declaration specified `float` the values at those memory addresses will be parsed as `float` values.

```json
    {
      "addr": Modbus address such as "300001",
      "data": [
        values,
        ...
      ]
    }
```
