#pragma once

#include <WiFi.h>
#include <FastledCfg.h>
#include <FastLED.h>
#include <log.h>

#include <wificonfig.h>  
// #define WIFI_ssid "******"
// #define WIFI_password "****"

const long WIFI_TIMEOUT = 10 * 1000; // sec before stopping non connected wifi

class myWifi
{
  int  mBegunOn;
  bool mON = false;
  bool mWantON = false;

public:
  void start();
  void stop(const char* reason = nullptr);
  bool update();
};

