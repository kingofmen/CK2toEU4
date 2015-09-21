#ifndef EU4COUNTRY_HH
#define EU4COUNTRY_HH

#include "EU4Province.hh"
#include "UtilityFunctions.hh"

class CK2Ruler;

class EU4Country : public Enumerable<EU4Country>, public ObjectWrapper {
public:
  EU4Country (Object* o);

  void addProvince (EU4Province* prov) {provinces.push_back(prov);}
  void addBarony (Object* barony) {baronies.push_back(barony);}
  CK2Ruler* getRuler () const {return ckSovereign;}
  void setRuler (CK2Ruler* ruler);

  EU4Province::Iter startProvince () {return provinces.begin();}
  EU4Province::Iter finalProvince () {return provinces.end();}
  objiter startBarony () {return baronies.begin();}
  objiter finalBarony () {return baronies.end();}
private:
  CK2Ruler* ckSovereign;
  objvec baronies;
  EU4Province::Container provinces;
};

#endif
