#ifndef EU4_PROVINCE_HH
#define EU4_PROVINCE_HH

#include "CK2Province.hh"
#include "UtilityFunctions.hh"

class EU4Province : public Enumerable<EU4Province>, public ObjectWrapper {
public:
  EU4Province (Object* o);

  void assignProvince (CK2Province* ck);
  int numCKProvinces () const {return ckProvinces.size();}
  CK2Province::Iter startProv () {return ckProvinces.begin();}
  CK2Province::Iter finalProv () {return ckProvinces.end();}
private:
  CK2Province::Container ckProvinces;
};

#endif
