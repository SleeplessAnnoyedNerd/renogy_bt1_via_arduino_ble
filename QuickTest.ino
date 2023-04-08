
//
// In the BLE world, the central/peripheral difference is very easy to define and recognize: 
//
// Central - the BLE device which initiates an outgoing connection request to an advertising peripheral device. 
// Peripheral - the BLE device which accepts an incoming connection request after advertising.
//

#include <ArduinoBLE.h>
#include <WiFi.h>
#include <MQTT.h>  // https://github.com/256dpi/arduino-mqtt

#include "secrets.h"

// For efficiency, the Bluetooth® Low Energy (BLE) specification adds support for shortened 16-bit UUIDs.
// We work with BLE here... so we have to use 16-bit UUIDs (it seems).

// const String NOTIFY_CHAR_UUID = String("0000fff1-0000-1000-8000-00805f9b34fb");
// const String WRITE_CHAR_UUID = String("0000ffd1-0000-1000-8000-00805f9b34fb");

const String NOTIFY_CHAR_UUID = String("fff1");
const String WRITE_CHAR_UUID = String("ffd1");

BLEDevice thePeripheral;
BLECharacteristic notifyCharacteristic;
BLECharacteristic writeCharacteristic;
bool properlyConnected = false;
int rePollCtr = 0;

// ---

WiFiClient net;
MQTTClient mqttClient;

// f000ffd1-0451-4000-b000-000000000000

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

  return CHARGING_STATE[b]; // TODO change range!
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
    String(decodeChargingState(bytes[68])) // charging_state
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
    String("\"charging_status\": \"") + decodedValues[20] + "\"" +
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

// ----

void connect() {
  Serial.println("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Wifi connected.");
}

void bleScan() {
  // Serial.println("Bluetooth® Low Energy Central scan");
  // start scanning for peripheral
  // BLE.scan();
  // BLE.scanForAddress(RENOGY_BT1_MAC_ADDRESS);
  // BLE.scanForName("BT-TH-F26A7003");

  Serial.println("\nBluetooth® Low Energy Central scan");
  BLE.scanForAddress(RENOGY_BT1_MAC_ADDRESS);
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

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

  Serial.println(buildJson(values));
  
  Serial.println("}");

  Serial.print(ssid);
  Serial.print(" / ");
  Serial.println(pass);
  WiFi.begin(ssid, pass);

  connect();

  mqttClient.begin(MQTT_SERVER_IP, MQTT_SERVER_PORT, net);

  // ---

  // order regading WIFI and BLE is important ... 

  // Be sure that iOS is not connected.
  // Kill iOS app if necessary.
  // Sometimes at helps to connect via iOS first and kill the iOS app.

  // BLE.debug(Serial);

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");

    while (1);
  }

  bleScan();
}

void loop() {
  // return; // for testing

  // Serial.println("looping");

  if (properlyConnected) {
    // nothing to do anymore ... event handler takes over...

    thePeripheral.poll(500);

    BLE.poll(500);

    // Serial.println("looping...");

    if (notifyCharacteristic.valueUpdated()) {
      Serial.println("value updated");

      // Serial.println(notifyCharacteristic.valueLength()); // current
      // Serial.println(notifyCharacteristic.valueSize()); // maximum

      byte values[255];
      int count = notifyCharacteristic.readValue(values, 255);

      String *valuesAsString = decodeValues(values);
      String asJson = buildJson(valuesAsString);
      Serial.println(asJson);

      if (mqttClient.publish("/renogy/b1", asJson)) {
        Serial.println("MQTT publish was successful.");
      }
    }

    if (rePollCtr++ > 15) {
      Serial.println("repolling...");

      rePollCtr = 0;
  
      const uint8_t request[] = { 255, 3, 1, 0, 0, 34, 209, 241 };
      if (!writeCharacteristic.writeValue(request, 8, true)) {
        Serial.println("write failed.");
      }      
    }

    return;
  }

  // Serial.println("Bluetooth® Low Energy Central scan");
  // BLE.scanForAddress(RENOGY_BT1_MAC_ADDRESS);

  // check if a peripheral has been discovered
  BLEDevice peripheral = BLE.available();

  if (peripheral) {
    // discovered a peripheral
    Serial.println("Discovered a peripheral");
    Serial.println("-----------------------");

    // print address
    Serial.print("Address: ");
    Serial.println(peripheral.address());

    // print the local name, if present
    if (peripheral.hasLocalName()) {
      Serial.print("Local Name: ");
      Serial.println(peripheral.localName());
    }

    // print the advertised service UUIDs, if present
    if (peripheral.hasAdvertisedServiceUuid()) {
      Serial.print("Service UUIDs: ");
      for (int i = 0; i < peripheral.advertisedServiceUuidCount(); i++) {
        Serial.print(peripheral.advertisedServiceUuid(i));
        Serial.print(" ");
      }
      Serial.println();
    }

    // print the RSSI
    Serial.print("RSSI: ");
    Serial.println(peripheral.rssi());

    Serial.println();

    if (peripheral.address() == RENOGY_BT1_MAC_ADDRESS) {
      Serial.println("BT-1 found.");
      BLE.stopScan(); // important, otherwise can't connect?

      if (peripheral.connect()) {
        Serial.println("Successfully connected.");

        // discover peripheral attributes
        Serial.println("Discovering attributes ...");
        if (peripheral.discoverAttributes()) {
          Serial.println("Attributes discovered");
        } else {
          Serial.println("Attribute discovery failed!");
          peripheral.disconnect();
          bleScan(); // rescan ...
          return;
        }

        // https://devzone.nordicsemi.com/guides/short-range-guides/b/bluetooth-low-energy/posts/ble-services-a-beginners-tutorial

        int serviceCount = peripheral.serviceCount();

        Serial.print(serviceCount);
        Serial.println(" services discovered");

        for (int i = 0; i < serviceCount; i++) {
          BLEService service = peripheral.service(i);

          if (service) {
            Serial.print(i);
            Serial.print(" ");
            Serial.print(service.uuid());
            Serial.print(" ");
            Serial.print(service.characteristicCount());
            Serial.println();

            // 1800 -- generic access
            // 180a -- device information
            // ffd0 -- 
            // fff0 --
            // f000ffd0-0451-4000-b000-000000000000 -- 
          }
        }

        int characteristicCount = peripheral.characteristicCount();

        Serial.print(characteristicCount);
        Serial.println(" characteristis discovered");

        for (int i = 0; i < characteristicCount; i++) {
          BLECharacteristic characteristic = peripheral.characteristic(i);

          if (characteristic) {
            Serial.print(i);
            Serial.print(" ");
            Serial.print(characteristic.canSubscribe());
            Serial.print(" ");
            Serial.println(characteristic.uuid());

            // https://development.libelium.com/ble-networking-guide/default-profile-on-ble-module
            if (
                  (characteristic.uuid() == String("2a00")) ||
                  (characteristic.uuid() == String("2a01")) ||
                  (characteristic.uuid() == String("2a04")) ||
                  (characteristic.uuid() == String("2a23")) ||
                  (characteristic.uuid() == String("2a24")) ||
                  (characteristic.uuid() == String("2a25")) ||
                  (characteristic.uuid() == String("2a26")) ||
                  (characteristic.uuid() == String("2a27")) ||
                  (characteristic.uuid() == String("2a28")) ||
                  (characteristic.uuid() == String("2a29")) ||
                  (characteristic.uuid() == String("2a2a")) ||
                  (characteristic.uuid() == String("2a50")) ||
                  (characteristic.uuid() == String("ffd1")) ||
                  (characteristic.uuid() == String("ffd2")) ||
                  (characteristic.uuid() == String("ffd3")) ||
                  (characteristic.uuid() == String("ffd4")) ||
                  (characteristic.uuid() == String("ffd5")) ||
                  (characteristic.uuid() == String("fff1"))
               ) { // Device Name
              char value[255];
              characteristic.readValue(value, 255);
              Serial.println(value);

              // starting with 2a2a garbage is delivered. maybe no strings anymore.
            }

          }        
        }

        notifyCharacteristic = peripheral.characteristic(NOTIFY_CHAR_UUID.c_str());
        if (notifyCharacteristic) {
          Serial.println("use the notifyCharacteristic");
          
          if (!notifyCharacteristic.canSubscribe()) {
            Serial.println("notifyCharacteristic characteristic is not subscribable!");
            peripheral.disconnect();
            return;
          } else if (!notifyCharacteristic.subscribe()) {
            Serial.println("notifyCharacteristic subscription failed!");
            peripheral.disconnect();
            return;
          } else {
            notifyCharacteristic.setEventHandler(BLESubscribed | BLEUnsubscribed | BLERead | BLEWritten | BLENotify, switchCharacteristicWritten);
          }
        } else {
          Serial.println("Peripheral does NOT have required service.");
        }

        // https://diysolarforum.com/threads/renogy-devices-and-raspberry-pi-bluetooth-wifi.30235/page-6
        // das ist gut, weil da sind die notwendigen bytes drin :)
        // ist aber BT-2 :(
        // DEBUG: create_poll_request BatteryParamInfo => [255, 3, 1, 0, 0, 7, 16, 42]
        // DEBUG: [regulator] Writing data to 0000ffd1-0000-1000-8000-00805f9b34fb - [255, 3, 1, 0, 0, 7, 16, 42] (ff0301000007102a)
        // ----
        // https://forum.fhem.de/index.php?topic=121750.15
        // gatt tool...
        
        writeCharacteristic = peripheral.characteristic(WRITE_CHAR_UUID.c_str());
        if (writeCharacteristic) {
          Serial.println("use the writeCharacteristic");

          const uint8_t request[] = { 255, 3, 1, 0, 0, 34, 209, 241 };
          // ff 03 01 00 00 22 d1 f1
          // https://www.lammertbies.nl/comm/info/crc-calculation => CRC-16 (Modbus)	0xF1D1  

          // (DEVICE_ID, READ_PARAMS["FUNCTION"], READ_PARAMS["REGISTER"], READ_PARAMS["WORDS"])
          // WORDS ... 34

          // set_load:
          // request = create_request_payload(DEVICE_ID, WRITE_PARAMS_LOAD["FUNCTION"], WRITE_PARAMS_LOAD["REGISTER"], value)
          // device_id, 255
          // write_params_load, function, 3
          // register, 256 (16 bit)
          // WORDS, 34 (16 bit)
          // crc, 16 bit
          if (!writeCharacteristic.writeValue(request, 8, true)) {
            Serial.println("write failed.");
          }

          /*
          def create_request_payload(device_id, function, regAddr, readWrd):                             
            data = None                                

            if regAddr:
                data = []
                data.append(device_id)
                data.append(function)
                data.append(int_to_bytes(regAddr, 0))
                data.append(int_to_bytes(regAddr, 1))
                data.append(int_to_bytes(readWrd, 0))
                data.append(int_to_bytes(readWrd, 1))

                crc = libscrc.modbus(bytes(data))
                data.append(int_to_bytes(crc, 1))
                data.append(int_to_bytes(crc, 0))
                logging.debug("{} {} => {}".format("create_read_request", regAddr, data))
            return data
          */
        } else {
          Serial.println("Peripheral does NOT have required service.");
          return;
        }

        thePeripheral = peripheral;

        properlyConnected = true;
        Serial.println("properlyConnected");
      } else {
        Serial.println("Connection failed.");
      }
    }
  }
}

void switchCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  // central wrote new value to characteristic, update LED
  Serial.print("Characteristic event, read: ");

  if (characteristic.value()) {
    Serial.println("LED on");
  } else {
    Serial.println("LED off");
  }
}
