#ifndef CK2_PROVINCE_HH
#define CK2_PROVINCE_HH

#include "UtilityFunctions.hh"
#include "EU4Province.hh"

class CK2Province : public Enumerable<CK2Province>, public ObjectWrapper {
public:
  CK2Province (Object* o);

  void setEu4Province (EU4Province* t) {target = t;}
  EU4Province* getEu4Province () const {return target;}
private:
  EU4Province* target;
};

#endif
