#ifndef EU4COUNTRY_HH
#define EU4COUNTRY_HH

#include "EU4Province.hh"
#include "UtilityFunctions.hh"
#include "Logger.hh"
class CK2Ruler;
class CK2Title;

class EU4Country : public Enumerable<EU4Country>, public ObjectWrapper {
public:
  EU4Country (Object* o);

  void addProvince (EU4Province* prov) {if (find(provinces.begin(), provinces.end(), prov) == provinces.end()) provinces.push_back(prov);}
  void remProvince (EU4Province* prov) {REMOVE(provinces, prov);}
  void addBarony (Object* barony) {baronies.push_back(barony);}
  bool converts();
  CK2Ruler* getRuler () const {return ckSovereign;}
  CK2Title* getTitle () const {return ckTitle;}
  void setRuler (CK2Ruler* ruler, CK2Title* title);

  EU4Province::Iter startProvince () {return provinces.begin();}
  EU4Province::Iter finalProvince () {return provinces.end();}
  objiter startBarony () {return baronies.begin();}
  objiter finalBarony () {return baronies.end();}

  static const string kNoProvinceMarker;
private:
  CK2Ruler* ckSovereign;
  CK2Title* ckTitle;
  objvec baronies;
  EU4Province::Container provinces;
};

#endif
