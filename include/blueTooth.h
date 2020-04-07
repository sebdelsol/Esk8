#pragma once

#include <Streaming.h>
#include <BluetoothSerial.h>
#include <BTcmd.h>
#include <myPins.h>

//#define BT_BAUD 9600 //38400 //9600
#define BT_TERMINAL_NAME "Esk8"
#define AUTO_STOP_IF_NOTCONNECTED 10000 // duration before bluetooth autostop after started if not connected

class BlueTooth
{
  long mStartTime;
  bool mON = false;
  BTcmd *mBTcmd;
  BluetoothSerial *mBTserial;
  Stream* mDbgSerial;

public:
  BluetoothSerial *getBtSerial() { return mBTserial; };
  bool update();
  void sendUpdate();
  bool registerObj(const OBJVar& obj, char* name) { return mBTcmd->registerObj(obj, name); };
  void save(bool isdefault) { mBTcmd->save(isdefault); };
  void load(bool isdefault, bool change = true) { mBTcmd->load(isdefault, change); };
  void sendLimsOverBT() { mBTcmd->sendLimsOverBT(); };

  BlueTooth();
  void init(Stream &serial);
  void start(const bool on=true);
  void toggle();
};
