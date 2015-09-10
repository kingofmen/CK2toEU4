#ifndef CK2_PROVINCE_HH
#define CK2_PROVINCE_HH

#include <vector>
#include "UtilityFunctions.hh"

class CK2Title;
class EU4Province;

class ProvinceWeight : public Enumerable<ProvinceWeight> {
public:
  ProvinceWeight (string n, bool f) : Enumerable<ProvinceWeight>(this, n, f) {}
  static ProvinceWeight const* const Manpower;
  static ProvinceWeight const* const Production;
  static ProvinceWeight const* const Taxation;
};

class CK2Province : public Enumerable<CK2Province>, public ObjectWrapper {
public:
  CK2Province (Object* o);

  void assignProvince (EU4Province* t);
  CK2Title* getCountyTitle () const {return countyTitle;}
  double getWeight (ProvinceWeight const* const pw);
  int numEU4Provinces () const {return targets.size();}
  void setCountyTitle (CK2Title* t) {countyTitle = t;}

  objiter startBarony () {return baronies.begin();}
  objiter finalBarony () {return baronies.end();}
  
private:
  CK2Title* countyTitle;
  objvec baronies;
  vector<EU4Province*> targets;
  vector<double> weights;
};

#endif
