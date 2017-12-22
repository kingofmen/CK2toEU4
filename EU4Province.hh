#ifndef EU4_PROVINCE_HH
#define EU4_PROVINCE_HH

#include "CK2Province.hh"
#include "UtilityFunctions.hh"

class EU4Country;

class EU4Province : public Enumerable<EU4Province>, public ObjectWrapper {
public:
  EU4Province (Object* o);

  void addCore (string countryTag);
  void assignCountry (EU4Country* eu4);
  void assignProvince (CK2Province* ck);
  bool converts () const;
  EU4Country* getEU4Country () const {return eu4Country;}
  bool hasCore (string countryTag);
  int numCKProvinces () const {return ckProvinces.size();}
  void remCore (string countryTag);
  CK2Province::Container& ckProvs () {return ckProvinces;}
  CK2Province::Iter startProv () {return ckProvinces.begin();}
  CK2Province::Iter finalProv () {return ckProvinces.end();}
private:
  CK2Province::Container ckProvinces;
  EU4Country* eu4Country;
};

#endif
