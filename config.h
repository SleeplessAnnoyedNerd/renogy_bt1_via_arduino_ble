#ifndef _CONFIG_H_
#define _CONFIG_H_

const bool ENABLE_BLE_DEBUG = false;

const char BLE_DEVICE_NAME[] = "Arduino Nano 33 IoT";
const char BLE_LOCAL_NAME[] = "Arduino Nano 33 IoT";

const char ssid[] = "DoNotConnect28"; // TODO rename
const char pass[] = "89449511"; // TODO rename

const String RENOGY_BT1_MAC_ADDRESS = String("f0:f8:f2:6a:70:03");

const char MQTT_SERVER_IP[] = "192.168.0.210";
const int MQTT_SERVER_PORT = 1883;
const char MQTT_TOPIC[] = "renogy/bt-1";
const char MQTT_CLIENT_ID[] = "Nano33IoT";

#endif
