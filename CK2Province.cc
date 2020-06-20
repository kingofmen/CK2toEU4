#include "CK2Province.hh"

#include <unordered_map>
#include <unordered_set>

#include "constants.hh"
#include "CK2Title.hh"
#include "CK2Ruler.hh"
#include "EU4Province.hh"
#include "Logger.hh"
#include "UtilityFunctions.hh"

ProvinceWeight const* const ProvinceWeight::Manpower      = new ProvinceWeight("pw_manpower", false);
ProvinceWeight const* const ProvinceWeight::Production    = new ProvinceWeight("pw_production", false);
ProvinceWeight const* const ProvinceWeight::Taxation      = new ProvinceWeight("pw_taxation", false);
ProvinceWeight const* const ProvinceWeight::Galleys       = new ProvinceWeight("pw_galleys", false);
ProvinceWeight const* const ProvinceWeight::Fortification = new ProvinceWeight("pw_fortification", true);

map<string, CK2Province*> CK2Province::baronyMap;
std::vector<const ProvinceWeight*> CK2Province::weight_areas = {
  ProvinceWeight::Manpower, ProvinceWeight::Production, ProvinceWeight::Taxation};

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

void CK2Province::calculateWeights (Object* minWeights, objvec& buildings) {
  for (unsigned int i = 0; i < weights.size(); ++i) weights[i] = 0;
  int baronies = 0;
  for (objiter barony = startBarony(); barony != finalBarony(); ++barony) {
    string baronyType = (*barony)->safeGetString("type", "None");
    if (baronyType == "None") continue;
    ++baronies;
    std::unordered_set<Object*> province_buildings;
    std::map<std::string, double> areaWeights;
    for (auto p = ProvinceWeight::start(); p != ProvinceWeight::final(); ++p) {
      std::string areaName = (*p)->getName();
      double multiplier = 0;
      for (auto* building : buildings) {
        string key = building->getKey();
        if (((*barony)->safeGetString(key, "no") != "yes") &&
            (baronyType != key)) {
          continue;
        }
        double weight = building->safeGetFloat(areaName);
        areaWeights[areaName] += weight;
        multiplier += building->safeGetFloat(areaName + "_mult");
        if (weight + multiplier > 0) {
          province_buildings.insert(building);
        }
      }
      (*barony)->setLeaf(areaName, areaWeights[areaName] * (1 + multiplier));
    }
    Logger::logStream("buildings") << (*barony)->getKey() << " in "
                                   << nameAndNumber(this) << " has weights:\n"
                                   << LogOption::Indent;
    for (auto p = ProvinceWeight::start(); p != ProvinceWeight::final(); ++p) {
      std::string areaName = (*p)->getName();
      double bWeight = (*barony)->safeGetFloat(areaName);
      weights[**p] += bWeight;

      Logger::logStream("buildings")
          << (*p)->getName() << " : " << bWeight << "\n";
      if (bWeight < 0.001) {
        continue;
      }
      Logger::logStream("buildings") << LogOption::Indent << "from: ";
      for (const auto* building : province_buildings) {
        double weight = building->safeGetFloat(areaName);
        double mult = building->safeGetFloat(areaName + "_mult");
        if (weight + mult < 0.00001) {
          continue;
        }
        Logger::logStream("buildings") << "(" << building->getKey();
        if (weight > 0) {
          Logger::logStream("buildings") << " " << weight;
        }
        if (mult > 0) {
          Logger::logStream("buildings") << " x" << mult;
        }
        Logger::logStream("buildings") << ") ";
      }
      Logger::logStream("buildings") << "\n" << LogOption::Undent;
    }
    Logger::logStream("buildings") << LogOption::Undent;
  }
  Logger::logStream("provinces") << "Found " << baronies << " settlements in "
                                 << nameAndNumber(this) << "\n";

  for (auto p = ProvinceWeight::start(); p != ProvinceWeight::final(); ++p) {
    double minimum = minWeights->safeGetFloat((*p)->getName());
    if (weights[**p] < minimum) {
      weights[**p] = minimum;
    }
  }

  Object* nerf = minWeights->getNeededObject("special_nerfs");
  Object* govWeights = minWeights->getNeededObject("government_weights");
  double de_jure_nerf = 1;
  auto* liege = countyTitle;
  while (liege != nullptr) {
    string key = liege->getKey();
    double current_nerf = nerf->safeGetFloat(key, 1);
    if (current_nerf != 1) {
      de_jure_nerf *= current_nerf;
      Logger::logStream("provinces")
          << "Nerfing " << nameAndNumber(this) << " by " << current_nerf
          << " due to de-jure " << key << ".\n";
    }
    liege = liege->getDeJureLiege();
  }
  liege = countyTitle;
  std::unordered_set<CK2Ruler*> rulers;
  while (liege != nullptr) {
    auto* ruler = liege->getRuler();
    if (ruler != nullptr && rulers.find(ruler) == rulers.end()) {
      rulers.insert(ruler);
      string govKey = ruler->safeGetString(governmentString);
      double currGov = govWeights->safeGetFloat(govKey, 1);
      if (currGov != 1) {
        de_jure_nerf *= currGov;
        Logger::logStream("provinces")
            << "Nerfing " << nameAndNumber(this) << " by " << currGov
            << " due to government " << govKey << " of " << nameAndNumber(ruler)
            << ".\n";
      }
    }
    liege = liege->getLiege();
  }
  for (unsigned int i = 0; i < weights.size(); ++i) {
    weights[i] *= de_jure_nerf;
  }
}

double CK2Province::totalWeight() const {
  return getWeight(ProvinceWeight::Production) +
         getWeight(ProvinceWeight::Manpower) +
         getWeight(ProvinceWeight::Taxation);
}

double CK2Province::totalTech() const {
  Object* tech = safeGetObject("technology");
  if (!tech) return 0;
  tech = tech->safeGetObject("tech_levels");
  if (!tech) return 0;
  double ret = 0;
  for (int i = 0; i < tech->numTokens(); ++i) {
    ret += tech->tokenAsFloat(i);
  }
  return ret;
}
