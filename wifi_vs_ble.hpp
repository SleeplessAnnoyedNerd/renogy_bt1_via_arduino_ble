#ifndef _WIFI_VS_BLE_H_
#define _WIFI_VS_BLE_H_

#include <ArduinoBLE.h>
#include <WiFi.h>

extern bool networkInitialized;
extern bool wifiModeFlag;

void bleStart(bool enableBleDebug);
bool switch2BleMode(const bool enableBleDebug, const char *bleDeviceName, const char *bleLocalName, void (*bleScan)(void));
void wifiMode(const char *ssid, const char *pass);
bool switch2WiFiMode();
void printWiFiStatus();

#endif
