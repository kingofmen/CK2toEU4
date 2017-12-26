#ifndef EU4COUNTRY_HH
#define EU4COUNTRY_HH

#include <string>

#include "EU4Province.hh"
#include "UtilityFunctions.hh"
#include "Logger.hh"

class CK2Ruler;
class CK2Title;

class EU4Country : public Enumerable<EU4Country>, public ObjectWrapper {
public:
  EU4Country (Object* o);

  void addProvince(EU4Province* prov);
  void remProvince (EU4Province* prov);
  void addBarony (Object* barony) {baronies.push_back(barony);}
  bool converts();
  CK2Ruler* getRuler () const {return ckSovereign;}
  CK2Title* getTitle () const {return ckTitle;}
  bool isROTW () const;
  void removeCore (EU4Province* prov);
  void setAsCore (EU4Province* prov);
  void setRuler (CK2Ruler* ruler, CK2Title* title);
  std::string getGovernment();
  void setGovernment(const std::string& government);

  EU4Province::Container& getProvinces() {return provinces;}
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
