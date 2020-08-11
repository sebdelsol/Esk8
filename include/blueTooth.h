#pragma once

#include <Streaming.h>
#include <BluetoothSerial.h>
#include <Pins.h>

#define BT_TERMINAL_NAME "Esk8"
#define AUTO_STOP_IF_NOTCONNECTED 30000 // duration before bluetooth autostop if not connected

class BlueTooth
{
  long mStartTime;
  bool mON = false;
  bool mConnected = false;

  Stream& mDbgSerial;
  BluetoothSerial mBTSerial;

public:
  BlueTooth(Stream& serial) : mDbgSerial(serial) {};
  void init(const bool on=true);
  void start(const bool on=true);
  void toggle();
  void onEvent(esp_spp_cb_event_t event, esp_spp_cb_param_t* param); 
  BluetoothSerial& getSerial() { return mBTSerial; };
  bool isReadyToReceive();
  bool isReadyToSend();
};
