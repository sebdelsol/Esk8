#pragma once

#include <Wire.h>
#include <Streaming.h>
#include <FastLED.h> // for lerp15by16

// #include <I2Cdev.h>
// #include <MPU6050_6Axis_MotionApps20.h>

#define ACCEL_AVG .9
#define ACCEL_BASE_FREQ 60.

class myMPU6050
{
  ulong mT = 0;
  int mX = 0, mY = 0, mZ = 0;

  void readAccel();
public:

  void begin();
  bool getXYZ(int &x, int &y, int &z, int &oneG);
};
