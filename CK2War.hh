#ifndef CK2_WAR_HH
#define CK2_WAR_HH

#include "CK2Ruler.hh"
#include "UtilityFunctions.hh"

class CK2War : public Enumerable<CK2War>, public ObjectWrapper {
public:
  CK2War (Object* obj);
private:
  CK2Ruler::Container attackers;
  CK2Ruler::Container defenders;
};

#endif
