#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <Arduino.h>
#include "config.h"

// Global variables (defined in functions.cpp)
extern unsigned long now;
extern unsigned long lastLectura;
extern unsigned long lastEnvio;
extern float lastTemp;
extern float lastHum;
extern bool lastSent;

const char* getRoomName(int id);
bool testRange(float t, float h);
bool readDht(float &t, float &h);
void readData();
void sendData();

// Temperature and humidity sensor functions
void initSensor();
float readTemperature();
float readHumidity();
void displaySensorData();

// WiFi functions
void initWiFi();
bool connectToWiFi();
void disconnectWiFi();

// Data transmission functions
void sendData(float temperature, float humidity);

#endif // FUNCTIONS_H
