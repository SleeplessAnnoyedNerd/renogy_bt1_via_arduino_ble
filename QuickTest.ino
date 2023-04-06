/*
  Scan

  This example scans for Bluetooth® Low Energy peripherals and prints out their advertising details:
  address, local name, advertised service UUID's.

  The circuit:
  - Arduino MKR WiFi 1010, Arduino Uno WiFi Rev2 board, Arduino Nano 33 IoT,
    Arduino Nano 33 BLE, or Arduino Nano 33 BLE Sense board.

  This example code is in the public domain.
*/

#include <ArduinoBLE.h>

const String ADDRESS = String("f0:f8:f2:6a:70:03");

// For efficiency, the Bluetooth® Low Energy (BLE) specification adds support for shortened 16-bit UUIDs.
// We work with BLE here... so we have to use 16-bit UUIDs (it seems).

// const String NOTIFY_CHAR_UUID = String("0000fff1-0000-1000-8000-00805f9b34fb");
// const String WRITE_CHAR_UUID = String("0000ffd1-0000-1000-8000-00805f9b34fb");

const String NOTIFY_CHAR_UUID = String("fff1");
const String WRITE_CHAR_UUID = String("ffd1");

// f000ffd1-0451-4000-b000-000000000000

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");

    while (1);
  }

  Serial.println("Bluetooth® Low Energy Central scan");

  // Be sure that iOS is not connected.
  // Kill iOS app if necessary.

  // start scanning for peripheral
  // BLE.scan();
  BLE.scanForAddress("f0:f8:f2:6a:70:03");
  // BLE.scanForName("BT-TH-F26A7003");
}

void loop() {
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

        // https://diysolarforum.com/threads/renogy-devices-and-raspberry-pi-bluetooth-wifi.30235/page-6
        // das ist gut, weil da sind die notwendigen bytes drin :)
        // ist aber BT-2 :(
        // DEBUG: create_poll_request BatteryParamInfo => [255, 3, 1, 0, 0, 7, 16, 42]
        // DEBUG: [regulator] Writing data to 0000ffd1-0000-1000-8000-00805f9b34fb - [255, 3, 1, 0, 0, 7, 16, 42] (ff0301000007102a)
        // ----
        // https://forum.fhem.de/index.php?topic=121750.15
        // gatt tool...
        
        BLECharacteristic writeCharacteristic = peripheral.characteristic(WRITE_CHAR_UUID.c_str());
        if (writeCharacteristic) {
          Serial.println("use the writeCharacteristic");
        } else {
          Serial.println("Peripheral does NOT have required service.");
        }

        BLECharacteristic notifyCharacteristic = peripheral.characteristic(NOTIFY_CHAR_UUID.c_str());
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
          }
        } else {
          Serial.println("Peripheral does NOT have required service.");
        }
      } else {
        Serial.println("Connection failed.");
      }
    }
  }
}
