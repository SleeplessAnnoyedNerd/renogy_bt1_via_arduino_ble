
#include <Arduino.h>
#include <String.h>
#include <hardwareSerial.h>

#include "renogy_bt1.hpp"

String decodeFunction(byte b) {
  if (b == 3) {
    return String("READ");
  } else if (b == 6) {
    return String("WRITE");
  } else {
    return String("UNKNOWN FUNCTION");
  }
}

String decodeChargingState(byte b) {
  const String CHARGING_STATE[] {
    String("deactivated"),
    String("activated"),
    String("mppt"),
    String("equalizing"),
    String("boost"),
    String("floating"),
    String("current limiting")
  };

  return CHARGING_STATE[b]; // TODO check range!
}

String decodeLoadState(byte b) {
  return ((b == 0) ? String("off") : String("on"));
}

int decodeTemperature(byte raw_value) {
  int sign = (raw_value >> 7);

  return ((sign == 1) ? -(raw_value - 128) : raw_value);

  // sign = raw_value >> 7
  // return -(raw_value - 128) if sign == 1 else raw_value
}

int bytes_to_int_16(const byte bytes[], int start) {
  return ((bytes[start] << 8) + bytes[start + 1]);
}

int bytes_to_int_32(const byte bytes[], int start) {
  return ((bytes[start] << 24) + (bytes[start + 1] << 16) + (bytes[start + 2] << 8) + bytes[start + 3]);
}

String *decodeValues(const byte bytes[]) {
  static String decoded_values[] = {
    String(decodeFunction(bytes[1])), // function
    String(bytes_to_int_16(bytes, 3)), // battery_percentage
    String(bytes_to_int_16(bytes, 5) * 0.1), // battery_voltage
    String(bytes_to_int_16(bytes, 7) * 0.01), // battery_current
    String(decodeTemperature(bytes[10])), // battery_temperature
    String(decodeTemperature(bytes[9])), // controller_temperature
    String(decodeLoadState(bytes[67] >> 7)), // load_status
    String(bytes_to_int_16(bytes, 11) * 0.1), // load_voltage
    String(bytes_to_int_16(bytes, 13) * 0.01), // load_current
    String(bytes_to_int_16(bytes, 15)), // load_power
    String(bytes_to_int_16(bytes, 17) * 0.1), // pv_voltage
    String(bytes_to_int_16(bytes, 19) * 0.01), // pv_current
    String(bytes_to_int_16(bytes, 21)), // pv_power
    String(bytes_to_int_16(bytes, 33)), // max_charging_power_today
    String(bytes_to_int_16(bytes, 35)), // max_discharging_power_today
    String(bytes_to_int_16(bytes, 37)), // charging_amp_hours_today
    String(bytes_to_int_16(bytes, 39)), // discharging_amp_hours_today
    String(bytes_to_int_16(bytes, 41)), // power_generation_today
    String(bytes_to_int_16(bytes, 43)), // power_consumption_today
    String(bytes_to_int_32(bytes, 59)), // power_generation_total
    String(decodeChargingState(bytes[68])), // charging_state
    String((bytes[67] >> 7) ? "true" : "false") // load_status (as boolean)
  };

  return decoded_values;
}

String buildJson(String decodedValues[]) {
  return String("{") +
    String("\"function\": \"") + decodedValues[0] + "\", " +
    String("\"battery_percentage\": ") + decodedValues[1] + ", " +
    String("\"battery_voltage\": ") + decodedValues[2] + ", " +
    String("\"battery_current\": ") + decodedValues[3] + ", " +
    String("\"battery_temperature\": ") + decodedValues[4] + ", " +
    String("\"controller_temperature\": ") + decodedValues[5] + ", " +
    String("\"load_status\": \"") + decodedValues[6] + "\", " +
    String("\"load_voltage\": ") + decodedValues[7] + ", " +
    String("\"load_current\": ") + decodedValues[8] + ", " +
    String("\"load_power\": ") + decodedValues[9] + ", " +
    String("\"pv_voltage\": ") + decodedValues[10] + ", " +
    String("\"pv_current\": ") + decodedValues[11] + ", " +
    String("\"pv_power\": ") + decodedValues[12] + ", " +
    String("\"max_charging_power_today\": ") + decodedValues[13] + ", " +
    String("\"max_discharging_power_today\": ") + decodedValues[14] + ", " +
    String("\"charging_amp_hours_today\": ") + decodedValues[15] + ", " +
    String("\"discharging_amp_hours_today\": ") + decodedValues[16] + ", " +
    String("\"power_generation_today\": ") + decodedValues[17] + ", " +
    String("\"power_consumption_today\": ") + decodedValues[18] + ", " +
    String("\"power_generation_total\": ") + decodedValues[19] + ", " +
    String("\"charging_status\": \"") + decodedValues[20] + "\", " +
    String("\"load_status_as_boolean\": ") + decodedValues[21] + "" +
    String("}"); 
}

/*
    data['battery_percentage'] = bytes_to_int(bs, 3, 2)
    data['battery_voltage'] = bytes_to_int(bs, 5, 2) * 0.1
    data['battery_current'] = bytes_to_int(bs, 7, 2) * 0.01
    data['battery_temperature'] = parse_temperature(bytes_to_int(bs, 10, 1))
    data['controller_temperature'] = parse_temperature(bytes_to_int(bs, 9, 1))
    data['load_status'] = LOAD_STATE[bytes_to_int(bs, 67, 1) >> 7]
    data['load_voltage'] = bytes_to_int(bs, 11, 2) * 0.1
    data['load_current'] = bytes_to_int(bs, 13, 2) * 0.01
    data['load_power'] = bytes_to_int(bs, 15, 2)
    data['pv_voltage'] = bytes_to_int(bs, 17, 2) * 0.1
    data['pv_current'] = bytes_to_int(bs, 19, 2) * 0.01
    data['pv_power'] = bytes_to_int(bs, 21, 2)
    data['max_charging_power_today'] = bytes_to_int(bs, 33, 2)
    data['max_discharging_power_today'] = bytes_to_int(bs, 35, 2)
    data['charging_amp_hours_today'] = bytes_to_int(bs, 37, 2)
    data['discharging_amp_hours_today'] = bytes_to_int(bs, 39, 2)
    data['power_generation_today'] = bytes_to_int(bs, 41, 2)
    data['power_consumption_today'] = bytes_to_int(bs, 43, 2)
    data['power_generation_total'] = bytes_to_int(bs, 59, 4)
    data['charging_status'] = CHARGING_STATE[bytes_to_int(bs, 68, 1)]
*/

//


void test() {
  // TEST
  const byte SAMPLE_RESPONSE[] = {
    0xff, 0x3, 0x44, 0x0, 0x64, 0x0, 0x92, 0x0, 0x7, 0xd, 0x19, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0xfd, 
    0x0, 0x1, 0x0, 0x1, 0x0, 0x0, 0x0, 0x7f, 0x0, 0x93, 0x0, 0xca, 0x0, 0x0, 0x0, 0x19, 0x0, 0x0, 0x0, 
    0x9, 0x0, 0x0, 0x0, 0x7d, 0x0, 0x0, 0x0, 0xa0, 0x0, 0xd, 0x1, 0x93, 0x0, 0x0, 0x3, 0xcc, 0x0,
     0x0, 0x0, 0x0, 0x0, 0x0, 0x33, 0x26, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x18, 0xc8
  };

  Serial.println("TEST {");
  String *values = decodeValues(SAMPLE_RESPONSE);
  // for (int i = 0; i < 21; i++) {
  //   Serial.println(values[i]);
  // }

  String asJson = buildJson(values);
  Serial.println(asJson);
  
  Serial.println("}");
}
