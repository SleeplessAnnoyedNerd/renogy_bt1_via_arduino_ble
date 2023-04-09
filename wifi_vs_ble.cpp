#include <hardwareSerial.h>

#include "wifi_vs_ble.hpp"

// ---

int wifiStatus = WL_IDLE_STATUS;
bool networkInitialized = false;
bool wifiModeFlag = false;

// ---

void bleStart(bool enableBleDebug) {
  // Be sure that iOS is not connected to Renogy BT-1.
  // Kill iOS app if necessary.
  // Sometimes at helps to connect via iOS first and kill the iOS app.

  if (enableBleDebug) {
      BLE.debug(Serial);
  }

  if (!BLE.begin()) {
    Serial.println("Starting BLE module failed!");

    while (1);
  }
}

bool switch2BleMode(bool enableBleDebug, const char *bleDeviceName, const char *bleLocalName, void (*bleScan)(void)) {
  Serial.println("switch2BleMode");

  WiFi.end();
  wiFiDrv.wifiDriverDeinit();

  // set advertised local name and service UUID
  BLE.setDeviceName(bleDeviceName);
  BLE.setLocalName(bleLocalName);

  bleStart(enableBleDebug);
  bleScan();  

  return true;
}

void wifiMode(const char *ssid, const char *pass) {
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
