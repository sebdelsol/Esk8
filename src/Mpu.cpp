#include <mpu.h>

#define __PGMSPACE_H_ 1 // pgmsplace.h define PGMSPACE_INCLUDE instead of __PGMSPACE_H
#ifdef USE_V6_12
  #include <MPU6050_6Axis_MotionApps_V6_12.h> // longer to init & bug with sensitivity
#else
  #include <MPU6050_6Axis_MotionApps20.h>
#endif

//--------------------------------------
#ifdef MPU_GET_CORE
  SemaphoreHandle_t   OutputMutex = xSemaphoreCreateMutex();
  EventGroupHandle_t  FlagReady = xEventGroupCreate();
  TaskHandle_t        NotifyToCalibrate;
  SensorOutput        SharedOutput; // shared with update so that the mutex is barely taken by MPUComputeTask

  void MPUComputeTask(void* _mpu)
  {
    MPU*          mpu = (MPU*) _mpu;
    SensorOutput  taskOutput; //output to store computation
    TickType_t    lastWakeTime = xTaskGetTickCount();

    for (;;) // forever
    {
      if(mpu->getFiFoPacket())
      {
        mpu->compute(taskOutput);

        xSemaphoreTake(OutputMutex, portMAX_DELAY);
        memcpy(&SharedOutput, &taskOutput, sizeof(SensorOutput)); 
        xSemaphoreGive(OutputMutex);

        xEventGroupSetBits(FlagReady, 1);

        if(ulTaskNotifyTake(pdTRUE, 0)) // pool the the task semaphore
          mpu->calibrate();
      }

      // should got a packet every 10ms
      vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10)); 
    }
  }
#endif

//--------------------------------------
void MPU::init()
{
  // save calibration
  #define AddOffset(var)     AddVarHid(var, -32768, 32767)
  AddOffset(mXGyroOffset);   AddOffset(mYGyroOffset);  AddOffset(mZGyroOffset);
  AddOffset(mXAccelOffset);  AddOffset(mYAccelOffset); AddOffset(mZAccelOffset);
  
  AddBoolNameHid("gotOffset", mGotOffset);
  
  #ifdef MPU_GET_CORE
    xTaskCreatePinnedToCore(MPUComputeTask, "mpuTask", MPU_GET_STACK, this, MPU_GET_PRIO, &NotifyToCalibrate, MPU_GET_CORE);  
    AddCmd("calibrate",  xTaskNotifyGive(NotifyToCalibrate) ) // trigger a calibration
    _log << _FMT("Mpu runs on Core % with Prio %", MPU_GET_CORE, MPU_GET_PRIO) << endl;

  #else
    AddCmd("calibrate",  calibrate() ) 
  #endif

  AddBoolName("auto", mAutoCalibrate);
}

//--------------------------------------
void MPU::calibrate()
{
  CalibrateAccel(CALIBRATION_LOOP);
  CalibrateGyro(CALIBRATION_LOOP);

  mXGyroOffset = getXGyroOffset();   mYGyroOffset = getYGyroOffset();   mZGyroOffset = getZGyroOffset();
  mXAccelOffset = getXAccelOffset(); mYAccelOffset = getYAccelOffset(); mZAccelOffset = getZAccelOffset();
  printOffsets(F("MPU calibrated"));
  mGotOffset = true;
}

bool MPU::setOffsets()
{
  _log << F("Try to get Offset...");

  if (mGotOffset)
  {
    setXGyroOffset(mXGyroOffset);   setYGyroOffset(mYGyroOffset);   setZGyroOffset(mZGyroOffset);
    setXAccelOffset(mXAccelOffset); setYAccelOffset(mYAccelOffset); setZAccelOffset(mZAccelOffset); 
    printOffsets(F("Got internal offsets"));
  }
  else 
    _log << endl;

  return mGotOffset;
}

void MPU::printOffsets(const __FlashStringHelper* txt)
{
  _log << txt << endl;
  _log << F("Acc Offset: ") << SpaceIt(_WIDTH(getXAccelOffset(), 6), _WIDTH(getYAccelOffset(), 6), _WIDTH(getZAccelOffset(), 6)) << endl;
  _log << F("Gyr Offset: ") << SpaceIt(_WIDTH(getXGyroOffset(), 6),  _WIDTH(getYGyroOffset(), 6),  _WIDTH(getZGyroOffset(), 6))  << endl;
}

//--------------------------------------
void MPU::begin()
{ 
  mHasBegun = true;
  Wire.begin(SDA, SCL, I2C_CLOCK);

  initialize(); reset(); resetI2CMaster(); //help with startup reliabilily

  _log << F("MPU connection...") << (testConnection() ? F("successful") : F("failed")) << endl;
  uint8_t devStatus = dmpInitialize();

  if (devStatus == 0) // did it work ?
  { 
    if(!mAutoCalibrate || !setOffsets())
      calibrate();

    mFifoBuffer = (uint8_t* )malloc(dmpGetFIFOPacketSize() * sizeof(uint8_t)); // FIFO storage buffer
    assert (mFifoBuffer!=nullptr);

    setDMPEnabled(true);
    mDmpReady = true;
  }
  else // error
  {
    const __FlashStringHelper* error =  devStatus == 1 ? F("initial memory load") : (devStatus == 2 ? F("DMP configuration updates") : F("unknown"));
    _log << _FMT(F("DMP ERROR #% : % failure"), devStatus, error) << endl;
  }
}

//--------------------------------------
void MPU::getAxiSAngle(VectorInt16& v, int& angle, Quaternion& q)
{
  if (q.w > 1) q.normalize(); // needs q.w < 1 for acos and sqrt
  angle = acos(q.w) * 2 * 10430.; // 32767 / PI 
  
  float s = sqrt(1 - q.w * q.w);
  if (s < 0.001) // div 0
  {
    v.x = 1; v.y = v.z = 0;
  }
  else
  {
    float n = 32767. / s; 
    v.x = q.x * n; v.y = q.y * n; v.z = q.z * n;
  }
}

//--------------------------------------
bool MPU::getFiFoPacket() 
{ 
  ulong dt = micros() - mT; // best place to get the actual dt if called right after the delay function

  if (mDmpReady && dmpGetCurrentFIFOPacket(mFifoBuffer))
  {
    mdt = dt;
    mT += dt;
    return true; 
  }
  return false;
}

//--------------------------------------
#define STAYS_SHORT(x) constrain(x, -32768, 32767)
#define SHIFTR_VECTOR(v, n) v.x = v.x >> n;   v.y = v.y >> n;   v.z = v.z >> n; 

void MPU::compute(SensorOutput& output)
{
  dmpGetQuaternion(&mQuat, mFifoBuffer);
  dmpGetGyro(&mW, mFifoBuffer);
  #ifdef USE_V6_12
    SHIFTR_VECTOR(mW, 2) // fix sensibility bug in MPU6050_6Axis_MotionApps_V6_12.h
  #endif 

  // axis angle
  dmpGetGravity(&mGrav, &mQuat);
  getAxiSAngle(output.axis, output.angle, mQuat);

  // real acceleration, adjusted to remove gravity
  dmpGetAccel(&mAcc, mFifoBuffer);
  #ifdef USE_V6_12 
    SHIFTR_VECTOR(mAcc, 1) // fix sensibility bug in MPU6050_6Axis_MotionApps_V6_12.h
  #endif
  dmpGetLinearAccel(&mAccReal, &mAcc, &mGrav);

  // smooth acc & gyro
  uint16_t smooth = - int(pow(1. - ACCEL_AVG, mdt * ACCEL_BASE_FREQ * .000001) * 65536.); // 1 - (1-accel_avg) ^ (dt * 60 / 1000 000) using fract16
  output.accX =  lerp15by16(output.accX,  STAYS_SHORT(mAccReal.x),  smooth);
  output.accY =  lerp15by16(output.accY,  STAYS_SHORT(mAccReal.y),  smooth);
  output.accZ =  lerp15by16(output.accZ,  STAYS_SHORT(mAccReal.z),  smooth);
  output.wZ =    lerp15by16(output.wZ,    STAYS_SHORT(mW.z * -655), smooth);
  output.updated = true;

  #ifdef MPU_DBG
    _log << "[ dt "   << _FLOATW(mdt * .001, 2, 6) << "ms - smooth " << _FLOATW(smooth / 65536., 2,  6) << "  - Wz "  << _WIDTH(output.wZ, 6) << "] ";
    _log << "[ gyr "  << SpaceIt( _WIDTH(mW.x, 4),        _WIDTH(mW.y, 4),        _WIDTH(mW.z, 4))        << "] ";
    _log << "[ grav " << SpaceIt( _FLOATW(mGrav.x, 2, 6), _FLOATW(mGrav.y, 2, 6), _FLOATW(mGrav.z, 2, 6))     << "] ";
    _log << "[ avg "  << SpaceIt( _WIDTH(output.accX, 6), _WIDTH(output.accY, 6), _WIDTH(output.accZ, 6)) << "] ";
    _log << "[ acc "  << SpaceIt( _WIDTH(mAcc.x, 6),      _WIDTH(mAcc.y, 6),      _WIDTH(mAcc.z, 6))      << "] ";
    _log << "[ real " << SpaceIt( _WIDTH(mAccReal.x, 6),  _WIDTH(mAccReal.y, 6),  _WIDTH(mAccReal.z, 6))  << "] ";
    _log << "\r";
  #endif
}

//--------------------------------------
void MPU::update()
{
  if (!mHasBegun)
    begin();

  #ifdef MPU_GET_CORE
    if (xEventGroupGetBits(FlagReady) && xSemaphoreTake(OutputMutex, 0) == pdTRUE) // pool the mpuTask
    {
      memcpy(&mOutput, &SharedOutput, sizeof(SensorOutput)); 
      xSemaphoreGive(OutputMutex); // release the mutex after measures have been copied
    }
  #else
    if (getFiFoPacket())
      compute(mOutput);
  #endif
}
