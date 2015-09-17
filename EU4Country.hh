#ifndef EU4COUNTRY_HH
#define EU4COUNTRY_HH

#include "CK2Title.hh"
#include "EU4Province.hh"
#include "UtilityFunctions.hh"

class EU4Country : public Enumerable<EU4Country>, public ObjectWrapper {
public:
  EU4Country (Object* o);

  void addProvince (EU4Province* prov) {provinces.push_back(prov);}
  void assignCountry (CK2Title* ck2);
  CK2Ruler* getRuler () const {return ckSovereign;}
  void setRuler (CK2Ruler* ruler);

  EU4Province::Iter startProvince () {return provinces.begin();}
  EU4Province::Iter finalProvince () {return provinces.end();}
private:
  CK2Ruler* ckSovereign;
  CK2Title::Container ck2Titles;
  EU4Province::Container provinces;
};

#endif
