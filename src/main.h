#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <TimeLib.h>

#include <config.h>

bool needRestart = false;
int wifiRetry = 0;
unsigned int mode = 255;
unsigned int state = 0;
unsigned int alarm = 0;
unsigned int blinkCount = 0;
unsigned int blinkDelay = 0;
unsigned long lastBlink = 0;
String chipid;

void handleRoot();
void blinkHandler();
void alarmHandler();
void connectToWifi();
void checkWifi();