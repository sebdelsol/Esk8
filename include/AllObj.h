#pragma once

#include <Streaming.h>
#include <ObjVar.h>
#include <Buf.h>
#include <SPIFFS.h>

// #define DBG_CMD  // to see what's happening with send & received cmd
#define ALLOBJ_MAXOBJ    20

#define ALLOBJ_MOTION_CMD  '!'
#define ALLOBJ_1ST_ID      (ALLOBJ_MOTION_CMD + 1)
#define ALLOBJ_TERM        '\n' 
#define ALLOBJ_ALIVE       '~'

#define ALLOBJ_SET     "set"
#define ALLOBJ_GET     "get"
#define ALLOBJ_INIT    "init"

#define FNAME_CURRENT "/config.cfg"
#define FNAME_DEFAULT "/config.def"

#define FORMAT_SPIFFS_IF_FAILED true

//-------------------------------
class AllObj 
{
  struct parsedCmd 
  {
    OBJVar* obj;
    MyVar* var;
    const char* objName;
    const char* varName;
  };

  struct mRegisteredOBJ
  {
    char* name;
    OBJVar* obj;
  } mOBJ[ALLOBJ_MAXOBJ];

  byte mNOBJ = 0;
  byte mID = 0;

  Stream& mDbgSerial;
  
  bool spiffsOK = false;
  const char* cfg_fname = FNAME_CURRENT;
  const char* def_fname = FNAME_DEFAULT;

  BUF mTmpBuf;
  
  #ifdef DBG_CMD
    void dbgCmd(const char* cmdKeyword, const parsedCmd& parsed, int nbArg, int* args);
  #endif

  bool isNumber(const char* txt);
  OBJVar* getObjFromName(const char* name); 

  void handleSetCmd(const parsedCmd& parsed, BUF& buf, bool change);
  void handleGetCmd(const parsedCmd& parsed, Stream& stream, bool compact);
  void handleInitCmd(const parsedCmd& parsed, Stream& stream);
  bool getObjVar(parsedCmd& parsed, BUF& buf);
  void handleCmd(Stream& stream, BUF& buf, bool change = true, bool compact = false);
  File getFile(bool isdefault, const char* mode);

protected:
  const char* mSetKeyword = ALLOBJ_SET;
  const char* mGetKeyword = ALLOBJ_GET;
  const char* mInitKeyword = ALLOBJ_INIT;

  void readCmdFromStream(Stream& stream, BUF& buf, bool change = true, bool compact = false);
  void emulateCmdForAllVars(const char* cmdKeyword, Stream& stream, OBJVar::ObjTestVarFunc testVar = NULL, bool change = true, bool compact = false);

public:
  AllObj(Stream& dbgSerial);
  bool registerObj(OBJVar& obj, const char* name);
  void init();
  void save(bool isdefault);
  void load(bool isdefault, bool change = true);
};