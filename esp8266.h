#ifndef ESP8266_H
#define ESP8266_H

#include <Arduino.h>




// WiFi and connection functions
void connectWifi();

// Data transmission
void sendData();
void handleServer();

#endif // ESP8266_H