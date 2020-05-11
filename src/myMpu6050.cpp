#include <myMpu6050.h>

#define __PGMSPACE_H_ 1 // pgmsplace.h define PGMSPACE_INCLUDE instead of __PGMSPACE_H
#ifdef USE_V6.12
  #include <MPU6050_6Axis_MotionApps_V6_12.h> // longer to init & bug with sensitivity
#else
  #include <MPU6050_6Axis_MotionApps20.h>
#endif

//--------------------------------------
#ifdef MPU_GETFIFO_CORE
  SemaphoreHandle_t   mpuOutputMutex;
  EventGroupHandle_t  mpuFlagReady;
  TaskHandle_t        mpuNotifyToCalibrate;
  SensorOutput        sharedOutput; // shared with updateMotion so that the mutex is barely taken by MPUGetTask

  void MPUGetTask(void* _myMpu)
  {
    myMPU6050*    myMpu = (myMPU6050* )_myMpu;
    SensorOutput  computeOutput; //output to store computation

    for (;;) // forever
    {
      if(ulTaskNotifyTake(pdTRUE, 0)) // pool the the task notification semaphore
        myMpu->calibrate();

      if(myMpu->dmpGetCurrentFIFOPacket(myMpu->mFifoBuffer))
      {
        myMpu->computeMotion(computeOutput);

        xSemaphoreTake(mpuOutputMutex, portMAX_DELAY);
        memcpy(&sharedOutput, &computeOutput, sizeof(SensorOutput)); 
        xSemaphoreGive(mpuOutputMutex);

        xEventGroupSetBits(mpuFlagReady, 1);
      }
    }
  }
#endif

//--------------------------------------
void myMPU6050::init()
{
  #define REGISTER_MPU(var) REGISTER_VAR_SIMPLE_NOSHOW(myMPU6050, #var, self->var, -32768, 32767)

  REGISTER_MPU(mXGyroOffset);   REGISTER_MPU(mYGyroOffset);  REGISTER_MPU(mZGyroOffset);
  REGISTER_MPU(mXAccelOffset);  REGISTER_MPU(mYAccelOffset); REGISTER_MPU(mZAccelOffset);

  #ifdef MPU_GETFIFO_CORE
    REGISTER_CMD(myMPU6050, "calibrate",  {xTaskNotifyGive(mpuNotifyToCalibrate);} ) // trigger a calibration
  #else
    REGISTER_CMD(myMPU6050, "calibrate",  {self->calibrate();} ) 
  #endif
}

void myMPU6050::calibrate()
{
  CalibrateAccel(CALIBRATION_LOOP);
  CalibrateGyro(CALIBRATION_LOOP);

  mXGyroOffset = getXGyroOffset();   mYGyroOffset = getYGyroOffset();   mZGyroOffset = getZGyroOffset();
  mXAccelOffset = getXAccelOffset(); mYAccelOffset = getYAccelOffset(); mZAccelOffset = getZAccelOffset();

  PrintActiveOffsets();
}

void myMPU6050::loadCalibration()
{
  setXGyroOffset(mXGyroOffset);   setYGyroOffset(mYGyroOffset);   setZGyroOffset(mZGyroOffset);
  setXAccelOffset(mXAccelOffset); setYAccelOffset(mYAccelOffset); setZAccelOffset(mZAccelOffset); 
  
  PrintActiveOffsets();
}

//--------------------------------------
void myMPU6050::begin(Stream &serial, bool doCalibrate)
{ 
  mSerial = &serial;
  *mSerial << "---------" << endl;
  
  Wire.begin(SDA, SCL);
  Wire.setClock(400000); // 400kHz I2C clock.

  initialize();
  *mSerial << "MPU connection " << (testConnection() ? "successful" : "failed") << endl;
  uint8_t devStatus = dmpInitialize();

  if (devStatus == 0) // did it work ?
  { 
    doCalibrate ? calibrate() : loadCalibration();

    setDMPEnabled(true);
    mDmpReady = true;

    uint16_t packetSize = dmpGetFIFOPacketSize();
    mFifoBuffer = (uint8_t* )malloc(packetSize * sizeof(uint8_t)); // FIFO storage buffer
    assert (mFifoBuffer!=NULL);

    #ifdef MPU_GETFIFO_CORE
      mpuOutputMutex = xSemaphoreCreateMutex();
      mpuFlagReady = xEventGroupCreate();
      xTaskCreatePinnedToCore(MPUGetTask, "mpuTask", 2048, this, MPU_GETFIFO_PRIO, &mpuNotifyToCalibrate, MPU_GETFIFO_CORE);  
      *mSerial << "Mpu runs on task on Core " << MPU_GETFIFO_CORE << " with Prio " << MPU_GETFIFO_PRIO << endl;
    #else 
      *mSerial << "Mpu runs on Main Core" << endl;
    #endif

    *mSerial << "DMP enabled" << endl;
  }
  else // ERROR! 1 = initial memory load failed, 2 = DMP configuration updates failed
    *mSerial << "DMP Initialization failed (" << devStatus << ")" << endl;
  
  *mSerial << "---------" << endl;
}

//--------------------------------------
void myMPU6050::getAxiSAngle(VectorInt16& v, int& angle, Quaternion& q)
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
#define STAYS_SHORT(x) constrain(x, -32768, 32767)
#define SHIFTR_VECTOR(v, n) v.x = v.x >> n;   v.y = v.y >> n;   v.z = v.z >> n; 

void myMPU6050::computeMotion(SensorOutput& output)
{
  ulong t = micros();
  ulong dt = t - mT;
  mT = t;

  dmpGetQuaternion(&mQuat, mFifoBuffer);
  dmpGetGyro(&mW, mFifoBuffer);
  #ifdef USE_V6.12
    SHIFTR_VECTOR(mW, 2) // fix sensibility bug in <MPU6050_6Axis_MotionApps_V6_12.h
  #endif 

  // axis angle
  dmpGetGravity(&mGrav, &mQuat);
  getAxiSAngle(output.axis, output.angle, mQuat);

  // real acceleration, adjusted to remove gravity
  dmpGetAccel(&mAcc, mFifoBuffer);
  #ifdef USE_V6.12 
    SHIFTR_VECTOR(mAcc, 1) // fix sensibility bug in <MPU6050_6Axis_MotionApps_V6_12.h
  #endif
  dmpGetLinearAccel(&mAccReal, &mAcc, &mGrav);

  // smooth acc & gyro
  uint16_t smooth = - int(pow(1. - ACCEL_AVG, dt * ACCEL_BASE_FREQ * .000001) * 65536.); // 1 - (1-accel_avg) ^ (dt * 60 / 1000 000) using fract16
  output.accX =  lerp15by16(output.accX,  STAYS_SHORT(mAccReal.x),  smooth);
  output.accY =  lerp15by16(output.accY,  STAYS_SHORT(mAccReal.y),  smooth);
  output.accZ =  lerp15by16(output.accZ,  STAYS_SHORT(mAccReal.z),  smooth);
  output.wZ =    lerp15by16(output.wZ,    STAYS_SHORT(mW.z * -655), smooth);

  #ifdef MPU_DBG
    *mSerial << "[ dt "   << dt*.001 << "ms\t smooth" << smooth/65536. << "\t Wz "  << output.wZ  << "]\t ";
    *mSerial << "[ gyr "  << mW.x << "\t "            << mW.y << "\t "              << mW.z << "\t ";
    *mSerial << "[ grav " << mGrav.x << "\t "         << mGrav.y << "\t "           << mGrav.z << "]\t ";
    *mSerial << "[ avg "  << output.accX << "\t "     << output.accY << "\t "       << output.accZ << "]\t ";
    *mSerial << "[ acc "  << mAcc.x << "\t "          << mAcc.y << "\t "            << mAcc.z << "]\t ";
    *mSerial << "[ real " << mAccReal.x << "\t "      << mAccReal.y << "\t "        << mAccReal.z << "]\t ";
    *mSerial << endl;
  #endif
}

//--------------------------------------
void myMPU6050::updateMotion()
{
#ifdef MPU_GETFIFO_CORE
  
  if (mDmpReady && xEventGroupGetBits(mpuFlagReady) && xSemaphoreTake(mpuOutputMutex, 0) == pdTRUE) // pool the mpuTask
  {
    memcpy(&mOutput, &sharedOutput, sizeof(SensorOutput)); 
    xSemaphoreGive(mpuOutputMutex); // release the mutex after measures have been copied
    updated = true;
  }
  else
    updated = false;

#else
  if (mDmpReady && dmpGetCurrentFIFOPacket(mFifoBuffer))
    computeMotion(mOutput);

  updated = mDmpReady;
#endif
}
