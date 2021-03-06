#pragma once

#include <log.h>

// #define DBG_HASH

// hash for [name] => Class* obj, with a Class::getname() method defined
template <class Class, int N>
class HashName 
{
  // the bigger multiplicator, the less collisions
  static constexpr int getN(uint8_t n) { return n * 2; }; 

  Class*      objs[getN(N)] = {nullptr};
  uint8_t     maxCol = 0;

  // djb2: http://www.cse.yorku.ca/~oz/hash.html
  uint8_t hash(const char *name) 
  {
    unsigned long hash = 5381;
    int c;
    while (c = *name++)
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash % getN(N);;
  }

  // quadratic probing
  uint8_t next(uint8_t i, uint8_t col) 
  { 
    return (i + col * col) % getN(N); 
  };

  bool hasAnotherName(Class* obj, const char* mename) 
  {
    const char* objname = obj->getName();
    return objname != nullptr && strcmp(objname, mename) != 0;
  };

public:

  // obj needs to implement the getName() method
  void add(Class* obj)
  {
    assert (obj!=nullptr);
    const char *name = obj->getName();
    assert (name!=nullptr);
    
    // already exists ??
    assert(get(name) == nullptr); 
    
    uint8_t col = 0;
    uint8_t i = hash(name);
    
    // lookup for an empty slot
    while(objs[i] != nullptr) 
      i = next(i, ++col);

    if (col > maxCol) maxCol = col;
    
    #ifdef DBG_HASH
      if (col > 0) _log << " [" << name << "]: +" << col << " lookup" << (col > 1 ? "s" : "") << endl;
    #endif
    
    objs[i] = obj;
  };

  // return an obj or nullptr if nothing found
  Class* get(const char *name)
  {
    assert (name!=nullptr);
    
    uint8_t col = 0;
    uint8_t i = hash(name);
    
    // lookup for empty slot or an obj->name == name
    while(objs[i] != nullptr && hasAnotherName(objs[i], name) ) 
    {
      // failed coz too much collisions ?
      if (++col > maxCol) return nullptr; 
      i = next(i, col);
    }

    return objs[i];
  };
};
