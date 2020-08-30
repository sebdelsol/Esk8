#pragma once

struct Raster
{
#ifdef DEBUG_RASTER
  static const int max = 20;

  Stream& mSerial;

  struct                                                                                                               
  {                                                                                                                           
    const char* name;                                                                                          
    long        time;                                                                                                                
  } r[max];                                                                                                     

  int  n;                                                                                                       
  long start;                                                                                                 
  long lastEnd;                                     

  Raster(Stream& serial) : mSerial(serial) {};

  void begin()
  {
    n = 0;                                                                                                           
    start = micros();
  };       

  void add(const char* name)
  {
    if (n < max)                                                                                              
    {                                                                                                                           
      r[n].time = micros();                                                                                   
      r[n++].name = name;                                                                                   
    }                                                                                                                           
    else                                                                                                                        
      mSerial << ">> ERROR !! Max Raster reached "  << n << endl;
  };

  void show()
  {
    long end = micros();                                                                                                   
    mSerial << "LOOP "  << (end - lastEnd) << "µs";                                                                    
    lastEnd = end;

    mSerial << " \t TOTAL " << (end - start) << "µs";                                                                  
    for(byte i=0; i < n; i++)                                                                                        
      mSerial << " \t - " << r[i].name << " " << (r[i].time - (i==0 ? start : r[i-1].time)) << "µs  "; 
    
    mSerial << "\t free Heap - " << ESP.getFreeHeap() << endl;
  };

#else
  Raster(Stream& serial){};
  void begin(){};       
  void add(const char* name){};
  void show(){};
#endif
};
