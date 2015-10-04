#include "CK2Province.hh"
#include "EU4Province.hh"

ProvinceWeight const* const ProvinceWeight::Manpower      = new ProvinceWeight("pw_manpower", false);
ProvinceWeight const* const ProvinceWeight::Production    = new ProvinceWeight("pw_production", false);
ProvinceWeight const* const ProvinceWeight::Taxation      = new ProvinceWeight("pw_taxation", false);
ProvinceWeight const* const ProvinceWeight::Galleys       = new ProvinceWeight("pw_galleys", false);
ProvinceWeight const* const ProvinceWeight::Fortification = new ProvinceWeight("pw_fortification", true);

map<string, CK2Province*> CK2Province::baronyMap;

CK2Province::CK2Province (Object* o)
  : Enumerable<CK2Province>(this, o->getKey(), false)
  , ObjectWrapper(o)
  , countyTitle(0)
{
  weights.resize(ProvinceWeight::totalAmount());
  for (unsigned int i = 0; i < weights.size(); ++i) weights[i] = -1;

  objvec leaves = object->getLeaves();
  for (objiter leaf = leaves.begin(); leaf != leaves.end(); ++leaf) {
    if ((*leaf)->isLeaf()) continue;
    string baronyType = (*leaf)->safeGetString("type", "None");
    if (baronyType == "None") continue;
    baronies.push_back(*leaf);
    baronyMap[(*leaf)->getKey()] = this;
  }
}

void CK2Province::assignProvince (EU4Province* t) {
  targets.push_back(t);
  t->assignProvince(this);
}

double CK2Province::getWeight (ProvinceWeight const* const pw) const {
  return weights[*pw];
}

void CK2Province::calculateWeights (Object* weightObject, Object* troops, objvec& buildings) {
  for (unsigned int i = 0; i < weights.size(); ++i) weights[i] = 0;
  for (objiter barony = startBarony(); barony != finalBarony(); ++barony) {
    string baronyType = (*barony)->safeGetString("type", "None");
    if (baronyType == "None") continue;
    Object* typeWeight = weightObject->getNeededObject(baronyType);
    double buildingWeight = 0;
    double forts = 0;
    for (objiter building = buildings.begin(); building != buildings.end(); ++building) {
      if ((*barony)->safeGetString((*building)->getKey(), "no") != "yes") continue;
      buildingWeight += (*building)->safeGetFloat("weight");
      forts += (*building)->safeGetFloat("fort_level");
    }
    (*barony)->setLeaf(ProvinceWeight::Production->getName(), buildingWeight * typeWeight->safeGetFloat("prod", 0.5));
    (*barony)->setLeaf(ProvinceWeight::Taxation->getName(), buildingWeight * typeWeight->safeGetFloat("tax", 0.5));
    (*barony)->setLeaf(ProvinceWeight::Fortification->getName(), forts);

    Object* levyObject = (*barony)->safeGetObject("levy");
    double mpWeight = 0;
    double galleys = 0;
    if (levyObject) {
      objvec levies = levyObject->getLeaves();
      for (objiter levy = levies.begin(); levy != levies.end(); ++levy) {
	string key = (*levy)->getKey();
	double amount = (*levy)->tokenAsFloat(1);
	if (key == "galleys_f") {
	  galleys += amount;
	}
	else {
	  mpWeight += amount * troops->safeGetFloat(key, 0.0001);
	}
      }
    }
    (*barony)->setLeaf(ProvinceWeight::Manpower->getName(), mpWeight);
    (*barony)->setLeaf(ProvinceWeight::Galleys->getName(), galleys);
    for (ProvinceWeight::Iter p = ProvinceWeight::start(); p != ProvinceWeight::final(); ++p) {
      weights[**p] += (*barony)->safeGetFloat((*p)->getName());
    }
  }
}
