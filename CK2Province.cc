#include "CK2Province.hh"
#include "EU4Province.hh"

ProvinceWeight const* const ProvinceWeight::Manpower = new ProvinceWeight("pw_manpower", false);
ProvinceWeight const* const ProvinceWeight::Production = new ProvinceWeight("pw_production", false);
ProvinceWeight const* const ProvinceWeight::Taxation = new ProvinceWeight("pw_taxation", true);

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

double CK2Province::getWeight (ProvinceWeight const* const pw) {
  if (0 <= weights[*pw]) return weights[*pw];
  for (unsigned int i = 0; i < weights.size(); ++i) weights[i] = 0;
  for (objiter barony = startBarony(); barony != finalBarony(); ++barony) {
    for (ProvinceWeight::Iter p = ProvinceWeight::start(); p != ProvinceWeight::final(); ++p) {
      weights[**p] += (*barony)->safeGetFloat((*p)->getName());
    }
  }

  return weights[*pw];
}

