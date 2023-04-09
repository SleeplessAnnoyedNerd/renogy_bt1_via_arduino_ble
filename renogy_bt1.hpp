#ifndef _RENOGY_BT1_H_
#define _RENOGY_BT1_H_

#include <Arduino.h>
#include <String.h>

String decodeFunction(byte b);
String decodeChargingState(byte b);
String decodeLoadState(byte b);
int decodeTemperature(byte raw_value);
int bytes_to_int_16(const byte bytes[], int start);
int bytes_to_int_32(const byte bytes[], int start);
String *decodeValues(const byte bytes[]);
String buildJson(String decodedValues[]);

#endif
