#include <ledstrip.h>

// ----------------------------------------------------
#ifdef FASTLED_SHOW_CORE
  TaskHandle_t FastLEDshowTaskHandle;
  TaskHandle_t userTaskHandle = 0;

  void TriggerFastLEDShow()
  {
    if (userTaskHandle == 0)
    {
      userTaskHandle = xTaskGetCurrentTaskHandle(); //so that the show task can notify when it's done
      xTaskNotifyGive(FastLEDshowTaskHandle); // trigger fastled show task
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FASTLED_WAIT_TASKSHOW)); // Wait to be notified that it's done
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
AllLedStrips::AllLedStrips(const int maxmA, Stream& serial) : mSerial(serial)
{
  FastLED.setMaxPowerInVoltsAndMilliamps(5, maxmA);
  FastLED.countFPS();
  FastLED.setDither(BINARY_DITHER);
}

void AllLedStrips::init() 
{
  #ifdef FASTLED_SHOW_CORE
    xTaskCreatePinnedToCore(FastLEDshowTask, "FastLEDshowTask", 2048, nullptr, FASTLED_TASK_PRIO, &FastLEDshowTaskHandle, FASTLED_SHOW_CORE);  
    mSerial << "Fastled runs on Core " << FASTLED_SHOW_CORE << " with Prio " << FASTLED_TASK_PRIO << endl;
  #endif
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
    mSerial << ">> ERROR !! Max LedStrips is reached " << MAXSTRIP << endl; 

  return ok;
}

void AllLedStrips::clearAndShow() 
{ 
  for(byte i=0; i < 3; i++)
    FastLED.clear(true);
}

void AllLedStrips::update()
{
  ulong t = GET_MILLIS();
  ulong dt = mLastT ? t - mLastT : 1; // to prevent possible /0
  mLastT += dt;

  for (byte i=0; i < mNStrips; i++)
    mStrips[i]->update(t, dt);
}

void AllLedStrips::showInfo()
{
  mSerial << "FPS " << FastLED.getFPS() << endl;
  for (byte i=0; i < mNStrips; i++)
    mStrips[i]->showInfo();
}

void AllLedStrips::show() 
{ 
  #ifdef FASTLED_SHOW_CORE
    TriggerFastLEDShow();
  #else
    noInterrupts();
    FastLED.show();
    interrupts(); 
  #endif
}
