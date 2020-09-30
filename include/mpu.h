#pragma once

#include <FastledCfg.h>
#include <FastLED.h> // for lerp15by16
#include <Wire.h>
#include <helper_3dmath.h>  // vector & quaternion 
#include <log.h>
#include <Pins.h>
#include <ObjVar.h>
#include <Variadic.h>

#define MPU6050_INCLUDE_DMP_MOTIONAPPS20 // so that all dmp functions are included
#include <MPU6050.h>

//----------------------------- dmp Version & Debug
// #define USE_V6_12
// #define USE_MEASURED_GRAVITY // use the measured gravity for real acc computation, instead a (more precise?) rotated gravity
#define MPU_DBG

//----------------------------- calibration & I2c clock
#define CALIBRATION_LOOP  6
#define I2C_CLOCK         400000 // 400kHz 

//----------------------------- Smooth accel & gyro
#define ACCEL_AVG         .05 // use 5% of the new measure in the avg
#define ACCEL_BASE_FREQ   60. // based on a 60fps measure

//----------------------------- Run in a task
#define MPU_GET_CORE  1 // mpu on core 1 to prevent ISR hanging
#define MPU_GET_PRIO  0 // enough & more stable with every other ISR
#define MPU_GET_STACK 2048

//----------------------------- 
struct SensorOutput 
{
  VectorInt16 axis;
  int         angle = 0;
  int16_t     acc = 0;
  int16_t     w = 0;
  bool        updated = false;
};

//-----------------------------
class MPU : public OBJVar, public MPU6050
{
  bool        mDmpReady = false; 
  bool        mHasBegun = false;

  uint8_t*    mFifoBuffer; 
  ulong       mT = 0;     // µs
  ulong       mdt = 1000; // µs

  Quaternion  mQuat;      // quat from dmp fifobuffer
  VectorInt16 mW;         // gyro 
  VectorInt16 mAcc;       // accel 
  VectorInt16 mAccReal;   // gravity-free accel
  
  #ifndef USE_MEASURED_GRAVITY
    void getLinearAccel(VectorInt16 *v, VectorInt16 *vRaw, Quaternion *q); // use rotated gravity
  #endif
  void getAxiSAngle(VectorInt16 &v, int &angle, Quaternion &q);

  int16_t mXGyroOffset,   mYGyroOffset,   mZGyroOffset;
  int16_t mXAccelOffset,  mYAccelOffset,  mZAccelOffset;
  bool    mGotOffset     = false;
  bool    mAutoCalibrate = false;

  void printOffsets(const __FlashStringHelper* txt);
  bool setOffsets();

  int16_t  mAccY  = 0;
  int16_t  mWZ    = 0;

  uint16_t mSmoothAcc  = 1600;
  uint16_t mNeutralAcc = 60;
  byte     mDivAcc     = 2;
  uint16_t mNeutralW   = 3000;
  int      mMaxW       = 7000; 

public:
  SensorOutput  mOutput; // public outpout

  // for mpu task
  void calibrate();
  bool getFiFoPacket();
  void compute(SensorOutput& output);

  void init();
  void begin();
  void update();
};
