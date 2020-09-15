#include <ledstrip.h>

// ----------------------------------------------------
#ifdef FASTLED_CORE
  TaskHandle_t FastLEDshowTaskHandle;
  TaskHandle_t userTaskHandle = 0;

  void TriggerFastLEDShow()
  {
    if (userTaskHandle == 0)
    {
      userTaskHandle = xTaskGetCurrentTaskHandle(); //so that the show task can notify when it's done
      xTaskNotifyGive(FastLEDshowTaskHandle); // trigger fastled show task
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FASTLED_WAIT)); // Wait to be notified that it's done
      userTaskHandle = 0;
    }
  }

  void FastLEDshowTask(void* pvParameters)
  {
    for (;;) // forever
    {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for the trigger
      FastLED.show(); 
      xTaskNotifyGive(userTaskHandle); // Notify the calling task
    }
  }
#endif

// ----------------------------------------------------
AllLedStrips::AllLedStrips() 
#ifdef DBG_TIMEtoSHOW
  : beginTime(millis())
#endif
{
  setBrightness(0);
  setDither(mDither);
  setMaxmA(mMaxmA);
}

void AllLedStrips::init() 
{
  AddVarCode ("dither",   mDither = args[0]; setDither(args[0]),     mDither, 0,   1);
  AddVarCode ("maxmA",    mMaxmA  = args[0]; setMaxmA(args[0]),      mMaxmA,  100, 1000);
  AddVarCode ("bright",   mBright = args[0]; setBrightness(args[0]), mBright, 1,   255);
  AddBoolName("probe",    mProbe);
  AddVarName ("minProbe", mMinProbe, 1, mMaxProbe);

  #ifdef FASTLED_CORE
    xTaskCreatePinnedToCore(FastLEDshowTask, "FastLEDshowTask", FASTLED_STACK, nullptr, FASTLED_PRIO, &FastLEDshowTaskHandle, FASTLED_CORE);  
    _log << "Fastled runs on Core " << FASTLED_CORE << " with Prio " << FASTLED_PRIO << endl;
  #endif
}

void AllLedStrips::addObjs(AllObj& allobj)
{
  for (auto strip : *this)
    strip->addObjs(allobj);
}

bool AllLedStrips::addStrip(BaseLedStrip &strip)
{
  bool ok = mNStrips < MAXSTRIP;
  if (ok)
  {
    mStrips[mNStrips++] = &strip;
    strip.init();
  }
  else
    _log << ">> ERROR !! Max LedStrips is reached " << MAXSTRIP << endl; 

  return ok;
}

void AllLedStrips::setBrightness(const byte bright) 
{ 
  mRawBright = (bright * ((mFade >> 8) + 1)) >> 8; 
  FastLED.setBrightness(mRawBright); 
};

void AllLedStrips::update()
{
  #ifdef DBG_TIMEtoSHOW
    if (!mHasbegun)
    {
      _log << "------\n>> Time to Start the show: " << millis() - beginTime << "ms" << endl << "------" << endl;
      mHasbegun = true;
    }
  #endif

  // update all strips
  ulong t = GET_MILLIS();
  ulong dt = mLastT ? t - mLastT : 1; // to prevent possible /0
  mLastT += dt;

  for (auto strip : *this)
    strip->update(t, dt);

  // read probe and adjust brightness
  if(mProbe)
  {
    int light = analogRead(LDR_PIN);
    mBright = map(light, mMinProbe, mMaxProbe, 255, 0); // the darker the light, the brighter the leds
  }
  mFade = lerp16by16(mFade,  65535,  650);
  setBrightness(mBright); // use mFade

  // showing if dithering is off
  if (!mDither) show();
}

bool AllLedStrips::doDither()
{
  // to be called as much as possible for Fastled brightness dithering
  if (mDither) show(); 
  return mDither;
}

void AllLedStrips::showInfo()
{
  _log << "FPS " << FastLED.getFPS() << endl;
  for (auto strip : *this)
    strip->showInfo();
}

void AllLedStrips::show() 
{ 
  #ifdef FASTLED_CORE
    TriggerFastLEDShow();
  #else
    noInterrupts();
    FastLED.show();
    interrupts(); 
  #endif
}
