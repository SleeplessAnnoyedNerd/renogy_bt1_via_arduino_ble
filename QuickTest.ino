
#include <ArduinoBLE.h>
#include <WiFi.h>
#include <MQTT.h>  // https://github.com/256dpi/arduino-mqtt

#include "renogy_bt1.hpp"

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

int wifiStatus = WL_IDLE_STATUS;
bool networkInitialized = false;
bool wifiModeFlag = false;

// ---

WiFiClient net;
MQTTClient mqttClient(1024);

// ----

void bleStart() {
  // Be sure that iOS is not connected to Renogy BT-1.
  // Kill iOS app if necessary.
  // Sometimes at helps to connect via iOS first and kill the iOS app.

  if (ENABLE_BLE_DEBUG) {
      BLE.debug(Serial);
  }

  if (!BLE.begin()) {
    Serial.println("Starting BLE module failed!");

    while (1);
  }
}

void bleScan() {
  Serial.println("BLE Central scan");
  BLE.scanForAddress(RENOGY_BT1_MAC_ADDRESS);
  // BLE.scan();
}

void mqttMessageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}

void initMqtt() {
  Serial.println("MQTT");

  mqttClient.begin(MQTT_SERVER_IP, MQTT_SERVER_PORT, net);
  // mqttClient.onMessage(mqttMessageReceived);
  mqttClient.connect(MQTT_CLIENT_ID);

  if (!mqttClient.connected()) {
    Serial.println("MQTT not connected");
  } else {
    Serial.println("MQTT connected");
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial);
}

void loop() {
  // return; // for testing

  // Serial.println("looping");

  if (!networkInitialized) {
    if (!wifiModeFlag) {
      Serial.print("Switch to BLE: ");
      if (!switch2BleMode()) {
        Serial.println("failed");
      } else {
        networkInitialized = true;
        Serial.println("success");
      }
    } else {
      Serial.print("Switch to WiFi: ");
      if (!switch2WiFiMode()) {
        Serial.println("failed");
      } else {
        networkInitialized = true;
        Serial.println("success");

        networkInitialized = false;
        wifiModeFlag = false;

        delay(1000);
      }
    }
  } else {
    if (!wifiModeFlag) {
      // Serial.println("BLE mode...");
      // bleMode();
      BLE.poll(500);
    } else {
      // wifiMode();
    }
  }

  if (properlyConnected) {
    // nothing to do anymore ... event handler takes over...

    thePeripheral.poll(500);

    BLE.poll(500);

    if (notifyCharacteristic.valueUpdated()) {
      Serial.println("value updated");

      // Serial.println(notifyCharacteristic.valueLength()); // current
      // Serial.println(notifyCharacteristic.valueSize()); // maximum

      byte values[255];
      int count = notifyCharacteristic.readValue(values, 255);

      String *valuesAsString = decodeValues(values);
      String asJson = buildJson(valuesAsString);
      Serial.println(asJson);

      Serial.println("Need to switch from BLE to WiFi");

      properlyConnected = false; // since we disconnect from Bluetooth/BLE device
      switch2WiFiMode();
      wifiMode();

      Serial.println("MQTT");

      initMqtt();
      
      if (!mqttClient.connected()) {
        Serial.println("MQTT not connected, reconnecting");
        if (mqttClient.connect(MQTT_CLIENT_ID)) {
          Serial.println("MQTT reconnected");
        } else {
          Serial.println("MQTT reconnect failed");
        }
      } else {
        Serial.println("MQTT is connected");
      }

      if (mqttClient.publish(MQTT_TOPIC, asJson)) {
        Serial.println("MQTT publish was successful.");
      } else {
        Serial.println("MQTT publish failed!");

        Serial.print("MQTT lastError / returnCode: ");
        Serial.print(mqttClient.lastError());
        Serial.print(" ");
        Serial.println(mqttClient.returnCode());
      }

      mqttClient.loop();

      delay(1000);

      mqttClient.disconnect();

      switch2BleMode(); // Switch back to BLE.
    }

    if (rePollCtr++ > 15) {
      Serial.println("repolling...");

      rePollCtr = 0;
  
      const uint8_t request[] = { 255, 3, 1, 0, 0, 34, 209, 241 };
      if (!writeCharacteristic.writeValue(request, 8, true)) {
        Serial.println("BLE characteristic write failed.");
      }      
    }

    return;
  }

  // check if a peripheral has been discovered
  BLEDevice peripheral = BLE.available();

  if (peripheral) {
    // discovered a peripheral
    Serial.print("Discovered a peripheral: ");

    // print address
    Serial.print("Address: ");
    Serial.print(peripheral.address());

    // print the local name, if present
    if (peripheral.hasLocalName()) {
      Serial.print(" / Local Name: ");
      Serial.print(peripheral.localName());
    }

    // print the advertised service UUIDs, if present
    if (peripheral.hasAdvertisedServiceUuid()) {
      Serial.print(" / Service UUIDs: ");
      for (int i = 0; i < peripheral.advertisedServiceUuidCount(); i++) {
        Serial.print(peripheral.advertisedServiceUuid(i));
        Serial.print(" ");
      }
      Serial.println();
    }

    Serial.print(" / RSSI: ");
    Serial.print(peripheral.rssi());

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

          BLE.end();
          bleStart();
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
        Serial.println("Connected to BLE device.");
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

// ---

bool switch2BleMode() {
  Serial.println("switch2BleMode");

  WiFi.end();
  wiFiDrv.wifiDriverDeinit();

  // set advertised local name and service UUID
  BLE.setDeviceName(BLE_DEVICE_NAME);
  BLE.setLocalName(BLE_LOCAL_NAME);

  bleStart();
  bleScan();  

  return true;
}

void wifiMode() {
  int connectCount = 0;

  if (wifiStatus != WL_CONNECTED) {
    while (wifiStatus != WL_CONNECTED) {
      connectCount++;
      Serial.print("WiFi attempt: ");
      Serial.println(connectCount);

      if (connectCount > 10) {
        networkInitialized = false;
        wifiModeFlag = false;
        Serial.println("WiFi connection failed");
        return;
      }

      Serial.print("Attempting to connect to SSID: ");
      Serial.println(ssid);

      wifiStatus = WiFi.begin(ssid, pass);

      if (wifiStatus != WL_CONNECTED) {
        delay(10000); // wait 10 seconds for connection
      }
    }

    printWiFiStatus();
  }
}

bool switch2WiFiMode() {
  BLE.stopAdvertise();
  BLE.stopScan();
  BLE.disconnect();
  BLE.end();

  wifiStatus = WL_IDLE_STATUS;

  // Re-initialize the WiFi driver
  // This is currently necessary to switch from BLE to WiFi
  wiFiDrv.wifiDriverDeinit();
  wiFiDrv.wifiDriverInit();

  return true;
}

void printWiFiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
