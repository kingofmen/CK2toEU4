#ifndef CK2_PROVINCE_HH
#define CK2_PROVINCE_HH

#include <string>
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
  static ProvinceWeight const* const Galleys;
  static ProvinceWeight const* const Fortification;
};

class CK2Province : public Enumerable<CK2Province>, public ObjectWrapper {
public:
  CK2Province (Object* o);

  void addBarony (Object* house) {baronies.push_back(house);}
  void assignProvince (EU4Province* t);
  void calculateWeights (Object* weightObject, objvec& buildings);
  CK2Title* getCountyTitle () const {return countyTitle;}
  double getWeight (ProvinceWeight const* const pw) const;
  int numEU4Provinces () const {return targets.size();}
  void setCountyTitle (CK2Title* t) {countyTitle = t;}
  double totalWeight () const;
  double totalTech () const;

  EU4Province* eu4Province(int idx) { return targets[idx]; }
  vector<EU4Province*>& eu4Provinces() { return targets; }
  vector<EU4Province*>::iterator startEU4Province() { return targets.begin(); }
  vector<EU4Province*>::iterator finalEU4Province() { return targets.end(); }
  objiter startBarony() { return baronies.begin(); }
  objiter finalBarony() { return baronies.end(); }

  static CK2Province* getFromBarony (string baronyTag) {return baronyMap[baronyTag];}
  static std::vector<const ProvinceWeight*> weight_areas;

private:
  CK2Title* countyTitle;
  objvec baronies;
  vector<EU4Province*> targets;
  vector<double> weights;

  static map<string, CK2Province*> baronyMap;
};

#endif
