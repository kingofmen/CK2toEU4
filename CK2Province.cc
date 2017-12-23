#include "CK2Province.hh"
#include "EU4Province.hh"
#include "Logger.hh"
#include "UtilityFunctions.hh"

ProvinceWeight const* const ProvinceWeight::Manpower      = new ProvinceWeight("pw_manpower", false);
ProvinceWeight const* const ProvinceWeight::Production    = new ProvinceWeight("pw_production", false);
ProvinceWeight const* const ProvinceWeight::Taxation      = new ProvinceWeight("pw_taxation", false);
ProvinceWeight const* const ProvinceWeight::Galleys       = new ProvinceWeight("pw_galleys", false);
ProvinceWeight const* const ProvinceWeight::Trade         = new ProvinceWeight("pw_trade", false);
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
  int baronies = 0;
  for (objiter barony = startBarony(); barony != finalBarony(); ++barony) {
    string baronyType = (*barony)->safeGetString("type", "None");
    if (baronyType == "None") continue;
    ++baronies;
    Object* typeWeight = weightObject->getNeededObject(baronyType);
    double buildingWeight = typeWeight->safeGetFloat("cost");
    double forts = 0;
    vector<pair<string, double>> province_buildings;
    for (auto* building : buildings) {
      string key = building->getKey();
      if ((*barony)->safeGetString(key, "no") != "yes") continue;
      double weight = building->safeGetFloat("weight");
      buildingWeight += weight;
      province_buildings.emplace_back(key, weight);
      forts += building->safeGetFloat("fort_level");
    }
    (*barony)->setLeaf(ProvinceWeight::Production->getName(),
                       buildingWeight * typeWeight->safeGetFloat("prod", 0.5));
    (*barony)->setLeaf(ProvinceWeight::Taxation->getName(),
                       buildingWeight * typeWeight->safeGetFloat("tax", 0.5));
    (*barony)->setLeaf(ProvinceWeight::Fortification->getName(), forts);
    Object* levyObject = (*barony)->safeGetObject("levy");
    double mpWeight = 0;
    double galleys = 0;
    if (levyObject) {
      objvec levies = levyObject->getLeaves();
      for (auto* levy : levies) {
        string key = levy->getKey();
        double amount = getLevyStrength(key, levyObject);
        if (key == "galleys_f") {
          galleys += amount;
        } else {
          mpWeight += amount * troops->safeGetFloat(key, 0.0001);
        }
      }
    }

    (*barony)->setLeaf(ProvinceWeight::Manpower->getName(), mpWeight);
    (*barony)->setLeaf(ProvinceWeight::Galleys->getName(), galleys);
    Logger::logStream("buildings")
        << (*barony)->getKey() << " in " << getKey() << " ("
        << safeGetString("name", "name_not_found") << ") has weights:\n"
        << LogOption::Indent;
    for (ProvinceWeight::Iter p = ProvinceWeight::start();
         p != ProvinceWeight::final(); ++p) {
      weights[**p] += (*barony)->safeGetFloat((*p)->getName());
      Logger::logStream("buildings")
          << (*p)->getName() << " : "
          << (*barony)->safeGetFloat((*p)->getName()) << "\n";
    }
    Logger::logStream("buildings")
        << "from " << province_buildings.size() << " buildings: ";
    for (const auto& building : province_buildings) {
      Logger::logStream("buildings")
          << "(" << building.first << " " << building.second << ") ";
    }
    Logger::logStream("buildings") << "\n" << LogOption::Undent;

  }
  Logger::logStream("provinces") << "Found " << baronies << " settlements in "
                                 << nameAndNumber(this) << "\n";
  // Doesn't seem to be used any more?
  weights[*ProvinceWeight::Trade] = safeGetInt("realm_tradeposts");
}
