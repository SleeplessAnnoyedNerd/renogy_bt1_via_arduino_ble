/*
  Scan

  This example scans for Bluetooth® Low Energy peripherals and prints out their advertising details:
  address, local name, advertised service UUID's.

  The circuit:
  - Arduino MKR WiFi 1010, Arduino Uno WiFi Rev2 board, Arduino Nano 33 IoT,
    Arduino Nano 33 BLE, or Arduino Nano 33 BLE Sense board.

  This example code is in the public domain.
*/

// TODO
// - Refactor, of course. But first it needs to work.

// In the BLE world, the central/peripheral difference is very easy to define and recognize: 
//
// Central - the BLE device which initiates an outgoing connection request to an advertising peripheral device. 
// Peripheral - the BLE device which accepts an incoming connection request after advertising.

#include <ArduinoBLE.h>

const String ADDRESS = String("f0:f8:f2:6a:70:03");

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

// f000ffd1-0451-4000-b000-000000000000

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // BLE.debug(Serial);

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");

    while (1);
  }

  Serial.println("Bluetooth® Low Energy Central scan");

  // Be sure that iOS is not connected.
  // Kill iOS app if necessary.
  // Sometimes at helps to connect via iOS first and kill the iOS app.

  // start scanning for peripheral
  // BLE.scan();
  BLE.scanForAddress("f0:f8:f2:6a:70:03");
  // BLE.scanForName("BT-TH-F26A7003");
}

void loop() {
  if (properlyConnected) {
    // nothing to do anymore ... event handler takes over...

    thePeripheral.poll(500);

    BLE.poll(500);

    // Serial.println("looping...");

    if (notifyCharacteristic.valueUpdated()) {
      Serial.println("value updated");

      Serial.println(notifyCharacteristic.valueLength()); // current
      // Serial.println(notifyCharacteristic.valueSize()); // maximum

      byte value[255];
      int count = notifyCharacteristic.readValue(value, 255);

      for (int i = 0; i < count; i++) {
        Serial.print(value[i], HEX);
        Serial.print(" ");
      }  

      Serial.println();

      Serial.println(value[3] * 256 + value[4]); // battery_percentage
      Serial.println((value[5] * 256 + value[6]) * 0.1); // battery_voltage
      Serial.println((value[17] * 256 + value[18]) * 0.1); // pv_voltage
      Serial.println((value[19] * 256 + value[20]) * 0.01); // pv_current
      Serial.println(value[68]); // charging_status

/*
def parse_charge_controller_info(bs):
    data = {}
    data['function'] = FUNCTION[bytes_to_int(bs, 1, 1)]
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
    return 

    CHARGING_STATE = {
    0: 'deactivated',
    1: 'activated',
    2: 'mppt',
    3: 'equalizing',
    4: 'boost',
    5: 'floating',
    6: 'current limiting'
}

LOAD_STATE = {
  0: 'off',
  1: 'on'
}
*/


    }

    return;
  }

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

    if (peripheral.address() == ADDRESS) {
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