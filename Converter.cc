#include <algorithm>
#include <cstdio>
#include <direct.h>
#include <deque>
#include <exception>
#include <iostream> 
#include <string>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "constants.hh"
#include "Converter.hh"
#include "CK2Province.hh"
#include "CK2Ruler.hh"
#include "CK2Title.hh"
#include "CK2War.hh"
#include "EU4Province.hh"
#include "EU4Country.hh"
#include "Logger.hh"
#include "Parser.hh"
#include "StructUtils.hh" 
#include "StringManips.hh"
#include "UtilityFunctions.hh"
#include "Window.hh"

using namespace std;

ConverterJob const *const ConverterJob::Convert =
    new ConverterJob("convert", false);
ConverterJob const *const ConverterJob::DebugParser =
    new ConverterJob("debug", false);
ConverterJob const *const ConverterJob::DejureLieges =
    new ConverterJob("dejures", false);
ConverterJob const *const ConverterJob::CheckProvinces =
    new ConverterJob("check_provinces", false);
ConverterJob const *const ConverterJob::LoadFile =
    new ConverterJob("loadfile", false);
ConverterJob const *const ConverterJob::PlayerWars = 
    new ConverterJob("player_wars", false);
ConverterJob const *const ConverterJob::Statistics = 
    new ConverterJob("statistics", false);
ConverterJob const *const ConverterJob::DynastyScores = 
    new ConverterJob("dynasty_scores", true);

namespace {
unordered_set<EU4Country*> independenceRevolts;
Object* euLeaderTraits = nullptr;
Object* ck_province_setup = nullptr;
Object* eu4_areas = nullptr;
map<string, vector<string> > religionMap;
map<string, vector<string> > cultureMap;
const string kStateKey = "part_of_state";
map<string, unordered_set<EU4Province*>> area_province_map;
std::string gameDate = "";
int gameDays = 0;
}

Converter::Converter (Window* ow, string fn)
  : ck2FileName(fn)
  , ck2Game(0)
  , eu4Game(0)
  , ckBuildingObject(0)
  , ckBuildingWeights(0)
  , configObject(0)
  , countryMapObject(0)
  , customObject(0)
  , deJureObject(0)
  , euBuildingObject(0)
  , provinceMapObject(0)
  , outputWindow(ow)
{
  configure(); 
}  

Converter::~Converter () {
  if (eu4Game) delete eu4Game;
  if (ck2Game) delete ck2Game; 
  eu4Game = 0;
  ck2Game = 0; 
}

void Converter::run () {
  char* emergency = new char[16384];
  while (true) {
    if (jobsToDo.empty()) {
      sleep(5);
      continue;
    }

    ConverterJob const* const job = jobsToDo.front();
    jobsToDo.pop();
    try {
      if (ConverterJob::Convert        == job) convert();
      if (ConverterJob::DebugParser    == job) debugParser();
      if (ConverterJob::DejureLieges   == job) dejures();
      if (ConverterJob::LoadFile       == job) loadFile();
      if (ConverterJob::CheckProvinces == job) checkProvinces();
      if (ConverterJob::PlayerWars     == job) playerWars();
      if (ConverterJob::Statistics     == job) statistics();
      if (ConverterJob::DynastyScores  == job) dynastyScores();
    } catch(const std::bad_alloc& e) {
      delete emergency;
      Logger::logStream(LogStream::Error)
          << "Caught bad alloc: " << e.what() << "\n";
      break;
    } catch(const std::runtime_error& e) {
      Logger::logStream(LogStream::Error)
          << "Caught runtime error: " << e.what() << "\n";
      break;
    } catch(const std::exception& e) {
      Logger::logStream(LogStream::Error)
          << "Caught standard exception: " << e.what() << "\n";
      break;
    } catch (...) {
      delete emergency;
      Logger::logStream(LogStream::Error) << "Caught unknown exception!\n";
      break;
    }
  }
}

void Converter::loadFile () {
  if (ck2FileName == "") return;
  Parser::ignoreString = "CK2txt";
  Parser::specialCases["de_jure_liege=}"] = "";
  Parser::specialCases["\t="] = "special_f=";
  Parser::specialCases["\\\""] = "'";
  ck2Game = loadTextFile(ck2FileName);
  Parser::ignoreString = "";
  Parser::specialCases.clear();
  Logger::logStream(LogStream::Info) << "Ready to convert.\n";
}

void Converter::cleanUp () {
  // Restore the initial '-' in province tags.
  string minus("-");
  for (auto* eu4prov : EU4Province::getAll()) {
    eu4prov->object->setKey(minus + eu4prov->getKey());
    eu4prov->unsetValue(kStateKey);
    if (eu4prov->converts()) {
      eu4prov->unsetValue("fort_level");
      eu4prov->unsetValue("base_fort_level");
      eu4prov->unsetValue("influencing_fort");
      eu4prov->unsetValue("fort_influencing");
      eu4prov->unsetValue("estate");
      auto* history = eu4prov->safeGetObject("history");
      if (history) {
        history->unsetValue("seat_in_parliament");
      }
    }
  }

  string infantryType = configObject->safeGetString("infantry_type", "\"western_medieval_infantry\"");
  string cavalryType = configObject->safeGetString("cavalry_type", "\"western_medieval_knights\"");
  for (auto* eu4country : EU4Country::getAll()) {
    eu4country->unsetValue("needs_heir");
    eu4country->unsetValue(EU4Country::kNoProvinceMarker);
    double fraction = eu4country->safeGetFloat("manpower_fraction");
    eu4country->unsetValue("manpower_fraction");
    eu4country->unsetValue("ck_troops");
    if (!eu4country->getRuler()) continue;
    Object* unitTypes = eu4country->getNeededObject("sub_unit");
    unitTypes->resetLeaf("infantry", infantryType);
    unitTypes->resetLeaf("cavalry", cavalryType);
    eu4country->resetLeaf("army_tradition", "0.000");
    eu4country->resetLeaf("navy_tradition", "0.000");
    eu4country->resetLeaf("papal_influence", "0.000");
    eu4country
        ->resetLeaf("mercantilism",
                    eu4country->safeGetString("government") ==
                            "merchant_republic"
                        ? "0.250"
                        : "0.100");
    eu4country->resetLeaf("last_bankrupt", "1.1.1");
    eu4country->resetLeaf("last_focus_move", "1.1.1");
    eu4country->resetLeaf("last_conversion", "1.1.1");
    eu4country->resetLeaf("last_sacrifice", "1.1.1");
    eu4country->resetLeaf("wartax", "1.1.1");
    eu4country->resetLeaf("update_opinion_cache", "yes");
    eu4country->resetLeaf("needs_refresh", "yes");
    eu4country->unsetValue("last_debate");
    double max_manpower = 10;
    for (auto* eu4prov : eu4country->getProvinces()) {
      double autonomy = eu4prov->safeGetFloat("local_autonomy");
      if (autonomy < 75)
        autonomy = 75;
      autonomy = 0.01 * (100 - autonomy);
      // 250 per manpower point, and 1 = 1000.
      max_manpower += 0.25 * eu4prov->safeGetFloat("base_manpower") * autonomy;
    }
    eu4country->resetLeaf("manpower", fraction * max_manpower);
    eu4country->resetLeaf("technology_group", "western");
    eu4country->resetHistory("technology_group", "western");
    objvec estates = eu4country->getValue("estate");
    for (auto* estate : estates) {
      estate->unsetValue("province");
      estate->resetLeaf("loyalty", "40.000");
      estate->resetLeaf("total_loyalty", "40.000");
      estate->resetLeaf("influence", "40.000");
      estate->resetLeaf("province_influence", "0.000");
    }
  }

  eu4Game->getNeededObject("combat")->clear();
  eu4Game->unsetValue("previous_war");
}

void Converter::configure () {
  configObject = processFile("config.txt");
  Logger::logStream(LogStream::Debug).setActive(false);

  Object* debug = configObject->safeGetObject("streams");
  if (debug) {
    if (debug->safeGetString("generic", "no") == "yes") Logger::logStream(LogStream::Debug).setActive(true);
    objvec streams = debug->getLeaves();
    for (objiter str = streams.begin(); str != streams.end(); ++str) {
      string str_name = (*str)->getKey();
      if (str_name == "generic") continue;
      if (str_name == "all") continue;
      if (!LogStream::findByName(str_name)) {
	LogStream const* new_stream = new LogStream(str_name);
	Logger* newlog = Logger::createStream(new_stream);
	QObject::connect(newlog, SIGNAL(message(QString)), outputWindow, SLOT(message(QString)));
      }
      Logger::logStream(str_name).setActive((*str)->getLeaf() == "yes");
    }

    bool activateAll = (debug->safeGetString("all", "no") == "yes");
    if (activateAll) {
      for (LogStream::Iter stream = LogStream::start(); stream != LogStream::final(); ++stream) {
	Logger::logStream(*stream).setActive(true);
      }
    }
  }

  CK2Ruler::humansSovereign = configObject->safeGetString("humans_always_independent", "no") == "yes";
}

void Converter::dejures () {
  Logger::logStream(LogStream::Info)
      << "Outputting de-jure lieges from savegame.\n";
  configure();
  if (!createCK2Objects()) return;

  for (CK2Title::Iter title = CK2Title::start(); title != CK2Title::final(); ++title) {
    if ((*title)->safeGetString("landless", "no") == "yes") continue;
    Object* deJureObj = (*title)->safeGetObject("de_jure_liege");
    string deJureTag = (*title)->safeGetString("de_jure_liege", PlainNone);
    if (deJureObj && deJureTag == PlainNone)
      deJureTag = remQuotes(deJureObj->safeGetString("title", QuotedNone));
    if (deJureTag == PlainNone) {
      continue;
    }
    (*title)->setDeJureLiege(CK2Title::findByName(deJureTag));
  }

  for (CK2Title::Iter title = CK2Title::start(); title != CK2Title::final(); ++title) {
    if ((*title)->safeGetString("landless", "no") == "yes") continue;
    auto* liege = (*title)->getDeJureLiege();
    if (!liege) {
      continue;
    }
    Logger::logStream(LogStream::Info)
        << (*title)->getTag() << " = " << liege->getTag() << "\n";
  }
  Logger::logStream(LogStream::Info) << "Done with de-jure lieges.\n";
}

void Converter::debugParser () {
  objvec parsed = ck2Game->getLeaves();
  Logger::logStream(LogStream::Info) << "Last parsed object:\n"
                                     << parsed.back();
}

struct TitleStats {
  CK2Title* title;
  std::vector<CK2Province*> ck2Provinces;
  void print() const;
};

std::unordered_map<CK2Title*, TitleStats> statsMap;

void TitleStats::print() const {
  if (ck2Provinces.empty()) {
    return;
  }
  unsigned int numToPrint = 2;
  if (ck2Provinces.size() > 5) numToPrint++;
  if (ck2Provinces.size() > 9) numToPrint++;
  if (ck2Provinces.size() > 14) numToPrint++;
  if (ck2Provinces.size() > 20) numToPrint++;
  if (numToPrint > ck2Provinces.size())
    numToPrint = ck2Provinces.size();
  double avg_tech = 0;
  for (const auto* p : ck2Provinces) {
    avg_tech += p->totalTech();
  }
  avg_tech /= ck2Provinces.size();

  Logger::logStream(LogStream::Info) << title->getKey() << ":\n"
                                     << LogOption::Indent;
  Logger::logStream(LogStream::Info)
      << "CK2 provinces: " << ck2Provinces.size() << "\n";
  Logger::logStream(LogStream::Info)
      << "Best provinces:\n" << LogOption::Indent;
  for (unsigned int i = 0; i < numToPrint; ++i) {
    CK2Province* curr = ck2Provinces[i];
    EU4Province* conv = NULL;
    if (curr->numEU4Provinces() > 0) {
      conv = curr->eu4Province(0);
    }
    Logger::logStream(LogStream::Info)
        << nameAndNumber(curr) << " (" << curr->totalWeight() << ") -> "
        << nameAndNumber(conv) << "(" << (conv ? conv->totalDev() : 0.0)
        << ")\n";
  }
  Logger::logStream(LogStream::Info) << LogOption::Undent;

  Logger::logStream(LogStream::Info) << "Average tech: " << avg_tech << "\n";
  Logger::logStream(LogStream::Info) << LogOption::Undent;
}

bool Converter::displayStats() {
  Logger::logStream(LogStream::Info) << "Statistics:\n" << LogOption::Indent;
  for (auto prov = CK2Province::start(); prov != CK2Province::final(); ++prov) {
    auto* title = (*prov)->getCountyTitle();
    if (!title) continue;
    while ((title = title->getDeJureLiege()) != 0) {
      if (title->getLevel() != TitleLevel::Kingdom &&
          title->getLevel() != TitleLevel::Empire) {
        continue;
      }
      if (statsMap.find(title) == statsMap.end()) {
        statsMap[title].title = title;
      }
      statsMap[title].ck2Provinces.push_back(*prov);
    }
  }

  for (auto& ts : statsMap) {
    auto& stat = ts.second;
    std::sort(stat.ck2Provinces.begin(), stat.ck2Provinces.end(),
              [](const CK2Province* one, const CK2Province* two) {
                // Descending order, greater-than.
                return one->totalWeight() > two->totalWeight();
              });
  }

  for (auto emp = CK2Title::startEmpire(); emp != CK2Title::finalEmpire();
       ++emp) {
    statsMap[*emp].print();
  }
  for (auto kng = CK2Title::startLevel(TitleLevel::Kingdom);
       kng != CK2Title::finalLevel(TitleLevel::Kingdom); ++kng) {
    statsMap[*kng].print();
  }

  Logger::logStream(LogStream::Info) << "Done with statistics.\n"
                                     << LogOption::Undent;
  return true;
}

void Converter::statistics() {
  if (!ck2Game) {
    Logger::logStream(LogStream::Info) << "No file loaded.\n";
    return; 
  }

  loadFiles();
  if (!createCK2Objects()) return;
  if (!createEU4Objects()) return;
  if (!createProvinceMap()) return;
  if (!createCountryMap()) return;
  if (!calculateProvinceWeights()) return;

  displayStats();
}

void Converter::playerWars () {
  Logger::logStream(LogStream::Info) << "Player wars.\n";
  if (!createCK2Objects()) {
    return;
  }

  auto playerFilter = [](const CK2Ruler* ruler) -> bool {
    return ruler->isHuman();
  };

  for (auto war = CK2War::start(); war != CK2War::final(); ++war) {
    auto attackers = (*war)->getParticipants(CK2War::Attackers, playerFilter);
    if (attackers.empty()) {
      continue;
    }
    auto defenders = (*war)->getParticipants(CK2War::Defenders, playerFilter);
    if (defenders.empty()) {
      continue;
    }
    Logger::logStream(LogStream::Info) << (*war)->getName() << ":\n  Attackers: ";
    for (const auto* attacker : attackers) {
      Logger::logStream(LogStream::Info)
          << attacker->safeGetString("player_name", "Someone") << " ";
    }
    Logger::logStream(LogStream::Info) << "\n  Defenders: ";
    for (const auto* defender : defenders) {
      Logger::logStream(LogStream::Info)
          << defender->safeGetString("player_name", "Someone") << " ";
    }
    Logger::logStream(LogStream::Info) << "\n";
  }

  Logger::logStream(LogStream::Info) << "Done with player wars.\n";
}

void Converter::dynastyScores () {
  Logger::logStream(LogStream::Info) << "Dynastic scores.\n";
  if (!createCK2Objects()) {
    return;
  }
  string dirToUse = remQuotes(configObject->safeGetString("maps_dir", ".\\maps\\"));
  string overrideFileName =
      remQuotes(configObject->safeGetString("custom", QuotedNone));
  customObject = loadTextFile(dirToUse + overrideFileName);
  if (!customObject) {
    Logger::logStream(LogStream::Warn)
        << "Couldn't find custom object, no dynastic scores.\n";
    return;
  }

  calculateDynasticScores();
}

Object* Converter::loadTextFile (string fname) {
  Logger::logStream(LogStream::Info) << "Parsing file " << fname << "\n";
  ifstream reader;
  reader.open(fname.c_str());
  if ((reader.eof()) || (reader.fail())) {
    Logger::logStream(LogStream::Error) << "Could not open file, returning null object.\n";
    return 0; 
  }
  
  Object* ret = processFile(fname);
  Logger::logStream(LogStream::Info) << " ... done.\n";
  return ret; 
}

bool Converter::swapKeys (Object* one, Object* two, string key) {
  if (one == two) return true; 
  Object* valOne = one->safeGetObject(key);
  if (!valOne) return false;
  Object* valTwo = two->safeGetObject(key);
  if (!valTwo) return false;

  one->unsetValue(key);
  two->unsetValue(key);
  one->setValue(valTwo);
  two->setValue(valOne);
  return true; 
}

void Converter::checkProvinces () {
  Logger::logStream(LogStream::Info) << "Checking provinces.\n";
  if (!createCK2Objects()) {
    return;
  }
  for (CK2Province::Iter ckprov = CK2Province::start();
       ckprov != CK2Province::final(); ++ckprov) {
    string baronytag =
        remQuotes((*ckprov)->safeGetString("primary_settlement", QuotedNone));
    if (baronytag == "---") {
      continue; // Indicates wasteland.
    }

    if (QuotedNone == baronytag) {
      Logger::logStream(LogStream::Warn)
          << "Could not find primary settlement of " << nameAndNumber(*ckprov)
          << ", ignoring.\n";
      continue;
    }

    std::unordered_map<string, int> baronies;
    objvec leaves = (*ckprov)->getLeaves();
    for (auto* leaf : leaves) {
      if (leaf->isLeaf()) {
        continue;
      }
      string baronyType = leaf->safeGetString("type", PlainNone);
      if (PlainNone == baronyType) {
        continue;
      }
      baronies[baronyType]++;
      if (leaf->getKey() == baronytag && baronyType != "castle") {
        Logger::logStream(LogStream::Info)
            << "Province " << nameAndNumber(*ckprov)
            << " has primary settlement of type " << baronyType << "\n";
      }
    }
    if (baronies["castle"] != 1 || baronies["temple"] != 1 || baronies["city"] != 1) {
      Logger::logStream(LogStream::Info)
          << "Province " << nameAndNumber(*ckprov) << " has settlements ";
      for (const auto &barony : baronies) {
        Logger::logStream(LogStream::Info) << barony.first << "=" << barony.second << " ";
      }
      Logger::logStream(LogStream::Info) << "\n";
    }
  }
}

void detectChangedString(const string& old_string, const string& new_string,
                         const objvec& objects, string* target) {
  *target = new_string;
  for (auto* obj : objects) {
    Object* newObject = obj->safeGetObject(new_string);
    Object* oldObject = obj->safeGetObject(old_string);
    if (!newObject && !oldObject)
      continue;
    if (oldObject) {
      Logger::logStream(LogStream::Info)
          << "Detected old keyword '" << old_string
          << "', proceeding with that.\n";
      *target = old_string;
      return;
    }
    if (newObject) {
      return;
    }
  }
  Logger::logStream(LogStream::Warn)
      << "Reached end of detectChangedString for " << old_string << " vs "
      << new_string << ", may indicate bug.\n";
}

/********************************  End helpers  **********************/

/******************************** Begin initialisers **********************/

bool Converter::createCK2Objects () {
  Logger::logStream(LogStream::Info) << "Creating CK2 objects\n" << LogOption::Indent;
  gameDate = remQuotes(ck2Game->safeGetString("date", "\"1444.11.10\""));
  gameDays = days(gameDate);
  if (gameDays == 0) {
    Logger::logStream(LogStream::Warn)
        << "Problem with game date: \"" << gameDate << "\".\n";
  }

  Object* wrapperObject = ck2Game->safeGetObject("provinces");
  if (!wrapperObject) {
    Logger::logStream(LogStream::Error)
        << "Could not find provinces object, cannot continue.\n"
        << LogOption::Undent;
    return false;
  }

  objvec provinces = wrapperObject->getLeaves();
  detectChangedString(kOldTradePost, kNewTradePost, provinces, &tradePostString);
  for (objiter province = provinces.begin(); province != provinces.end();
       ++province) {
    new CK2Province(*province);
  }
  Logger::logStream(LogStream::Info)
      << "Created " << CK2Province::totalAmount() << " CK2 provinces.\n";

  wrapperObject = ck2Game->safeGetObject("title");
  if (!wrapperObject) {
    Logger::logStream(LogStream::Error)
        << "Could not find title object, cannot continue.\n"
        << LogOption::Undent;
    return false;
  }
  objvec titles = wrapperObject->getLeaves();
  for (objiter title = titles.begin(); title != titles.end(); ++title) {
    new CK2Title(*title);
  }

  Logger::logStream(LogStream::Info) << "Created " << CK2Title::totalAmount() << " CK2 titles.\n";
  // De-jure lieges
  map<string, string> deJureMap;
  if (deJureObject) {
    objvec dejures = deJureObject->getLeaves();
    for (objiter dejure = dejures.begin(); dejure != dejures.end(); ++dejure) {
      deJureMap[(*dejure)->getKey()] = (*dejure)->getLeaf();
    }
  }
  for (CK2Title::Iter ckCountry = CK2Title::start(); ckCountry != CK2Title::final(); ++ckCountry) {
    if ((*ckCountry)->safeGetString("landless", "no") == "yes") continue;
    Object* deJureObj = (*ckCountry)->safeGetObject("de_jure_liege");
    string deJureTag = (*ckCountry)->safeGetString("de_jure_liege", PlainNone);
    if (deJureObj && deJureTag == PlainNone)
      deJureTag = remQuotes(deJureObj->safeGetString("title", QuotedNone));
    if ((deJureTag == PlainNone) && (deJureMap.count((*ckCountry)->getName()))) {
      deJureTag = deJureMap[(*ckCountry)->getName()];
    }
    if (deJureTag == PlainNone) {
      continue;
    }
    (*ckCountry)->setDeJureLiege(CK2Title::findByName(deJureTag));
  }
  Logger::logStream(LogStream::Info) << "Set de-jure lieges.\n";

  Object* characters = ck2Game->safeGetObject("character");
  if (!characters) {
    Logger::logStream(LogStream::Error)
        << "Could not find character object, cannot continue.\n"
        << LogOption::Undent;
    return false;
  }

  objvec charObjs = characters->getLeaves();
  detectChangedString(kOldDemesne, kNewDemesne, charObjs, &demesneString);
  detectChangedString(kOldBirthName, kNewBirthName, charObjs, &birthNameString);
  detectChangedString(kOldBirthDate, kNewBirthDate, charObjs, &birthDateString);
  detectChangedString(kOldDynasty, kNewDynasty, charObjs, &dynastyString);
  detectChangedString(kOldAttributes, kNewAttributes, charObjs,
                      &attributesString);
  detectChangedString(kOldPrestige, kNewPrestige, charObjs, &prestigeString);
  detectChangedString(kOldJobTitle, kNewJobTitle, charObjs, &jobTitleString);
  detectChangedString(kOldEmployer, kNewEmployer, charObjs, &employerString);
  detectChangedString(kOldFather, kNewFather, charObjs, &fatherString);
  detectChangedString(kOldMother, kNewMother, charObjs, &motherString);
  detectChangedString(kOldFemale, kNewFemale, charObjs, &femaleString);
  detectChangedString(kOldGovernment, kNewGovernment, charObjs,
                      &governmentString);
  detectChangedString(kOldDeadCharHoldings, kNewDeadCharHoldings, charObjs,
                      &deadCharHoldingsString);
  detectChangedString(kOldTraits, kNewTraits, charObjs, &traitString);

  Logger::logStream(LogStream::Info)
      << "Using keywords: " << demesneString << ", " << birthNameString << ", "
      << birthDateString << ", " << dynastyString << ", " << attributesString
      << ", " << prestigeString << ", " << jobTitleString << ", "
      << employerString << ", " << fatherString << ", " << motherString << ", "
      << femaleString << ", " << governmentString << ", "
      << deadCharHoldingsString << ", " << traitString << "\n";

  Object* dynasties = ck2Game->safeGetObject("dynasties");
  // Map from heir to ruler, not the other way around.
  std::unordered_map<std::string, std::string> heirMap;
  
  for (auto* ckCountry : CK2Title::getAll()) {
    string holderId = ckCountry->safeGetString("holder", PlainNone);
    if (PlainNone == holderId) continue;
    CK2Ruler* ruler = CK2Ruler::findByName(holderId);
    if (!ruler) {
      Object* character = characters->safeGetObject(holderId);
      if (!character) {
	Logger::logStream(LogStream::Warn) << "Could not find character " << holderId
					   << ", holder of title " << ckCountry->getKey()
					   << ". Ignoring.\n";
	continue;
      }
      std::string heirTag = character->safeGetString("dh", PlainNone);
      if (heirTag != PlainNone) {
        heirMap[heirTag] = holderId;
      }
      ruler = new CK2Ruler(character, dynasties);
      if (CK2Ruler::totalAmount() % 100 == 0) {
        Logger::logStream("characters")
            << "Processed " << CK2Ruler::totalAmount() << " rulers.\n";
      }
    }

    ruler->addTitle(ckCountry);
  }

  objvec heirOverrides = customObject->getNeededObject("heir_overrides")->getLeaves();
  for (const auto* over : heirOverrides) {
    heirMap[over->getLeaf()] = over->getKey();
  }

  Logger::logStream(LogStream::Info)
      << "Created " << CK2Ruler::totalAmount() << " CK2 rulers.\n";
  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    (*ruler)->createLiege();
  }

  Logger::logStream(LogStream::Info) << "Counting baronies\n";
  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    (*ruler)->countBaronies();
    (*ruler)->createClaims();
  }

  map<string, int> dynastyPower;
  objvec allChars = characters->getLeaves();
  Logger::logStream(LogStream::Info) << "Calculating dynasty power\n";
  for (objiter ch = allChars.begin(); ch != allChars.end(); ++ch) {
    std::string charTag = (*ch)->getKey();
    if ((*ch)->safeGetString("d_d", PlainNone) != PlainNone) {
      // Dead character, check for dynasty power.
      string dynastyId = (*ch)->safeGetString(dynastyString, PlainNone);
      if (PlainNone == dynastyId) continue;
      objvec holdings = (*ch)->getValue(deadCharHoldingsString);
      for (objiter holding = holdings.begin(); holding != holdings.end(); ++holding) {
	CK2Title* title = CK2Title::findByName(remQuotes((*holding)->getLeaf()));
	if (!title) continue;
	dynastyPower[dynastyId] += (1 + *title->getLevel());
      }
      continue;
    }

    objvec claims = (*ch)->getValue("claim");
    CK2Ruler* father = CK2Ruler::findByName((*ch)->safeGetString(fatherString));
    CK2Ruler* mother = CK2Ruler::findByName((*ch)->safeGetString(motherString));
    CK2Ruler* employer = nullptr;
    if (((*ch)->safeGetString(jobTitleString, PlainNone) != PlainNone) ||
	((*ch)->safeGetString("title", PlainNone) == "\"title_commander\"") ||
	((*ch)->safeGetString("title", PlainNone) == "\"title_high_admiral\"")) {
      employer = CK2Ruler::findByName((*ch)->safeGetString(employerString));
    }
    CK2Character* current = CK2Ruler::findByName(charTag);
    if (heirMap.find(charTag) != heirMap.end()) {
      if (current == nullptr) {
        current = new CK2Character((*ch), dynasties);
      }
      CK2Ruler* ruler = CK2Ruler::findByName(heirMap[charTag]);
      if (ruler != nullptr) {
        Logger::logStream("characters")
            << "Overriding: " << nameAndNumber(current, birthNameString)
            << " is heir of " << nameAndNumber(ruler, birthNameString) << "\n";
        ruler->overrideHeir(current);
      }
    }
    for (auto* sp : (*ch)->getValue("spouse")) {
      CK2Ruler* spouse = CK2Ruler::findByName(sp->getLeaf());
      if (!spouse) {
        continue;
      }
      if (current == nullptr) {
        current = new CK2Character((*ch), dynasties);
      }
      spouse->addSpouse(current);
    }
    if ((!father) && (!mother) && (!employer) && (0 == claims.size())) continue;
    if (current == nullptr) {
      current = new CK2Character((*ch), dynasties);
    }
    if (father) father->personOfInterest(current);
    if (mother) mother->personOfInterest(current);
    if (employer) employer->personOfInterest(current);
    current->createClaims();
  }

  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    string dynastyId = (*ruler)->safeGetString(dynastyString, PlainNone);
    if (PlainNone == dynastyId) continue;
    (*ruler)->resetLeaf(kDynastyPower, dynastyPower[dynastyId]);
  }
  
  // Diplomacy
  Logger::logStream(LogStream::Info) << "Creating relations\n";
  Object* relationObject = ck2Game->getNeededObject("relation");
  objvec relations = relationObject->getLeaves();
  for (objiter relation = relations.begin(); relation != relations.end(); ++relation) {
    string key = (*relation)->getKey();
    string rel_type = getField(key, 0, '_');
    if (rel_type != "diplo") continue;
    string mainCharTag = getField(key, 1, '_');
    CK2Ruler* mainChar = CK2Ruler::findByName(mainCharTag);
    if (!mainChar) {
      // Indicates a courtier.
      continue;
    }
    objvec subjects = (*relation)->getLeaves();
    for (objiter subject = subjects.begin(); subject != subjects.end(); ++subject) {
      CK2Ruler* otherChar = CK2Ruler::findByName((*subject)->getKey());
      if (!otherChar) continue;
      if ((*subject)->safeGetString("tributary", PlainNone) == mainCharTag) {
	mainChar->addTributary(otherChar);
      }
    }
  }
  
  objvec wars = ck2Game->getValue("active_war");
  for (objiter war = wars.begin(); war != wars.end(); ++war) {
    new CK2War(*war);
  }
  Logger::logStream(LogStream::Info) << "Created " << CK2War::totalAmount() << " CK2 wars.\n";
  
  Logger::logStream(LogStream::Info) << "Done with CK2 objects.\n" << LogOption::Undent;
  return true;
}

bool Converter::createEU4Objects () {
  Logger::logStream(LogStream::Info) << "Creating EU4 objects\n" << LogOption::Indent;
  Object* wrapperObject = eu4Game->safeGetObject("provinces");
  if (!wrapperObject) {
    Logger::logStream(LogStream::Error)
        << "Could not find EU4 provinces object, cannot continue.\n"
        << LogOption::Undent;
    return false;
  }

  objvec provinces = wrapperObject->getLeaves();
  for (objiter province = provinces.begin(); province != provinces.end(); ++province) {
    // For some reason, the province numbers are negative in the key, but nowhere else?!
    string provid = (*province)->getKey();
    if (provid[0] == '-') (*province)->setKey(provid.substr(1));
    new EU4Province(*province);
  }
  Logger::logStream(LogStream::Info) << "Created " << EU4Province::totalAmount() << " provinces.\n";

  objvec areas = eu4_areas->getLeaves();
  for (auto* area : areas) {
    for (int i = 0; i < area->numTokens(); ++i) {
      string provinceTag = area->getToken(i);
      auto* province = EU4Province::findByName(provinceTag);
      if (!province) {
        Logger::logStream("provinces")
            << "Could not find province " << provinceTag << ", part of area "
            << area->getKey() << "\n";
        continue;
      }
      province->resetLeaf(kStateKey, area->getKey());
      area_province_map[area->getKey()].insert(province);
    }
  }

  wrapperObject = eu4Game->safeGetObject("countries");
  if (!wrapperObject) {
    Logger::logStream(LogStream::Error)
        << "Could not find EU4 countries object, cannot continue.\n"
        << LogOption::Undent;
    return false;
  }

  objvec countries = wrapperObject->getLeaves();
  for (objiter country = countries.begin(); country != countries.end(); ++country) {
    EU4Country* eu4country = new EU4Country(*country);
    Object* powers = eu4country->safeGetObject("powers");
    if (!powers) continue;
    int totalPower = 0;
    for (int i = 0; i < powers->numTokens(); ++i) totalPower += powers->tokenAsInt(i);
    eu4country->resetLeaf(kAwesomePower, totalPower);
  }
  Logger::logStream(LogStream::Info) << "Created " << EU4Country::totalAmount() << " countries.\n";
 
  Logger::logStream(LogStream::Info) << "Done with EU4 objects.\n" << LogOption::Undent;
  return true;
}

void convertTitle(CK2Title* title, CK2Ruler* ruler,
                  map<EU4Country*, vector<EU4Province*>>& initialProvincesMap,
                  set<EU4Country*>& candidateCountries) {
  if (title->getEU4Country()) {
    return;
  }
  EU4Country* bestCandidate = nullptr;
  vector<pair<string, string> > bestOverlapList;
  for (auto* cand : candidateCountries) {
    vector<EU4Province*> eu4Provinces = initialProvincesMap[cand];
    vector<pair<string, string> > currOverlapList;
    for (auto* eu4Province : eu4Provinces) {
      for (auto* ck2Prov : eu4Province->ckProvs()) {
	CK2Title* countyTitle = ck2Prov->getCountyTitle();
	if (title->isDeJureOverlordOf(countyTitle)) {
          currOverlapList.emplace_back(countyTitle->getName(),
                                       nameAndNumber(eu4Province));
        }
      }
    }
    if ((0 < bestOverlapList.size()) &&
        (currOverlapList.size() <= bestOverlapList.size()))
      continue;
    bestCandidate = cand;
    bestOverlapList = currOverlapList;
  }
  if (3 > bestOverlapList.size()) {
    for (auto* cand : candidateCountries) {
      vector<EU4Province*> eu4Provinces = initialProvincesMap[cand];
      vector<pair<string, string> > currOverlapList;
      for (auto* eu4Province : eu4Provinces) {
        for (auto* ck2Prov : eu4Province->ckProvs()) {
          CK2Title* countyTitle = ck2Prov->getCountyTitle();
          if (ruler->hasTitle(countyTitle, true)) {
            currOverlapList.emplace_back(countyTitle->getName(),
                                         nameAndNumber(eu4Province));
          }
	}
      }
      if ((0 < bestOverlapList.size()) &&
          (currOverlapList.size() <= bestOverlapList.size())) {
        continue;
      }
      bestCandidate = cand;
      bestOverlapList = currOverlapList;
    }
  }

  if (bestCandidate == nullptr) {
    Logger::logStream(LogStream::Warn)
        << "Could not find conversion candidate for " << title->getName()
        << ", skipping.\n";
    return;
  }
  Logger::logStream("countries")
      << nameAndNumber(ruler) << " as ruler of " << title->getName()
      << " converts to " << bestCandidate->getKey() << " based on overlap "
      << bestOverlapList.size() << " with these counties: ";
  for (auto& overlap : bestOverlapList) {
    Logger::logStream("countries")
        << overlap.first << " -> " << overlap.second << " ";
  }
  Logger::logStream("countries") << "\n";
  bestCandidate->setRuler(ruler, title);
  candidateCountries.erase(bestCandidate);
}

bool Converter::createCountryMap () {
  if (!countryMapObject) {
    Logger::logStream(LogStream::Error) << "Error: Could not find country-mapping object.\n";
    return false; 
  }

  set<EU4Country*> forbiddenCountries;
  Logger::logStream(LogStream::Info) << "Beginning country mapping.\n" << LogOption::Indent;
  objvec overrides = countryMapObject->getLeaves();
  for (objiter override = overrides.begin(); override != overrides.end(); ++override) {
    string cktag = (*override)->getKey();
    string eutag = (*override)->getLeaf();
    CK2Title* ckCountry = CK2Title::findByName(cktag);
    if ((!ckCountry) || (!ckCountry->getRuler())) {
      Logger::logStream(LogStream::Warn)
          << "Attempted override " << cktag << " -> " << eutag
          << ", but could not find CK title. Ignoring.\n";
      continue;
    }
    EU4Country* euCountry = EU4Country::findByName(eutag);
    if (!euCountry) {
      Logger::logStream(LogStream::Warn)
          << "Attempted override " << cktag << " -> " << eutag
          << ", but could not find EU country. Ignoring.\n";
      continue;
    }
    if (ckCountry->getRuler()->getPrimaryTitle()->getTag() != cktag) {
      Logger::logStream(LogStream::Warn)
          << "Attempted override " << cktag << " -> " << eutag << ", but "
          << cktag << " is not primary title. Ignoring.\n";
      continue;
    }

    euCountry->setRuler(ckCountry->getRuler(), ckCountry);
    forbiddenCountries.insert(euCountry);
    Logger::logStream(LogStream::Info) << "Converting "
				       << euCountry->getName() << " from "
				       << nameAndNumber(ckCountry->getRuler())
				       << " due to custom override on "
				       << ckCountry->getName()
				       << ".\n";
  }

  // Candidate countries are: All countries that have provinces with
  // CK equivalent and none without; plus all countries with no provinces,
  // but which have discovered one of a configurable list of provinces.
  set<EU4Country*> candidateCountries;
  map<EU4Country*, vector<EU4Province*> > initialProvincesMap;
  for (EU4Province::Iter eu4 = EU4Province::start(); eu4 != EU4Province::final(); ++eu4) {
    string eu4tag = remQuotes((*eu4)->safeGetString("owner", QuotedNone));
    if (PlainNone == eu4tag) continue;
    EU4Country* owner = EU4Country::findByName(eu4tag);
    initialProvincesMap[owner].push_back(*eu4);
  }

  for (map<EU4Country*, vector<EU4Province*>>::iterator eu4 =
           initialProvincesMap.begin();
       eu4 != initialProvincesMap.end(); ++eu4) {
    if (forbiddenCountries.count((*eu4).first))
      continue;
    string badProvinceTag = "";
    bool hasGoodProvince = false;
    for (vector<EU4Province*>::iterator prov = (*eu4).second.begin();
         prov != (*eu4).second.end(); ++prov) {
      if (0 < (*prov)->numCKProvinces())
        hasGoodProvince = true;
      else {
        badProvinceTag = nameAndNumber(*prov);
        break;
      }
    }
    if (badProvinceTag != "") {
      Logger::logStream("countries")
          << "Disregarding " << (*eu4).first->getKey() << " because it owns "
          << badProvinceTag << "\n";
      forbiddenCountries.insert((*eu4).first);
    } else if (hasGoodProvince) {
      candidateCountries.insert((*eu4).first);
    }
  }

  Object* provsObject = customObject->safeGetObject("provinces_for_tags");
  EU4Province::Container provsToKnow;
  if (provsObject) {
    for (int i = 0; i < provsObject->numTokens(); ++i) {
      provsToKnow.push_back(EU4Province::findByName(provsObject->getToken(i)));
    }
  }
  if (3 > provsToKnow.size()) {
    provsToKnow.push_back(EU4Province::findByName("1"));
    provsToKnow.push_back(EU4Province::findByName("112"));
    provsToKnow.push_back(EU4Province::findByName("213"));
  }

  for (EU4Province::Iter known = provsToKnow.begin(); known != provsToKnow.end(); ++known) {
    Object* discovered = (*known)->getNeededObject("discovered_by");
    for (int i = 0; i < discovered->numTokens(); ++i) {
      string eutag = discovered->getToken(i);
      EU4Country* eu4country = EU4Country::findByName(eutag);
      if (!eu4country) {
	Logger::logStream(LogStream::Warn) << "Could not find EU4 country "
					   << eutag
					   << ", wanted for having discovered "
					   << nameAndNumber(*known)
					   << ".\n";
	continue;
      }
      if (forbiddenCountries.count(eu4country)) continue;
      candidateCountries.insert(eu4country);
    }
  }

  Logger::logStream("countries") << "Found these EU4 countries:";
  for (set<EU4Country*>::iterator eu4 = candidateCountries.begin(); eu4 != candidateCountries.end(); ++eu4) {
    Logger::logStream("countries") << " " << (*eu4)->getName();
  }
  Logger::logStream("countries") << "\n";
  if (12 > candidateCountries.size()) {
    Logger::logStream(LogStream::Error)
        << "Found " << candidateCountries.size()
        << " EU4 countries; cannot continue with so few.\n"
        << LogOption::Undent;
    return false;
  }

  int maxUnions = configObject->safeGetInt("max_unions", 2);
  map<CK2Ruler*, int> unions;
  int maxSmallVassals = configObject->safeGetInt("max_vassals_per_kingdom", 3);
  bool subInfeudate = (configObject->safeGetString("permit_subinfeudation") == "yes");
  map<CK2Ruler*, map<CK2Title*, int> > totalVassals;

  map<CK2Title*, int> priorities;
  const int kIgnore = -1000;
  const int kPrimary = 1000;
  const int kSovereign = 500;
  const std::map<TitleLevel const* const, int> kLevelPriorities = {
      {TitleLevel::Empire, 900},
      {TitleLevel::Kingdom, 700},
      {TitleLevel::Duchy, 500},
      {TitleLevel::County, -1400},
      {TitleLevel::Barony, kIgnore}};

  auto title_priority = [&](CK2Title* one, CK2Title* two) -> bool {
    for (auto* current : {one, two}) {
      if (priorities.find(current) != priorities.end()) {
        continue;
      }
      if (current->getLevel() == TitleLevel::Barony) {
        priorities[current] = kIgnore;
        continue;
      }
      Logger::logStream("countries")
          << "Calculating priority for " << current->getName() << "\n"
          << LogOption::Indent;
      auto* ruler = current->getRuler();
      if (!ruler) {
        Logger::logStream("countries") << "No ruler: " << kIgnore << "\n"
                                       << LogOption::Undent;
        priorities[current] = kIgnore;
        continue;
      }
      int score = 0;
      if (ruler->getPrimaryTitle() == current) {
        Logger::logStream("countries") << "Primary title: " << kPrimary << "\n";
        score += kPrimary;
      }
      if (ruler->isSovereign()) {
        Logger::logStream("countries")
            << nameAndNumber(ruler) << " is sovereign: " << kSovereign << "\n";
        score += kSovereign;
      }
      Logger::logStream("countries")
          << current->getLevel()->getName() << ": "
          << kLevelPriorities.at(current->getLevel()) << "\n";
      score += kLevelPriorities.at(current->getLevel());
      Logger::logStream("countries") << "Total: " << score << "\n"
                                     << LogOption::Undent;
      priorities[current] = score;
    }

    // Greater-than for descending order.
    return priorities[one] > priorities[two];
  };

  std::vector<CK2Title*> sortedTitles;
  for (auto* title : CK2Title::getAll()) {
    if (title->getLevel() == TitleLevel::Barony) {
      continue;
    }
    sortedTitles.push_back(title);
  }
  std::sort(sortedTitles.begin(), sortedTitles.end(), title_priority);
  for (auto* title : sortedTitles) {
    if (priorities[title] < 0) {
      Logger::logStream("countries") << "No more positive-priority titles\n";
      break;
    }
    if (candidateCountries.empty()) {
      Logger::logStream("countries") << "No more EU4 countries\n";
      break;
    }
    if (title->getEU4Country()) {
      continue;
    }
    Logger::logStream("countries") << "Converting " << title->getName() << "\n"
                                   << LogOption::Indent;
    auto* ruler = title->getRuler();
    if (!ruler) {
      Logger::logStream("countries") << "Skipping due to no ruler\n"
                                     << LogOption::Undent;
      continue;
    }
    Logger::logStream("countries")
        << "Ruler is " << nameAndNumber(ruler) << "\n";
    if (0 == ruler->countBaronies()) {
      // Rebels, adventurers, and suchlike riffraff.
      Logger::logStream("countries")
          << "Skipping " << nameAndNumber(ruler) << " due to zero baronies.\n"
          << LogOption::Undent;
      continue;
    }

    CK2Title* primary = ruler->getPrimaryTitle();
    if (!primary) {
      // Should not happen.
      Logger::logStream("countries")
          << "Skipping due to no primary title.\n " << LogOption::Undent;
      continue;
    }

    if (ruler->isSovereign()) {
      if (primary == title) {
        Logger::logStream("countries") << "Converting as primary\n"
                                       << LogOption::Undent;
        convertTitle(title, ruler, initialProvincesMap, candidateCountries);
        continue;
      } else {
        if (title->getEU4Country()) {
          Logger::logStream("countries")
              << "Already converted to " << title->getEU4Country()->getKey()
              << "\n"
              << LogOption::Undent;
          continue;
        }
        if (unions[ruler] >= maxUnions) {
          Logger::logStream("countries")
              << "Secondary title, but over union limit.\n"
              << LogOption::Undent;
          continue;
        }
        Logger::logStream("countries") << "Converting as union.\n"
                                       << LogOption::Undent;
        convertTitle(title, ruler, initialProvincesMap, candidateCountries);
        unions[ruler]++;
        continue;
      }
    }
    // Is a vassal.
    if (primary->getEU4Country()) {
      Logger::logStream("countries")
          << "Secondary title of vassal, primary " << primary->getName()
          << " already converted to " << primary->getEU4Country()->getKey()
          << ".\n"
          << LogOption::Undent;
      continue;
    }
    CK2Ruler* sovereign = primary->getSovereign();
    if (!sovereign) {
      Logger::logStream("countries")
          << "Could not find sovereign for " << nameAndNumber(ruler) << " of "
          << primary->getName() << "\n"
          << LogOption::Undent;
      continue;
    }
    CK2Title* kingdom = primary->getDeJureLevel(TitleLevel::Kingdom);
    if (kingdom == nullptr) {
      Logger::logStream("countries")
          << "Could not find de-jure kingdom for " << primary->getName()
          << ", using primary title instead.\n";
      kingdom = primary;
    }
    CK2Ruler* immediateLiege = ruler->getLiege();
    if (immediateLiege == nullptr) {
      Logger::logStream(LogStream::Warn)
          << "Something weird about " << primary->getName()
          << ", it is a vassal but has no liege. Skipping.\n"
          << LogOption::Undent;
      continue;
    }
    if ((!immediateLiege->isSovereign()) && (!subInfeudate)) {
      Logger::logStream("countries")
          << "Skipping " << primary->getName() << " to avoid subinfeudation.\n"
          << LogOption::Undent;
      continue;
    }
    bool notTooMany = (totalVassals[sovereign][kingdom] < maxSmallVassals);
    bool important = *(primary->getLevel()) >= *(TitleLevel::Kingdom);
    if (notTooMany || important) {
      Logger::logStream("countries")
          << "Converting " << primary->getName() << " as vassal, "
          << (notTooMany ? (nameAndNumber(sovereign) + " not over limit in " +
                            (kingdom ? kingdom->getName() : PlainNone))
                         : string("too big to ignore"))
          << ".\n";
      convertTitle(primary, ruler, initialProvincesMap, candidateCountries);
      totalVassals[sovereign][kingdom]++;
    } else {
      Logger::logStream("countries")
          << "Skipping " << primary->getName() << ", too small.\n";
    }
    Logger::logStream("countries") << LogOption::Undent;
  }

  Logger::logStream(LogStream::Info) << "Done with country mapping.\n" << LogOption::Undent;
  return true; 
}

bool Converter::createProvinceMap() {
  if (!provinceMapObject) {
    Logger::logStream(LogStream::Error)
        << "Error: Could not find province-mapping object.\n";
    return false;
  }

  Logger::logStream(LogStream::Info) << "Starting province mapping\n"
                                     << LogOption::Indent;
  for (auto* ckprov : CK2Province::getAll()) {
    Object* province_info = ck_province_setup->safeGetObject(ckprov->getKey());
    if (province_info == nullptr) {
      Logger::logStream(LogStream::Warn)
          << "Could not find province information for " << nameAndNumber(ckprov)
          << "\n";
      continue;
    }
    string countytag = province_info->safeGetString("title", PlainNone);
    if (countytag == PlainNone) {
      Logger::logStream(LogStream::Warn)
          << "Could not find title information for " << nameAndNumber(ckprov)
          << "\n";
      continue;
    }
    CK2Title* county = CK2Title::findByName(countytag);
    if (county == nullptr) {
      Logger::logStream(LogStream::Warn)
          << "Could not find county " << countytag << " for province "
          << nameAndNumber(ckprov) << ".\n";
      continue;
    }
    ckprov->setCountyTitle(county);

    objvec conversions = provinceMapObject->getValue(countytag);
    if (0 == conversions.size()) {
      Logger::logStream(LogStream::Warn)
          << "Could not find EU4 equivalent for province "
          << nameAndNumber(ckprov) << ", ignoring.\n";
      continue;
    }

    for (objiter conversion = conversions.begin();
         conversion != conversions.end(); ++conversion) {
      string eu4id = (*conversion)->getLeaf();
      EU4Province* target = EU4Province::findByName(eu4id);
      if (!target) {
        Logger::logStream(LogStream::Warn)
            << "Could not find EU4 province " << eu4id
            << ", (missing DLC in input save?), skipping "
            << nameAndNumber(ckprov) << ".\n";
        continue;
      }
      ckprov->assignProvince(target);
      Logger::logStream("provinces")
          << nameAndNumber(ckprov) << " mapped to EU4 province "
          << nameAndNumber(target) << ".\n";
    }
  }

  Logger::logStream(LogStream::Info) << "Done with province mapping.\n" << LogOption::Undent;
  return true; 
}

void Converter::loadFiles () {
  string dirToUse = remQuotes(configObject->safeGetString("maps_dir", ".\\maps\\"));
  Logger::logStream(LogStream::Info) << "Directory: \"" << dirToUse << "\"\n" << LogOption::Indent;

  Parser::ignoreString = "EU4txt";
  Parser::specialCases["map_area_data{"] = "map_area_data={";
  eu4Game = loadTextFile(dirToUse + "input.eu4");
  Parser::specialCases.clear();
  Parser::ignoreString = "";
  provinceMapObject = loadTextFile(dirToUse + "provinces.txt");
  deJureObject = loadTextFile(dirToUse + "de_jure_lieges.txt");
  ckBuildingObject = loadTextFile(dirToUse + "ck_buildings.txt");
  ckBuildingWeights = loadTextFile(dirToUse + "ck_building_weights.txt");
  euBuildingObject = loadTextFile(dirToUse + "eu_buildings.txt");
  Object* ckTraitObject = loadTextFile(dirToUse + "ck_traits.txt");
  CK2Character::ckTraits = ckTraitObject->getLeaves();
  Object* euTraitObject = loadTextFile(dirToUse + "eu_ruler_traits.txt");
  CK2Character::euRulerTraits = euTraitObject->getLeaves();
  euLeaderTraits = loadTextFile(dirToUse + "eu_leader_traits.txt");
  advisorTypesObject = loadTextFile(dirToUse + "advisors.txt");
  if (!advisorTypesObject) {
    Logger::logStream(LogStream::Warn)
        << "Warning: Did not find advisors.txt. Proceeding without advisor "
           "types info. No advisors will be created.\n";
    advisorTypesObject = new Object("dummy_advisors");
  }

  eu4_areas = loadTextFile(dirToUse + "areas.txt");
  ck_province_setup = loadTextFile(dirToUse + "ck_province_titles.txt");
  string overrideFileName = remQuotes(configObject->safeGetString("custom", QuotedNone));
  if ((PlainNone != overrideFileName) && (overrideFileName != "NOCUSTOM")) {
    customObject = loadTextFile(dirToUse + overrideFileName);
  } else {
    customObject = new Object("custom");
  }
  countryMapObject = customObject->getNeededObject("country_overrides");
  setDynastyNames(loadTextFile(dirToUse + "dynasties.txt"));

  if (eu4Game->safeGetObject("provinces") == nullptr) {
    Logger::logStream(LogStream::Warn)
        << "Couldn't find EU4 provinces object, this will cause errors later.\n";
  }
  Logger::logStream(LogStream::Info) << "Done loading input files\n" << LogOption::Undent;
}

void Converter::setDynastyNames (Object* dynastyNames) {
  if (!dynastyNames) {
    Logger::logStream(LogStream::Warn) << "Warning: Did not find "
                                          "dynasties.txt. Many famous "
                                          "dynasties will be nameless.\n";
    return;
  }

  Object* gameDynasties = ck2Game->safeGetObject("dynasties");
  if (!gameDynasties) {
    Logger::logStream(LogStream::Warn) << "Warning: Could not find dynasties object. Proceeding with foreboding.\n";
    return;
  }

  objvec dynasties = gameDynasties->getLeaves();
  for (objiter dyn = dynasties.begin(); dyn != dynasties.end(); ++dyn) {
    if ((*dyn)->safeGetString("name", PlainNone) != PlainNone) continue;
    Object* outsideDynasty = dynastyNames->safeGetObject((*dyn)->getKey());
    if (!outsideDynasty) {
      Logger::logStream("characters")
          << "Could not find dynasty information for nameless dynasty "
          << (*dyn)->getKey() << ".\n";
      continue;
    }

    (*dyn)->setLeaf("name", outsideDynasty->safeGetString("name"));
  }
}

/******************************* End initialisers *******************************/ 


/******************************* Begin conversions ********************************/

double getVassalPercentage(pair<EU4Country* const, vector<EU4Country*>>& lord,
                           map<EU4Country*, int>& ownerMap) {
  double total = 0;
  if (ownerMap.find(lord.first) != ownerMap.end()) {
    total = ownerMap.at(lord.first);
  } else {
    Logger::logStream(LogStream::Warn)
        << "Could not find overlord " << lord.first->getName() << " in owner map.\n";
  }

  double vassals = 0;
  for (auto* subject : lord.second) {
    double curr = ownerMap[subject];
    total += curr;
    vassals += curr;
  }
  if (0 == total) {
    return 0;
  }
  return vassals / total;
}

void constructOwnerMap (map<EU4Country*, int>* ownerMap) {
  for (auto* eu4prov : EU4Province::getAll()) {
    if (0 == eu4prov->numCKProvinces()) continue;
    EU4Country* owner = EU4Country::getByName(remQuotes(eu4prov->safeGetString("owner")));
    if (!owner) continue;
    (*ownerMap)[owner]++;
  }
}

bool Converter::adjustBalkanisation () {
  Logger::logStream(LogStream::Info) << "Adjusting balkanisation.\n" << LogOption::Indent;

  map<EU4Country*, vector<EU4Country*> > subjectMap;
  Object* diplomacy = eu4Game->getNeededObject("diplomacy");
  objvec relations = diplomacy->getLeaves();
  std::unordered_set<std::string> relations_to_blob = {"dependency", "union"};
  for (auto* relation : relations) {
    if (relations_to_blob.count(relation->getKey()) == 0) {
      continue;
    }
    EU4Country* overlord = EU4Country::getByName(
        remQuotes(relation->safeGetString("first", QuotedNone)));
    if (!overlord) {
      continue;
    }
    EU4Country* subject = EU4Country::getByName(
        remQuotes(relation->safeGetString("second", QuotedNone)));
    if (!subject) {
      continue;
    }
    subjectMap[overlord].push_back(subject);
  }

  map<EU4Country*, int> ownerMap;
  constructOwnerMap(&ownerMap);

  double maxBalkan = configObject->safeGetFloat("max_balkanisation", 0.40);
  double minBalkan = configObject->safeGetFloat("min_balkanisation", 0.30);
  int balkanThreshold = configObject->safeGetInt("balkan_threshold");
  for (auto& lord : subjectMap) {
    if (lord.second.empty()) continue;
    EU4Country* overlord = lord.first;
    double vassalPercentage = getVassalPercentage(lord, ownerMap);
    bool dent = false;
    bool printed = false;
    while (vassalPercentage < minBalkan) {
      if (ownerMap[overlord] <= balkanThreshold) {
        Logger::logStream("countries")
            << ownerMap[overlord] << " provinces are too few to balkanise.\n";
        break;
      }
      if (!printed) {
        Logger::logStream("countries")
            << "Vassal percentage " << vassalPercentage << ", balkanising "
            << overlord->getKey() << ".\n"
            << LogOption::Indent;
        printed = true;
        dent = true;
      }

      set<EU4Country*> attempted;
      bool success = false;
      while (attempted.size() < lord.second.size()) {
	EU4Country* target = lord.second[0];
	for (auto* subject : lord.second) {
	  if (attempted.count(subject)) continue;
	  if (ownerMap[subject] >= ownerMap[target]) continue;
	  target = subject;
	}
	if (attempted.count(target)) {
	  Logger::logStream("countries") << "Could not find good target for balkanisation.\n";
	  break;
	}
	attempted.insert(target);
	map<CK2Title*, int> kingdoms;
	for (EU4Province::Iter eu4prov = target->startProvince(); eu4prov != target->finalProvince(); ++eu4prov) {
	  for (CK2Province::Iter ck2prov = (*eu4prov)->startProv(); ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
	    CK2Title* countyTitle = (*ck2prov)->getCountyTitle();
	    if (!countyTitle) continue;
	    CK2Title* kingdom = countyTitle->getDeJureLevel(TitleLevel::Kingdom);
	    if (!kingdom) continue;
	    kingdoms[kingdom]++;
	  }
	}
	vector<CK2Title*> targetKingdoms;
	for (map<CK2Title*, int>::iterator kingdom = kingdoms.begin(); kingdom != kingdoms.end(); ++kingdom) {
	  kingdom->first->resetLeaf("vassal_provinces", kingdom->second);
	  targetKingdoms.push_back(kingdom->first);
	}
	if (0 == targetKingdoms.size()) {
	  Logger::logStream("countries") << "No CK kingdoms for balkanisation.\n";
	  break;
	}
	sort(targetKingdoms.begin(), targetKingdoms.end(), ObjectDescendingSorter("vassal_provinces"));
	for (auto* kingdom : targetKingdoms) {
	  for (auto* eu4prov : overlord->getProvinces()) {
	    bool acceptable = false;
	    for (auto* ck2prov : eu4prov->ckProvs()) {
	      CK2Title* countyTitle = ck2prov->getCountyTitle();
	      if (!countyTitle) continue;
	      CK2Title* curr = countyTitle->getDeJureLevel(TitleLevel::Kingdom);
	      if (curr != kingdom) continue;
	      acceptable = true;
	      break;
	    }
	    if (!acceptable) continue;
	    Logger::logStream("countries") << "Reassigned " << eu4prov->getName() << " to "
					   << target->getKey() << "\n";
	    eu4prov->assignCountry(target);
	    target->setAsCore(eu4prov);
	    overlord->removeCore(eu4prov);
	    ownerMap[target]++;
	    ownerMap[overlord]--;
	    success = true;
	    break;
	  }
	  if (success) break;
	}
	if (success) break;
      }

      if (!success) {
	Logger::logStream("countries") << "Giving up on balkanising " << overlord->getKey() << "\n";
	break;
      }      
      vassalPercentage = getVassalPercentage(lord, ownerMap);
    }

    while (vassalPercentage > maxBalkan) {
      if (!printed) {
        Logger::logStream("countries")
            << "Vassal percentage " << vassalPercentage << ", reblobbing "
            << overlord->getKey() << ".\n"
            << LogOption::Indent;
        dent = true;
        printed = true;
      }

      set<EU4Country*> attempted;
      bool success = false;

      while (attempted.size() < lord.second.size()) {
	EU4Country* target = lord.second[0];
	for (auto* subject : lord.second) {
	  if (attempted.count(subject)) continue;
	  if (0 == ownerMap[subject]) {
	    attempted.insert(subject);
	    continue;
	  }
	  if ((ownerMap[target] > 0) && (ownerMap[subject] >= ownerMap[target])) continue;
	  target = subject;
	}
	if (attempted.count(target)) break;
	attempted.insert(target);
	if (0 == ownerMap[target]) continue;
	EU4Province::Iter iter = target->startProvince();
	EU4Province* province = *iter;
        if (!province) {
          Logger::logStream(LogStream::Warn)
              << "Cannot find provinces of " << target->getKey()
              << ", skipping\n";
          continue;
        }
	if (province->getKey() == target->safeGetString("capital")) {
	  ++iter;
	  if (iter != target->finalProvince()) province = *iter;
	}

	Logger::logStream("countries") << "Reabsorbed " << province->getName()
                                       << " " << (int) province
				       << " from " << target->getName()
				       << ".\n";
	province->assignCountry(overlord);
	overlord->setAsCore(province);
	target->removeCore(province);

	ownerMap[overlord]++;
	ownerMap[target]--;
        if (ownerMap[target] == 0) {
          independenceRevolts.insert(target);
        }
	success = true;
	break;
      }

      if (!success) {
	Logger::logStream("countries") << "Giving up on reblobbing " << overlord->getKey() << "\n";
	break;
      }
      vassalPercentage = getVassalPercentage(lord, ownerMap);
    }
    if (dent) {
      Logger::logStream("countries")
          << "Final vassal percentage " << vassalPercentage << ": ("
          << overlord->getName();
      for (EU4Province::Iter prov = overlord->startProvince();
           prov != overlord->finalProvince(); ++prov) {
        Logger::logStream("countries") << " " << (*prov)->getName();
      }
      Logger::logStream("countries") << ") ";
      for (auto* subject : lord.second) {
        Logger::logStream("countries") << "(" << subject->getName();
        for (EU4Province::Iter prov = subject->startProvince();
             prov != subject->finalProvince(); ++prov) {
          Logger::logStream("countries") << " " << (*prov)->getName();
        }
        Logger::logStream("countries") << ") ";
      }

      Logger::logStream("countries") << "\n" << LogOption::Undent;
    }
  }

  Logger::logStream(LogStream::Info) << "Done with balkanisation.\n" << LogOption::Undent;
  return true;
}

struct DynastyScore {
  DynastyScore() = default;
  explicit DynastyScore(string n, int cheevos)
      : name(n), achievements(cheevos), members(0), cached_score(-1),
        adjusted_score(0), unadjusted_score(0) {}
  DynastyScore(const DynastyScore& other) = default;
  string name;
  int achievements;
  int members;
  int cached_score;
  int adjusted_score;
  int unadjusted_score;
  map<string, int> trait_counts;
  vector<pair<CK2Title*, int> > title_days;
  map<const TitleLevel*, int> current_titles;

  static int BaronyPoints;
  static int CountyPoints;
  static int DuchyPoints;
  static int KingdomPoints;
  static int EmpirePoints;

  void score () {
    cached_score = 0;
    for (const auto& held : title_days) {
      CK2Title* title = held.first;
      int days = held.second;
      int mult = 0;
      if (title->getLevel() == TitleLevel::Barony) {
        mult = BaronyPoints;
      } else if (title->getLevel() == TitleLevel::County) {
        mult = CountyPoints;
      } else if (title->getLevel() == TitleLevel::Duchy) {
        mult = DuchyPoints;
      } else if (title->getLevel() == TitleLevel::Kingdom) {
        mult = KingdomPoints;
      } else if (title->getLevel() == TitleLevel::Empire) {
        mult = EmpirePoints;
      }
      cached_score += days * mult;
    }
  }

  string score_string (Object* custom_score_traits, double median, double target) {
    string ret;
    struct levelInfo {
      string titles;
      double total_days = 0;
    };
    levelInfo counties;
    levelInfo duchies;
    levelInfo kingdoms;
    levelInfo empires;
    unordered_map<CK2Title*, int> total_days;
    for (const auto& held : title_days) {
      total_days[held.first] += held.second;
    }

    for (const auto& held : total_days) {
      CK2Title* title = held.first;
      int days = held.second;
      levelInfo* target = nullptr;
      if (title->getLevel() == TitleLevel::County) {
        target = &counties;
      } else if (title->getLevel() == TitleLevel::Duchy) {
        target = &duchies;
      } else if (title->getLevel() == TitleLevel::Kingdom) {
        target = &kingdoms;
      } else if (title->getLevel() == TitleLevel::Empire) {
        target = &empires;
      }
      if (target) {
        sprintf(strbuffer, "%s %i ", title->getKey().c_str(), days);
        target->titles += strbuffer;
        target->total_days += days;
      }
    }
    for (const auto& level : {counties, duchies, kingdoms, empires}) {
      if (level.titles.empty()) {
        continue;
      }
      sprintf(strbuffer, "%sTotal years: %.2f\n", level.titles.c_str(),
              level.total_days / 365);
      ret += strbuffer;
    }
    double actual_score = cached_score;
    if (actual_score < median) {
      sprintf(strbuffer, "Adjust to median: %.2f\n", median);
      ret += strbuffer;
      actual_score = median;
    }

    string trait_line;
    double total_trait_bonus = 0;
    for (const auto& trait : trait_counts) {
      string trait_name = trait.first;
      double bonus = custom_score_traits->safeGetFloat(trait_name);
      if (abs(bonus) < 0.001) {
        continue;
      }
      int number = trait.second;
      double fraction = number;
      fraction /= members;
      double trait_bonus = fraction * bonus * median;
      sprintf(strbuffer, "%s: %i (%.2f) -> %.1f ", trait_name.c_str(), number,
              fraction, trait_bonus);
      total_trait_bonus += trait_bonus;
      trait_line += strbuffer;
    }
    sprintf(strbuffer, "Total from traits: %.1f\n", total_trait_bonus);
    trait_line += strbuffer;
    ret += trait_line;
    actual_score += total_trait_bonus;

    double achievement_bonus =
        median * achievements *
        custom_score_traits->safeGetFloat("achievements", 0.01);
    sprintf(strbuffer, "Achievement bonus: %.1f\n", achievement_bonus);
    ret += strbuffer;


    double final_points = actual_score + achievement_bonus;
    double cached_final = cached_score + total_trait_bonus + achievement_bonus;
    sprintf(strbuffer, "Adjusted total: %.1f\n", final_points);
    ret += strbuffer;
    final_points /= median;
    final_points *= target;
    cached_final /= median;
    cached_final *= target;
    adjusted_score = (int)floor(final_points + 0.5);
    unadjusted_score = (int)floor(cached_final + 0.5);
    for (const auto& tc : current_titles) {
      sprintf(strbuffer, "%s : %i\n", tc.first->getName().c_str(), tc.second);
      ret += strbuffer;
    }
    return ret;
  }
};

int DynastyScore::BaronyPoints = 0;
int DynastyScore::CountyPoints = 1;
int DynastyScore::DuchyPoints = 3;
int DynastyScore::KingdomPoints = 5;
int DynastyScore::EmpirePoints = 7;

bool operator<(const DynastyScore& left, const DynastyScore& right) {
  return left.cached_score < right.cached_score;
}

void handleEvent(Object* event, CK2Title* title, int startDays, int endDays,
                 unordered_map<string, Object*>& characters,
                 unordered_map<string, DynastyScore>& dynasties) {
  if (!event) {
    return;
  }
  if (title == nullptr) {
    // This should never happen.
    Logger::logStream("characters") << "Warning: Null title passed to handleEvent.\n";
    return;
  }

  Object* holder = event->safeGetObject("holder");
  if (!holder) {
    return;
  }
  string characterId = holder->safeGetString("who", PlainNone);
  Object* character = characters[characterId];
  if (!character) {
    return;
  }
  string dynastyKey = character->safeGetString(dynastyString, PlainNone);
  if (dynastyKey == PlainNone) {
    return;
  }
  auto scoreObject = dynasties.find(dynastyKey);
  if (scoreObject == dynasties.end()) {
    Logger::logStream("characters")
        << "Did not find score object for dynasty " << dynastyKey << "\n";
    return;
  }

  int titleDays = endDays - startDays;
  if (holder->safeGetString("type") == "created") {
    Logger::logStream("characters")
        << nameAndNumber(character, birthNameString) << " of dynasty "
        << dynasties[dynastyKey].name << " created " << title->getKey()
        << ".\n";
    titleDays += 3650;
  }
  double years = titleDays;
  years /= 365;
  Logger::logStream("characters")
      << nameAndNumber(character, birthNameString) << " of dynasty "
      << dynasties[dynastyKey].name << " held " << title->getKey() << " for "
      << years << " years.\n";
  scoreObject->second.title_days.emplace_back(title, titleDays);
  if (endDays == gameDays) {
    scoreObject->second.current_titles[title->getLevel()]++;
  }
}

void Converter::calculateDynasticScores() {
  auto customs = customObject->getNeededObject("custom_score")->getLeaves();
  if (customs.size() == 0) {
    return;
  }
  Logger::logStream(LogStream::Info) << "Beginning dynasty score calculation.\n"
                                     << LogOption::Indent;

  auto* titlePoints = customObject->getNeededObject("custom_score_title_points");
  DynastyScore::BaronyPoints = titlePoints->safeGetInt("baron", 0);
  DynastyScore::CountyPoints = titlePoints->safeGetInt("count", 1);
  DynastyScore::DuchyPoints = titlePoints->safeGetInt("duke", 3);
  DynastyScore::KingdomPoints = titlePoints->safeGetInt("king", 5);
  DynastyScore::EmpirePoints = titlePoints->safeGetInt("emperor", 7);

  if (CK2Character::ckTraits.empty()) {
    string dirToUse = remQuotes(configObject->safeGetString("maps_dir", ".\\maps\\"));
    Object* ckTraitObject = loadTextFile(dirToUse + "ck_traits.txt");
    CK2Character::ckTraits = ckTraitObject->getLeaves();
  }
  unordered_map<int, string> trait_number_to_name;
  for (int idx = 0; idx < (int) CK2Character::ckTraits.size(); ++idx) {
    trait_number_to_name[idx+1] = CK2Character::ckTraits[idx]->getKey();
  }

  unordered_map<string, DynastyScore> dynasties;
  auto* dynastyObject = ck2Game->getNeededObject("dynasties");
  for (auto* custom : customs) {
    string index = custom->getKey();
    Object* ckDynasty = dynastyObject->safeGetObject(index);
    if (!ckDynasty) {
      Logger::logStream(LogStream::Warn)
          << "Could not find dynasty " << index << "\n";
      continue;
    }
    dynasties.emplace(piecewise_construct, forward_as_tuple(index),
                      forward_as_tuple(ckDynasty->safeGetString("name"),
                                       atoi(custom->getLeaf().c_str())));
  }

  Logger::logStream("characters") << "Starting character iteration.\n";
  unordered_map<string, Object*> characters;
  Object* score_traits = customObject->getNeededObject("custom_score_traits");
  for (auto* character : ck2Game->getNeededObject("character")->getLeaves()) {
    string dIndex = character->safeGetString(dynastyString, PlainNone);
    if (dIndex == PlainNone || dynasties.find(dIndex) == dynasties.end()) {
      continue;
    }
    dynasties[dIndex].members++;
    characters[character->getKey()] = character;
    Object* traits = character->safeGetObject(traitString);
    if (traits) {
      for (int i = 0; i < traits->numTokens(); ++i) {
        int trait_number = traits->tokenAsInt(i);
        string trait_name = trait_number_to_name[trait_number];
        double bonus = score_traits->safeGetFloat(trait_name);
        if (abs(bonus) < 0.001) {
          continue;
        }
        dynasties[dIndex].trait_counts[trait_name]++;
      }
    }
  }
  Logger::logStream("characters")
      << "Found " << characters.size() << " characters of interest.\n";
  if (characters.size() == 0) {
    return;
  }

  string startDate = remQuotes(ck2Game->safeGetString("start_date", "\"769.1.1\""));
  int startDays = days(startDate);
  if (startDays == 0) {
    Logger::logStream(LogStream::Warn)
        << "Problem with start date: \"" << startDate << "\".\n";
  }

  for (auto* title : CK2Title::getAll()) {
    if (title->safeGetString("landless") == "yes") {
      continue;
    }
    auto history = title->getNeededObject("history")->getLeaves();
    int previousDays = startDays;
    Object* previousEvent = nullptr;
    for (auto* event : history) {
      int start = days(event->getKey());
      if (start == 0) {
        Logger::logStream(LogStream::Warn)
            << "Problem with date " << event->getKey()
            << ", ignoring event in history of title " << title->getKey()
            << "\n";
        continue;
      }
      if (start < startDays) {
        continue;
      }
      handleEvent(previousEvent, title, previousDays, start, characters, dynasties);
      previousEvent = event;
      previousDays = start;
    }
    handleEvent(previousEvent, title, previousDays, gameDays, characters, dynasties);
  }

  vector<DynastyScore> sorted_dynasties;
  for (auto& dynasty : dynasties) {
    dynasty.second.score();
    sorted_dynasties.emplace_back(dynasty.second);
  }
  sort(sorted_dynasties.begin(), sorted_dynasties.end());
  if (sorted_dynasties.empty()) {
    return;
  }

  int size = sorted_dynasties.size();
  double median = 0.5 *
                  (sorted_dynasties[size / 2].cached_score +
                   sorted_dynasties[(size - 1) / 2].cached_score);
  double target = configObject->safeGetFloat("median_mana", 1000);
  for (auto& score : sorted_dynasties) {
    Logger::logStream(LogStream::Info)
        << score.name << " : " << score.cached_score << "\n"
        << LogOption::Indent << score.score_string(score_traits, median, target)
        << "\n"
        << LogOption::Undent;
  }

  Logger::logStream(LogStream::Info) << "\nMana:\n";
  for (const auto& score : sorted_dynasties) {
    Logger::logStream(LogStream::Info)
        << score.name << " : " << score.adjusted_score << " ("
        << score.unadjusted_score << ")\n";
  }

  Logger::logStream(LogStream::Info) << "Done with dynasty scores.\n" << LogOption::Undent;
}

// Returns the total weight of the fields in building, according to the entries
// in weights.
double getTotalWeight(Object* building, Object* weights) {
  if (!weights) {
    return 1;
  }
  objvec fieldWeights = weights->getLeaves();
  double weight = 0;
  for (auto* fw : fieldWeights) {
    double curr = building->safeGetFloat(fw->getKey());
    curr *= fw->getLeafAsFloat();
    weight += curr;
  }
  return weight;
}

void calculateBuildingWeights(objvec& buildingTypes, Object* weights) {
  if (10 > buildingTypes.size()) {
    Logger::logStream(LogStream::Warn)
        << "Only found " << buildingTypes.size()
        << " types of buildings. Proceeding, but dubiously.\n";
  }
  for (auto p = ProvinceWeight::start(); p != ProvinceWeight::final(); ++p) {
    std::string areaName = (*p)->getName();
    Object* currWeights = weights->safeGetObject(areaName);
    Object* linear = 0;
    if (currWeights == 0) {
      Logger::logStream(LogStream::Error)
          << "Could not find " << areaName
          << " in CK building weights, Bad Things will happen.\n";
    } else {
      linear = currWeights->safeGetObject("linear");
    }
    if (linear == 0) {
      Logger::logStream(LogStream::Error)
          << "Could not find linear " << areaName
          << " modifiers in CK building weights, Bad Things will happen.\n";
    }
    Object* mult = currWeights = currWeights->getNeededObject("mult");
    for (auto* bt : buildingTypes) {
      double weight = getTotalWeight(bt, linear);
      bt->setLeaf(areaName, weight);
      double totalMult = getTotalWeight(bt, mult);
      bt->setLeaf(areaName + "_mult", totalMult);
      Logger::logStream("buildings")
          << "Set " << areaName << " of " << bt->getKey() << " to " << weight
          << " and " << totalMult << "\n";
    }
  }
}

bool Converter::calculateProvinceWeights () {
  Logger::logStream(LogStream::Info) << "Beginning province weight calculations.\n" << LogOption::Indent;


  map<string, CK2Province*> patricianToCapitalMap;
  // Add merchant-family houses to provinces.
  for (CK2Title::Iter title = CK2Title::start(); title != CK2Title::final(); ++title) {
    if (TitleLevel::Barony != (*title)->getLevel()) continue;
    Object* settlement = (*title)->safeGetObject("settlement");
    if (!settlement) continue;
    if (settlement->safeGetString("type") != "family_palace") continue;
    CK2Title* liegeTitle = (*title)->getLiege();
    if (!liegeTitle) {
      continue;
    }
    CK2Ruler* liege = liegeTitle->getRuler();
    Object* demesne = liege->safeGetObject(demesneString);
    if (!demesne) continue;
    string capTag = remQuotes(demesne->safeGetString("capital", QuotedNone));
    CK2Province* capital = CK2Province::getFromBarony(capTag);
    if (!capital) continue;
    capital->addBarony(settlement);
    Logger::logStream("provinces") << "Assigning family house "
				   << (*title)->getKey()
				   << " to province "
				   << nameAndNumber(capital)
				   << ".\n";
    string holder = (*title)->safeGetString("holder", PlainNone);
    if (holder != PlainNone) patricianToCapitalMap[holder] = capital;
  }

  for (CK2Province::Iter ck2prov = CK2Province::start(); ck2prov != CK2Province::final(); ++ck2prov) {
    Object* tradepost = (*ck2prov)->safeGetObject(tradePostString);
    if (!tradepost) continue;
    string owner = tradepost->safeGetString("owner", PlainNone);
    if (owner == PlainNone) continue;
    CK2Province* capital = patricianToCapitalMap[owner];
    if (!capital) continue;
    Logger::logStream("provinces") << "Trade post in "
				   << nameAndNumber(*ck2prov)
				   << " assigned to capital "
				   << nameAndNumber(capital)
				   << ".\n";
    capital->resetLeaf("realm_tradeposts", 1 + capital->safeGetInt("realm_tradeposts"));
  }
  
  if (!ckBuildingObject) {
    Logger::logStream(LogStream::Error) << "Cannot proceed without building object.\n";
    return false;
  }
  if (!ckBuildingWeights) {
    Logger::logStream(LogStream::Error) << "Cannot proceed without building weights.\n";
    return false;
  }

  objvec buildingTypes = ckBuildingObject->getLeaves();
  calculateBuildingWeights(buildingTypes, ckBuildingWeights);

  Object* minWeights = configObject->getNeededObject("minimumWeights");
  minWeights->setValue(customObject->getNeededObject("special_nerfs"));
  for (auto* ck2prov : CK2Province::getAll()) {
    ck2prov->calculateWeights(minWeights, buildingTypes);
    Logger::logStream("provinces") << nameAndNumber(ck2prov)
				   << " has weights production "
				   << ck2prov->getWeight(ProvinceWeight::Production)
				   << ", taxation "
				   << ck2prov->getWeight(ProvinceWeight::Taxation)
				   << ", manpower "
				   << ck2prov->getWeight(ProvinceWeight::Manpower)
				   << ".\n";
  }

  // Initialise EU4Country iterator over baronies.
  for (auto* ck2prov : CK2Province::getAll()) {
    for (objiter barony = ck2prov->startBarony();
         barony != ck2prov->finalBarony(); ++barony) {
      CK2Title* baronyTitle = CK2Title::findByName((*barony)->getKey());
      if (!baronyTitle) continue;
      CK2Ruler* ruler = baronyTitle->getRuler();
      if (!ruler) {
        Logger::logStream("provinces")
            << "Could not find ruler for " << baronyTitle->getKey()
            << ", skipping.\n";
        continue;
      }
      while (!ruler->getEU4Country()) {
	ruler = ruler->getLiege();
	if (!ruler) break;
      }
      if (!ruler) continue;
      ruler->getEU4Country()->addBarony(*barony);
    }
  }

  Logger::logStream(LogStream::Info) << "Done with province weight calculations.\n" << LogOption::Undent;
  return true;
}

void cleanStates(Object* map_area_data) {
  if (!map_area_data) {
    return;
  }
  objvec areas = map_area_data->getLeaves();
  for (auto* area : areas) {
    Object* state = area->safeGetObject("state");
    if (!state) {
      continue;
    }
    objvec country_states = state->getValue("country_state");
    objvec to_remove;
    for (auto* cs : country_states) {
      std::string tag = remQuotes(cs->safeGetString("country", QuotedNone));
      EU4Country* country = EU4Country::findByName(tag);
      if (country->converts()) {
        to_remove.push_back(cs);
      }
    }
    for (auto* tr : to_remove) {
      state->removeObject(tr);
      delete tr;
    }
  }
}

bool Converter::cleanEU4Nations () {
  Logger::logStream(LogStream::Info) << "Beginning nation cleanup.\n" << LogOption::Indent;
  Object* keysToClear = configObject->getNeededObject("keys_to_clear");
  keysToClear->addToList("owned_provinces");
  keysToClear->addToList("controlled_provinces");
  keysToClear->addToList("core_provinces");

  Object* keysToRemove = configObject->getNeededObject("keys_to_remove");
  Object* zeroProvKeys = configObject->getNeededObject("keys_to_remove_on_zero_provs");

  map<EU4Country*, int> ownerMap;
  constructOwnerMap(&ownerMap);

  Object* diplomacy = eu4Game->getNeededObject("diplomacy");
  Object* active_advisors = eu4Game->getNeededObject("active_advisors");
  vector<string> tags_to_clean;
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    if (!(*eu4country)->converts()) continue;
    for (int i = 0; i < keysToClear->numTokens(); ++i) {
      Object* toClear = (*eu4country)->getNeededObject(keysToClear->getToken(i));
      toClear->clear();
    }
    for (int i = 0; i < keysToRemove->numTokens(); ++i) {
      (*eu4country)->unsetValue(keysToRemove->getToken(i));
    }

    if (0 < ownerMap[*eu4country]) continue;
    string eu4tag = (*eu4country)->getKey();
    Logger::logStream("countries")
        << eu4tag << " has no provinces, removing diplomacy.\n";
    tags_to_clean.push_back(eu4tag);
    (*eu4country)->resetLeaf(EU4Country::kNoProvinceMarker, "yes");
    active_advisors->unsetValue(eu4tag);
    for (int i = 0; i < zeroProvKeys->numTokens(); ++i) {
      (*eu4country)->unsetValue(zeroProvKeys->getToken(i));
    }

    objvec dipsToRemove;
    objvec dipObjects = diplomacy->getLeaves();
    for (objiter dip = dipObjects.begin(); dip != dipObjects.end(); ++dip) {
      string first = remQuotes((*dip)->safeGetString("first"));
      string second = (*dip)->safeGetString("second");
      if ((first != eu4tag) && (remQuotes(second) != eu4tag)) continue;
      Logger::logStream(LogStream::Info)
          << "Removing " << (*dip) << " due to " << first << ", " << second
          << " vs " << eu4tag << "\n";
      dipsToRemove.push_back(*dip);
      EU4Country* overlord = EU4Country::getByName(first);
      overlord->getNeededObject("friends")->remToken(second);
      overlord->getNeededObject("subjects")->remToken(second);
    }
    for (objiter rem = dipsToRemove.begin(); rem != dipsToRemove.end(); ++rem) {
      diplomacy->removeObject(*rem);
    }
  }

  for (auto* eu4prov : EU4Province::getAll()) {
    if (!eu4prov->converts()) continue;
    EU4Country* owner = EU4Country::getByName(remQuotes(eu4prov->safeGetString("owner")));
    if (owner) {
      owner->getNeededObject("owned_provinces")->addToList(eu4prov->getKey());
      owner->getNeededObject("controlled_provinces")
          ->addToList(eu4prov->getKey());
    }
    auto* cores = eu4prov->safeGetObject("cores");
    for (int i = 0; i < cores->numTokens(); ++i) {
      EU4Country* coreHaver = EU4Country::getByName(remQuotes(cores->getToken(i)));
      if (coreHaver) coreHaver->setAsCore(eu4prov);
    }
  }

  Object* trade = eu4Game->getNeededObject("trade");
  objvec nodes = trade->getValue("node");
  for (auto* node : nodes) {
    for (const auto& tag : tags_to_clean) {
      node->unsetValue(tag);
    }
    node->unsetValue("top_provinces");
    node->unsetValue("top_provinces_values");
    node->unsetValue("top_power");
    node->unsetValue("top_power_values");
  }
  for (EU4Country::Iter eu4country = EU4Country::start();
       eu4country != EU4Country::final(); ++eu4country) {
    Object* relations = (*eu4country)->safeGetObject("active_relations");
    if (!relations) {
      continue;
    }
    for (const auto& tag : tags_to_clean) {
      relations->unsetValue(tag);
    }
  }

  // Ensure that nations such as Portugal which own both converting and
  // non-converting provinces have their lists set correctly.
  unordered_map<string, vector<string>> provinceOwnership;
  for (EU4Province::Iter eu4prov = EU4Province::start(); eu4prov != EU4Province::final(); ++eu4prov) {
    if ((*eu4prov)->converts()) continue;
    string eu4Tag = remQuotes((*eu4prov)->safeGetString("owner", QuotedNone));
    if (eu4Tag == PlainNone) continue;
    provinceOwnership[eu4Tag].push_back((*eu4prov)->getKey());
  }

  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    if ((*eu4country)->converts()) continue;
    Object* owned_provs = (*eu4country)->safeGetObject("owned_provinces");
    if (owned_provs == nullptr) continue;
    string eu4tag = (*eu4country)->getKey();
    if (owned_provs->numTokens() == (int)provinceOwnership[eu4tag].size()) {
      continue;
    }
    Logger::logStream("countries")
        << "Nonconverted country " << eu4tag << " had " << owned_provs;
    owned_provs->clear();
    for (const auto& prov : provinceOwnership[eu4tag]) {
      owned_provs->addToList(prov);
    }
    Logger::logStream("countries") << "  changed to " << owned_provs << "\n";
    string capital = (*eu4country)->safeGetString("capital", "NoneSuch");
    if (find(provinceOwnership[eu4tag].begin(), provinceOwnership[eu4tag].end(),
             capital) == provinceOwnership[eu4tag].end()) {
      if (provinceOwnership[eu4tag].empty()) {
        Logger::logStream("countries")
            << eu4tag << " owns no provinces, not moving capital\n";
        continue;
      }
      string newCapital = provinceOwnership[eu4tag][0];
      (*eu4country)->resetLeaf("capital", newCapital);
      (*eu4country)->resetLeaf("original_capital", newCapital);
      (*eu4country)->resetLeaf("trade_port", newCapital);
      Logger::logStream("countries")
          << "  moved capital from " << capital << " to " << newCapital << "\n";
    }
  }

  eu4Game->unsetValue("trade_league");
  Object* map_area_data = eu4Game->safeGetObject("map_area_data");
  cleanStates(map_area_data);

  Logger::logStream(LogStream::Info) << "Done with nation cleanup.\n" << LogOption::Undent;
  Logger::logStream(LogStream::Info) << "Diplomacy object: " << diplomacy << "\n";
  return true;
}

// Returns a date add_years into the converted game.
string Converter::getConversionDate(int add_years) {
  string eu4Date = eu4Game->safeGetString("date", "1444.11.11");
  int eu4Year = 0;
  int eu4Month = 0;
  int eu4Day = 0;
  yearMonthDay(eu4Date, eu4Year, eu4Month, eu4Day);
  sprintf(strbuffer, "%i.%i.%i", eu4Year + add_years, eu4Month, eu4Day);
  return strbuffer;
}

// Returns the date of 'ck2Date' assuming that the CK save year converts to the
// EU4 save year; so if the conversion is from 1000 to 1100, and the date is in
// 950, 1050 is returned.
string Converter::getConversionDate(const string& ck2Date) {
  string eu4Date = eu4Game->safeGetString("date", "1444.11.11");
  int eu4Year = year(eu4Date);
  string conversionDate =
      remQuotes(ck2Game->safeGetString("date", "1444.11.11"));
  int conversionYear = year(conversionDate);

  int ck2Year = 0;
  int ck2Month = 0;
  int ck2Day = 0;
  yearMonthDay(ck2Date, ck2Year, ck2Month, ck2Day);

  ck2Year += (eu4Year - conversionYear);
  sprintf(strbuffer, "%i.%i.%i", ck2Year, ck2Month, ck2Day);
  return strbuffer;
}

double Converter::calculateTroopWeight (Object* levy, Logger* logstream, int idx) {
  static Object* troopWeights = configObject->getNeededObject("troops");
  static objvec troopTypes = troopWeights->getLeaves();
  double ret = 0;
  for (objiter ttype = troopTypes.begin(); ttype != troopTypes.end(); ++ttype) {
    string key = (*ttype)->getKey();
    double amount = getLevyStrength(key, levy, idx);
    if (logstream) (*logstream) << key << ": " << amount << "\n";
    amount *= troopWeights->safeGetFloat(key);
    ret += amount;
  }
  return ret;
}

bool Converter::createArmies () {
  Logger::logStream(LogStream::Info) << "Beginning army creation.\n"
                                     << LogOption::Indent;
  objvec armyIds;
  objvec regimentIds;
  set<string> armyLocations;
  string armyType = "4713";
  string regimentType = "4713";
  double totalCkTroops = 0;
  int countries = 0;

  for (auto* eu4country : EU4Country::getAll()) {
    if (!eu4country) {
      Logger::logStream(LogStream::Error) << "Null EU4 country?!\n";
      continue;
    }
    if (eu4country->isROTW()) {
      continue;
    }
    objvec armies = eu4country->getValue("army");
    for (objiter army = armies.begin(); army != armies.end(); ++army) {
      Object* id = (*army)->safeGetObject("id");
      if (id) armyIds.push_back(id);
      string location = (*army)->safeGetString("location", "-1");
      if (location != "-1") armyLocations.insert(location);
      objvec regiments = (*army)->getValue("regiment");
      for (objiter regiment = regiments.begin(); regiment != regiments.end(); ++regiment) {
	Object* id = (*regiment)->safeGetObject("id");
	if (id) regimentIds.push_back(id);
	location = (*regiment)->safeGetString("home", "-1");
	if (location != "-1") armyLocations.insert(location);
      }
    }
    eu4country->unsetValue("army");

    Logger::logStream("armies") << eu4country->getKey() << " levies: \n" << LogOption::Indent;
    double ck2troops = 0;
    double maxLevies = 0;
    double realLevies = 0;
    for (auto* eu4prov : eu4country->getProvinces()) {
      Logger::logStream("armies") << eu4prov->safeGetString("name") << ":\n" << LogOption::Indent;
      double weighted = 0;
      for (CK2Province::Iter ck2prov = eu4prov->startProv(); ck2prov != eu4prov->finalProv(); ++ck2prov) {
	double weightFactor = 1.0 / (*ck2prov)->numEU4Provinces();
	objvec leaves = (*ck2prov)->getLeaves();
	for (objiter leaf = leaves.begin(); leaf != leaves.end(); ++leaf) {
	  Object* levy = (*leaf)->safeGetObject("levy");
	  if (!levy) continue;
	  string baronyTag = (*leaf)->getKey();
	  if (baronyTag == tradePostString) continue;
	  CK2Title* baronyTitle = CK2Title::findByName(baronyTag);
	  if (!baronyTitle) continue;
	  CK2Ruler* sovereign = baronyTitle->getSovereign();
	  Logger::logStream("armies") << baronyTag << ": ";
          if (sovereign != eu4country->getRuler()) {
            if ((sovereign) && (sovereign->getEU4Country())) {
              Logger::logStream("armies")
                  << "Ignoring, part of "
                  << sovereign->getEU4Country()->getKey() << ".\n";
            } else {
              Logger::logStream("armies") << "Ignoring, no sovereign.\n";
            }
            continue;
          }
          double distanceFactor =
              1.0 / (1 + baronyTitle->distanceToSovereign());
          Logger::logStream("armies") << "(" << distanceFactor << "):\n" << LogOption::Indent;
          double amount =
              calculateTroopWeight(levy, &(Logger::logStream("armies")));
          realLevies += calculateTroopWeight(levy, nullptr, 0);
          maxLevies += amount;
          Logger::logStream("armies") << LogOption::Undent;
          amount *= distanceFactor;
	  amount *= weightFactor;
	  ck2troops += amount;
	  weighted += amount;
	}
      }
      Logger::logStream("armies") << "Province total: " << weighted << "\n" << LogOption::Undent;
    }
    Logger::logStream("armies") << eu4country->getKey() << " has "
                                << ck2troops << " weighted levies.\n"
                                << LogOption::Undent;
    eu4country->resetLeaf("ck_troops", ck2troops);
    if (maxLevies > 0) {
      eu4country->resetLeaf("manpower_fraction", realLevies / maxLevies);
    }
    totalCkTroops += ck2troops;
    ++countries;
  }

  if (0 < regimentIds.size()) regimentType = regimentIds.back()->safeGetString("type", regimentType);
  if (0 < armyIds.size()) armyType = armyIds.back()->safeGetString("type", armyType);

  double totalRetinues = 0;
  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    EU4Country* eu4country = (*ruler)->getEU4Country();
    if (!eu4country) continue;
    Object* demesne = (*ruler)->getNeededObject(demesneString);
    objvec armies = demesne->getValue("army");
    double currRetinue = 0;
    for (objiter army = armies.begin(); army != armies.end(); ++army) {
      objvec units = (*army)->getValue("sub_unit");
      for (objiter unit = units.begin(); unit != units.end(); ++unit) {
	if (QuotedNone == (*unit)->safeGetString("retinue_type", QuotedNone)) continue;
	Object* troops = (*unit)->safeGetObject("troops");
	if (troops) currRetinue += calculateTroopWeight(troops, 0);
      }
    }
    totalRetinues += currRetinue;
    (*ruler)->resetLeaf("retinueWeight", currRetinue);
    Logger::logStream("armies") << nameAndNumber(*ruler)
				<< " has retinue weight "
				<< currRetinue
				<< ".\n";
  }

  double retinueWeight = configObject->safeGetFloat("retinue_weight", 3);
  totalCkTroops += retinueWeight * totalRetinues;

  if (0 == totalCkTroops) {
    Logger::logStream(LogStream::Warn) << "Warning: Found no CK troops. No armies will be created.\n" << LogOption::Undent;
    return true;
  }

  int numRegiments = regimentIds.size();
  double averageTroops = totalCkTroops / countries;
  Logger::logStream("armies") << "Total weighted CK troop strength "
			      << totalCkTroops
			      << ", EU4 regiments "
			      << numRegiments
			      << ".\n";

  configObject->setLeaf("regimentsPerTroop", numRegiments / totalCkTroops);
  string infantryType = configObject->safeGetString("infantry_type", "\"western_medieval_infantry\"");
  string cavalryType = configObject->safeGetString("cavalry_type", "\"western_medieval_knights\"");
  int infantryRegiments = configObject->safeGetInt("infantry_per_cavalry", 7);
  double makeAverage = max(0.0, configObject->safeGetFloat("make_average", 1));
  
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    if (!(*eu4country)) {
      continue;
    }
    if (!(*eu4country)->converts()) continue;
    CK2Ruler* ruler = (*eu4country)->getRuler();

    double currWeight = (*eu4country)->safeGetFloat("ck_troops");
    Logger::logStream("armies") << (*eu4country)->getKey() << " levy strength " << currWeight;
    if (ruler->getEU4Country() == (*eu4country)) {
      double retinue = ruler->safeGetFloat("retinueWeight") * retinueWeight;
      Logger::logStream("armies") << " plus retinue " << retinue;
      currWeight += retinue;
    }
    currWeight += averageTroops * makeAverage;
    currWeight /= (1 + makeAverage);
    Logger::logStream("armies") << " gives adjusted weight " << currWeight;    

    currWeight /= totalCkTroops;
    currWeight *= numRegiments;
    int regimentsToCreate = (int) floor(0.5 + currWeight);
    Logger::logStream("armies") << " and "
				<< regimentsToCreate
				<< " regiments.\n";
    (*eu4country)->unsetValue("ck_troops");

    if (0 == regimentsToCreate) continue;
    Object* army = (*eu4country)->getNeededObject("army");
    Object* armyId = 0;
    if (!armyIds.empty()) {
      armyId = armyIds.back();
      armyIds.pop_back();
    }
    else {
      armyId = createUnitId(armyType);
    }

    army->setValue(armyId);
    string name = remQuotes(ruler->safeGetString(birthNameString, "\"Nemo\""));
    army->setLeaf("name", addQuotes(string("Army of ") + name));
    string capitalTag = (*eu4country)->safeGetString("capital");
    string location = capitalTag;
    if (0 == armyLocations.count(capitalTag)) {
      for (EU4Province::Iter prov = (*eu4country)->startProvince(); prov != (*eu4country)->finalProvince(); ++prov) {
	string provTag = (*prov)->getKey();
	if (0 == armyLocations.count(provTag)) continue;
	location = provTag;
	break;
      }
    }

    army->setLeaf("location", location);
    EU4Province* eu4prov = EU4Province::findByName(location);
    Object* unit = new Object(armyId);
    unit->setKey("unit");
    eu4prov->setValue(unit);
    for (int i = 0; i < regimentsToCreate; ++i) {
      Object* regiment = new Object("regiment");
      army->setValue(regiment);
      Object* regimentId = 0;
      if (regimentIds.empty()) {
	regimentId = createUnitId(regimentType);
      }
      else {
	regimentId = regimentIds.back();
	regimentIds.pop_back();
      }
      regiment->setValue(regimentId);
      regiment->setLeaf("home", location);
      regiment->setLeaf("name", createString("\"Retinue Regiment %i\"", i+1));
      regiment->setLeaf("type", ((i + 1) % (infantryRegiments + 1) == 0 ? cavalryType : infantryType));
      regiment->setLeaf("strength", "1.000");
    }
  }

  Logger::logStream(LogStream::Info) << "Done with armies.\n" << LogOption::Undent;
  return true;
}

string getDynastyName (CK2Character* ruler) {
  static const string defaultDynasty = "\"Generic Dynasty\"";
  Object* ckDynasty = ruler->getDynasty();
  string dynastyNumber = ruler->safeGetString(dynastyString, PlainNone);
  if (!ckDynasty) {
    if (dynastyNumber != PlainNone) return dynastyNumber;
    return defaultDynasty;
  }
  return ckDynasty->safeGetString("name", dynastyNumber);
}

string getFullName (CK2Character* character) {
  string name = remQuotes(character->safeGetString(birthNameString, "Someone"));
  name += " ";
  name += remQuotes(getDynastyName(character));
  return addQuotes(name);
}

void addPersonality(objvec& personalities, CK2Character* ck2char, Object* eu4char) {
  Object* best_personality = nullptr;
  int best_score = 0;
  map<string, int> points;
  for (auto* eu_personality : personalities) {
    objvec ck_traits = eu_personality->getLeaves();
    int curr_score = 0;
    map<string, int> curr_points;
    for (auto* ck_trait : ck_traits) {
      string key = ck_trait->getKey();
      if (!ck2char->hasTrait(key)) {
        continue;
      }
      int score = eu_personality->safeGetInt(key);
      curr_score += score;
      curr_points[key] = score;
    }
    if (curr_score <= best_score) {
      continue;
    }
    best_personality = eu_personality;
    best_score = curr_score;
    points = curr_points;
  }
  if (best_personality != nullptr) {
    eu4char->getNeededObject("personalities")
        ->setLeaf(best_personality->getKey(), "yes");
    Logger::logStream("characters")
        << "Personality " << best_personality->getKey() << " from ";
    for (const auto& point : points) {
      Logger::logStream("characters")
          << "(" << point.first << ", " << point.second << ") ";
    }
    Logger::logStream("characters") << "\n";
  }
}

void setCultureAndReligion(const string& capitalTag, CK2Character* ruler,
                           Object* target) {
  string ckCulture = ruler->getBelief("culture");
  if (ckCulture == PlainNone) {
    auto* capital = EU4Province::getByName(capitalTag);
    if (capital) {
      target->setLeaf("culture", capital->safeGetString("culture"));
    }
  } else if (!cultureMap[ckCulture].empty()) {
    target->setLeaf("culture", cultureMap[ckCulture][0]);
  }
  string ckReligion = ruler->getBelief("religion");
  if (!religionMap[ckReligion].empty()) {
    target->setLeaf("religion", religionMap[ckReligion][0]);
  }
}

Object* Converter::makeMonarchObject(const string& capitalTag,
                                     CK2Character* ruler, const string& keyword,
                                     Object* bonusTraits) {
  Object* monarchDef = new Object(keyword);
  monarchDef->setLeaf("name", ruler->safeGetString(birthNameString, "\"Some Guy\""));
  map<string, map<string, double> > sources;
  sources["MIL"]["ck_martial"] = max(0, ruler->getAttribute(CKAttribute::Martial) / 5);
  sources["DIP"]["ck_diplomacy"] =
      max(0,
          (ruler->getAttribute(CKAttribute::Diplomacy) +
           ruler->getAttribute(CKAttribute::Intrigue)) /
              5);
  sources["ADM"]["ck_stewardship"] =
      max(0,
          (ruler->getAttribute(CKAttribute::Stewardship) +
           ruler->getAttribute(CKAttribute::Learning)) /
              5);
  for (auto& area : sources) {
    Object* bonus = bonusTraits->getNeededObject(area.first);
    for (auto* bon : bonus->getLeaves()) {
      string desc = bon->getKey();
      if (!ruler->hasTrait(desc)) {
        continue;
      }
      area.second[desc] = bonus->safeGetInt(desc);
    }
  }
  double age = ruler->getAge(gameDate);
  if (age < 16) {
    int ageAdjust = (int) floor((16 - age) / 7 + 0.5);
    constexpr int kEducation = 1;
    sources["MIL"]["age adjust"] = ageAdjust;
    sources["DIP"]["age adjust"] = ageAdjust;
    sources["ADM"]["age adjust"] = ageAdjust;
    sources["MIL"]["future education"] = kEducation;
    sources["DIP"]["future education"] = kEducation;
    sources["ADM"]["future education"] = kEducation;
  }

  setCultureAndReligion(capitalTag, ruler, monarchDef);

  Object* defaults = configObject->getNeededObject("default_stats");
  if (defaults->safeGetString("use", "no") == "yes") {
      sources.clear();
      for (auto& stat : std::vector<std::string>{"MIL", "ADM", "DIP"}) {
        sources[stat]["default"] = defaults->safeGetInt(stat, 3);
      }
    }
  Logger::logStream("characters")
      << keyword << " " << nameAndNumber(ruler) << " with\n"
      << LogOption::Indent;
  for (map<string, map<string, double>>::iterator area = sources.begin();
       area != sources.end(); ++area) {
    Logger::logStream("characters") << area->first << " : ";
    int amount = 0;
    for (map<string, double>::iterator source = area->second.begin(); source != area->second.end(); ++source) {
      int rounded = (int) floor(0.5 + source->second);
      if (rounded < 1) continue;
      Logger::logStream("characters") << source->first << " " << rounded << " ";
      amount += rounded;
    }
    if (amount < 1) {
      Logger::logStream("characters") << "minimum 1 ";
      amount = 1;
    }
    else if (amount > 6) {
      Logger::logStream("characters") << "maximum 6 ";
      amount = 6;
    }
    Logger::logStream("characters") << "total: " << amount << "\n";
    monarchDef->setLeaf(area->first, amount);
  }
  if (ruler->safeGetString(femaleString, "no") == "yes")
    monarchDef->setLeaf("female", "yes");
  addPersonality(CK2Character::euRulerTraits, ruler, monarchDef);
  Logger::logStream("characters") << LogOption::Undent;
  monarchDef->setLeaf("dynasty", getDynastyName(ruler));
  monarchDef->setLeaf("birth_date",
                      getConversionDate(remQuotes(ruler->safeGetString(
                          birthDateString, addQuotes(gameDate)))));
  return monarchDef;
}

Object* Converter::makeMonarch(const string& capitalTag, CK2Character* ruler,
                               CK2Ruler* king, const string& keyword,
                               Object* bonusTraits) {
  auto* monarchDef = makeMonarchObject(capitalTag, ruler, keyword, bonusTraits);
  CK2Title* primary = king->getPrimaryTitle();
  monarchDef->setLeaf("country", addQuotes(primary->getEU4Country()->getKey()));
  Object* monarchId = createMonarchId();
  monarchDef->setValue(monarchId);
  Object* coronation = new Object(eu4Game->safeGetString("date", "1444.1.1"));
  coronation->setValue(monarchDef);
  Object* monarch = new Object(monarchId);
  monarch->setKey(keyword);
  for (CK2Title::Iter title = king->startTitle(); title != king->finalTitle(); ++title) {
    EU4Country* country = (*title)->getEU4Country();
    if (!country) continue;
    country->getNeededObject("history")->setValue(coronation);
    country->unsetValue(keyword);
    country->setValue(monarch);
  }
  return monarchDef;
}

Object* createLeader(CK2Character* base, const string& leaderType,
                     const objvec& generalSkills, const string& birthDate) {
  Object* leader = new Object("leader");
  leader->setLeaf("name", getFullName(base));
  leader->setLeaf("type", leaderType);
  for (const auto* skill : generalSkills) {
    string keyword = skill->getKey();
    int amount = 0;
    for (int i = 0; i < skill->numTokens(); ++i) {
      if (!base->hasTrait(skill->getToken(i))) continue;
      ++amount;
    }
    Logger::logStream("characters")
        << " " << keyword << ": " << amount << (amount > 0 ? " from " : "");
    for (int i = 0; i < skill->numTokens(); ++i) {
      if (!base->hasTrait(skill->getToken(i)))
        continue;
      Logger::logStream("characters") << skill->getToken(i) << " ";
    }
    leader->setLeaf(keyword, amount);
    Logger::logStream("characters") << "\n";
  }
  auto personalities = euLeaderTraits->getNeededObject(leaderType)->getLeaves();
  addPersonality(personalities, base, leader);
  leader->setLeaf("activation", birthDate);
  return leader;
}

bool Converter::makeAdvisor(CK2Character* councillor, Object* country_advisors,
                            objvec& advisorTypes, string& birthDate,
                            Object* history, string capitalTag,
                            map<string, objvec>& allAdvisors) {
  static unordered_set<string> existing_advisors;
  if (existing_advisors.find(councillor->getKey()) != existing_advisors.end()) {
    Logger::logStream("characters") << "\n";
    return false;
  }
  existing_advisors.insert(councillor->getKey());

  Object* advisor = new Object("advisor");
  advisor->setLeaf("name", getFullName(councillor));
  int points = -1;
  Object* advObject = 0;
  set<string> decisionTraits;
  for (objiter advType = advisorTypes.begin(); advType != advisorTypes.end();
       ++advType) {
    Object* mods = (*advType)->getNeededObject(traitString);
    objvec traits = mods->getLeaves();
    int currPoints = 0;
    set<string> yesTraits;
    for (objiter trait = traits.begin(); trait != traits.end(); ++trait) {
      string traitName = (*trait)->getKey();
      if ((!councillor->hasTrait(traitName)) &&
          (!councillor->hasModifier(traitName)))
        continue;
      currPoints += mods->safeGetInt(traitName);
      yesTraits.insert(traitName);
    }
    if (currPoints < points)
      continue;
    points = currPoints;
    advObject = (*advType);
    decisionTraits = yesTraits;
  }
  string advisorType = advObject->getKey();
  advisor->setLeaf("type", advisorType);
  allAdvisors[advisorType].push_back(advisor);
  int skill = 0;
  for (CKAttribute::Iter att = CKAttribute::start();
       att != CKAttribute::final(); ++att) {
    int mult = 1;
    if ((*att)->getName() == advObject->safeGetString("main_attribute")) {
      mult = 2;
    }
    skill += councillor->getAttribute(*att) * mult;
  }
  advisor->setLeaf("skill", skill);
  Logger::logStream("characters") << " " << advisorType << " based on traits ";
  for (set<string>::iterator t = decisionTraits.begin();
       t != decisionTraits.end(); ++t) {
    Logger::logStream("characters") << (*t) << " ";
  }
  Logger::logStream("characters") << "\n";
  advisor->setLeaf("location", capitalTag);
  setCultureAndReligion(capitalTag, councillor, advisor);
  advisor->setLeaf("date", "1444.1.1");
  advisor->setLeaf("hire_date", "1.1.1");
  advisor->setLeaf("death_date", "9999.1.1");
  advisor->setLeaf("female", councillor->safeGetString(femaleString, "no"));
  Object* id = createTypedId("advisor", "51");
  advisor->setValue(id);
  Object* birth = new Object(birthDate);
  birth->setValue(advisor);
  history->setValue(birth);
  Object* active = new Object(id);
  active->setKey("advisor");
  country_advisors->setValue(active);
  return true;
}

void Converter::makeLeader(Object* eu4country, const string& leaderType,
                           CK2Character* base, const objvec& generalSkills,
                           const string& birthDate) {
  Object* leader = createLeader(base, leaderType, generalSkills, birthDate);
  Object* id = createTypedId("leader", "49");
  leader->setValue(id);
  Object* active = new Object(id);
  active->setKey("leader");
  eu4country->setValue(active);
  Object* birth = new Object(birthDate);
  birth->setValue(leader);
  eu4country->getNeededObject("history")->setValue(birth);
}

bool Converter::createCharacters () {
  Logger::logStream(LogStream::Info) << "Beginning character creation.\n" << LogOption::Indent;
  Logger::logStream(LogStream::Info)
      << "Found " << CK2Character::ckTraits.size() << " CK traits.\n";
  Object* bonusTraits = configObject->getNeededObject("bonusTraits");
  Object* active_advisors = eu4Game->getNeededObject("active_advisors");
  map<string, objvec> allAdvisors;
  objvec advisorTypes = advisorTypesObject->getLeaves();
  objvec generalSkills = configObject->getNeededObject("generalSkills")->getLeaves();
  string birthDate = eu4Game->safeGetString("date", "1444.11.11");

  int numAdvisors = 0;
  for (auto* eu4country : EU4Country::getAll()) {
    if (!eu4country->converts()) continue;
    Object* country_advisors = active_advisors->getNeededObject(eu4country->getKey());
    country_advisors->clear();

    CK2Ruler* ruler = eu4country->getRuler();
    if (ruler->safeGetString("madeCharacters", PlainNone) == "yes") continue;
    ruler->setLeaf("madeCharacters", "yes");
    string capitalTag = eu4country->safeGetString("capital");
    EU4Province* capital = EU4Province::findByName(capitalTag);
    if (!capital) continue;
    string eu4Tag = eu4country->getKey();
    eu4country->unsetValue("advisor");
    Logger::logStream("characters")
        << "Creating characters for " << eu4Tag << ":\n"
        << LogOption::Indent;
    auto* ck2Regent = ruler->getAdvisor("title_regent");
    if (ck2Regent != nullptr &&
        ruler->getAge(
            remQuotes(ck2Game->safeGetString("date", "\"1444.11.11\""))) < 16) {
      auto* eu4Regent = makeMonarch(capitalTag, ck2Regent, ruler, "monarch", bonusTraits);
      eu4Regent->setLeaf("regent", "yes");
      makeMonarch(capitalTag, ruler, ruler, "heir", bonusTraits);
    } else {
      makeMonarch(capitalTag, ruler, ruler, "monarch", bonusTraits);
      eu4country->resetLeaf("original_dynasty",
                            addQuotes(getDynastyName(ruler)));

      eu4country->unsetValue("heir");
      if (eu4country->safeGetString("needs_heir", "no") == "yes") {
        CK2Character* ckHeir = ruler->getHeir();
        if (ckHeir) makeMonarch(capitalTag, ckHeir, ruler, "heir", bonusTraits);
      }
    }

    auto* spouse = ruler->getBestSpouse();
    if (spouse) {
      auto* queen = makeMonarch(capitalTag, spouse, ruler, "queen", bonusTraits);
      queen->setLeaf("consort", "yes");
    }

    Object* history = capital->getNeededObject("history");
    for (CouncilTitle::Iter council = CouncilTitle::start(); council != CouncilTitle::final(); ++council) {
      CK2Character* councillor = ruler->getCouncillor(*council);
      Logger::logStream("characters")
          << (*council)->getName() << ": "
          << (councillor ? nameAndNumber(councillor) : "None\n");
      if (!councillor) {
        continue;
      }
      if (makeAdvisor(councillor, country_advisors, advisorTypes, birthDate,
                      history, capitalTag, allAdvisors)) {
        ++numAdvisors;
      }
    }

    for (const auto& advisor : ruler->getAdvisors()) {
      string key = advisor.first;
      for (auto* courtier : advisor.second) {
        Logger::logStream("characters")
            << key << ": " << nameAndNumber(courtier);
        if (makeAdvisor(courtier, country_advisors, advisorTypes, birthDate,
                        history, capitalTag, allAdvisors)) {
          numAdvisors++;
        }
      }
    }

    CK2Character* bestGeneral = 0;
    int highestSkill = -1;
    for (CK2Character::CharacterIter commander = ruler->startCommander();
         commander != ruler->finalCommander(); ++commander) {
      int currSkill = 0;
      for (objiter skill = generalSkills.begin(); skill != generalSkills.end(); ++skill) {
	string keyword = (*skill)->getKey();
	for (int i = 0; i < (*skill)->numTokens(); ++i) {
	  if (!(*commander)->hasTrait((*skill)->getToken(i))) continue;
	  (*commander)->resetLeaf(keyword, 1 + (*commander)->safeGetInt(keyword));
	  ++currSkill;
	}
      }
      if (currSkill < highestSkill) continue;
      highestSkill = currSkill;
      bestGeneral = (*commander);
    }
    Logger::logStream("characters")
        << "General: " << (bestGeneral ? nameAndNumber(bestGeneral) : "None")
        << "\n";
    if (bestGeneral) {
      Logger::logStream("characters") << LogOption::Indent;
      makeLeader(eu4country->object, string("general"), bestGeneral,
                 generalSkills, birthDate);
      Logger::logStream("characters") << LogOption::Undent;
    }

    CK2Character* admiral = ruler->getAdmiral();
    Logger::logStream("characters")
        << "Admiral: " << (admiral ? nameAndNumber(admiral) : "None") << "\n";
    if (admiral) {
      Logger::logStream("characters") << LogOption::Indent;
      makeLeader(eu4country->object, string("admiral"), admiral,
                 generalSkills, birthDate);
      Logger::logStream("characters") << LogOption::Undent;
    }

    Logger::logStream("characters") << LogOption::Undent;
  }
  Logger::logStream("characters") << "Advisors created:\n" << LogOption::Indent;
  ObjectDescendingSorter skillSorter("skill");
  for (map<string, objvec>::iterator adv = allAdvisors.begin(); adv != allAdvisors.end(); ++adv) {
    Logger::logStream("characters") << adv->first << ": " << adv->second.size() << "\n";
    sort(adv->second.begin(), adv->second.end(), skillSorter);
    for (unsigned int i = 0; i < adv->second.size(); ++i) {
      double fraction = i;
      fraction /= adv->second.size();
      int actualSkill = (fraction < 0.1 ? 3 : (fraction < 0.333 ? 2 : 1));
      adv->second[i]->resetLeaf("skill", actualSkill);
    }
  }

  Logger::logStream("characters") << "Total advisors created: " << numAdvisors << ".\n"
				  << LogOption::Undent;

  Logger::logStream(LogStream::Info) << "Done with character creation.\n" << LogOption::Undent;
  return true;
}

bool Converter::createGovernments () {
  Logger::logStream(LogStream::Info) << "Beginning governments.\n" << LogOption::Indent;
  Object* govObject = configObject->getNeededObject("governments");
  double empireThreshold = configObject->safeGetFloat("empire_threshold", 1000);
  double kingdomThreshold = configObject->safeGetFloat("kingdom_threshold", 1000);
  for (auto* eu4country : EU4Country::getAll()) {
    CK2Ruler* ruler = eu4country->getRuler();
    if (!ruler) continue;
    CK2Title* primary = ruler->getPrimaryTitle();
    string government_rank = "1";
    double totalDevelopment = 0;
    for (auto* eu4prov : eu4country->getProvinces()) {
      totalDevelopment += eu4prov->totalDev();
    }
    if (totalDevelopment > empireThreshold) government_rank = "3";
    else if (totalDevelopment > kingdomThreshold) government_rank = "2";
    eu4country->resetLeaf("government_rank", government_rank);
    eu4country->resetHistory("government_rank", government_rank);
    string ckGovernment =
        ruler->safeGetString(governmentString, "feudal_government");
    Object* govInfo = govObject->safeGetObject(ckGovernment);
    Object* euGovernment = eu4country->getGovernment();
    if (customObject) {
      Object* customGov = customObject->getNeededObject("governments")
                            ->safeGetObject(eu4country->getKey());
      if (customGov) {
        ckGovernment = "custom override";
        govInfo = customGov;
      }
    }
    if (govInfo) {
      Logger::logStream("governments")
          << nameAndNumber(ruler) << " of " << eu4country->getKey()
          << " has CK government " << ckGovernment;
      if (primary) {
	string succession = primary->safeGetString("succession", PlainNone);
	Object* successionObject = govInfo->safeGetObject(succession);
	if (successionObject) {
	  Logger::logStream("governments") << " (" << succession << ")";
	  govInfo = successionObject;
	}
      }
      euGovernment = govInfo->safeGetObject("government");
      Logger::logStream("governments")
          << " giving EU: " << euGovernment << " of rank " << government_rank
          << " based on total development " << totalDevelopment << ".\n";
      eu4country->setGovernment(govInfo);
    }
    else {
      Logger::logStream(LogStream::Warn)
          << "No information about CK government " << ckGovernment
          << ", leaving " << eu4country->getKey() << " as " << euGovernment
          << ".\n";
    }
  }  
  Logger::logStream(LogStream::Info) << "Done with governments.\n" << LogOption::Undent;
  return true;
}

bool Converter::createNavies () {
  Logger::logStream(LogStream::Info) << "Beginning navy creation.\n" << LogOption::Indent;
  objvec navyIds;
  objvec shipIds;
  set<string> navyLocations;
  map<string, vector<string> > shipNames;
  string navyTypeNumber = "54";
  string shipTypeNumber = "54";
  std::map<string, int> shipTypes;

  auto* whitelist = configObject->safeGetObject("allowNavies");
  if (whitelist) {
    for (int i = 0; i < whitelist->numTokens(); ++i) {
      navyLocations.insert(whitelist->getToken(i));
    }
  }

  for (auto* eu4country : EU4Country::getAll()) {
    if (eu4country->isROTW()) {
      continue;
    }
    objvec navies = eu4country->getValue("navy");
    for (objiter navy = navies.begin(); navy != navies.end(); ++navy) {
      Object* id = (*navy)->safeGetObject("id");
      if (id) navyIds.push_back(id);
      string location = (*navy)->safeGetString("location", "-1");
      if (location != "-1") navyLocations.insert(location);
      objvec ships = (*navy)->getValue("ship");
      for (objiter ship = ships.begin(); ship != ships.end(); ++ship) {
	Object* id = (*ship)->safeGetObject("id");
	if (id) shipIds.push_back(id);
	location = (*ship)->safeGetString("home", "-1");
	if (location != "-1") navyLocations.insert(location);
        shipTypes[(*ship)->safeGetString("type")]++;
	shipNames[eu4country->getKey()].push_back((*ship)->safeGetString("name"));
      }
    }
    eu4country->unsetValue("navy");
  }

  if (0 < shipIds.size()) shipTypeNumber = shipIds.back()->safeGetString("type", shipTypeNumber);
  if (0 < navyIds.size()) navyTypeNumber = navyIds.back()->safeGetString("type", navyTypeNumber);

  double ckShips = 0;
  Object* shipWeights = configObject->getNeededObject("ships");
  objvec ckShipTypes = shipWeights->getLeaves();
  for (auto* ruler : CK2Ruler::getAll()) {
    EU4Country* eu4country = ruler->getEU4Country();
    if (!eu4country) continue;
    double currShip = 0;
    for (objiter barony = eu4country->startBarony();
         barony != eu4country->finalBarony(); ++barony) {
      Object* baronyLevy = (*barony)->getNeededObject("levy");
      double baronyShips = 0;
      for (auto* ttype : ckShipTypes) {
	string key = ttype->getKey();
	double amount = getLevyStrength(key, baronyLevy);
	amount *= shipWeights->safeGetFloat(key);
	baronyShips += amount;
      }
      Logger::logStream("navies") << "Barony " << (*barony)->getKey() << " has "
                                  << baronyShips << " ships.\n";
      currShip += baronyShips;
    }
    Logger::logStream("navies")
        << eu4country->getKey() << " has " << currShip << " CK ships.\n";
    ckShips += currShip;
    ruler->resetLeaf("shipWeight", currShip);    
  }

  if (0 == ckShips) {
    Logger::logStream(LogStream::Warn)
        << "Warning: Found no liege-levy ships. No navies will be created.\n"
        << LogOption::Undent;
    return true;
  }
  
  Object* forbidden = configObject->getNeededObject("forbidNavies");
  set<string> forbid;
  for (int i = 0; i < forbidden->numTokens(); ++i) {
    forbid.insert(forbidden->getToken(i));
  }
  Logger::logStream("navies") << "Total weighted navy strength " << ckShips << "\n";

  for (const auto& shipType : shipTypes) {
    int numShips = shipType.second;
    Logger::logStream("navies")
        << "EU4 ships of type " << shipType.first << ": " << numShips << ".\n"
        << LogOption::Indent;
    for (auto* ruler : CK2Ruler::getAll()) {
      EU4Country* eu4country = ruler->getEU4Country();
      if (!eu4country || !eu4country->converts()) {
        continue;
      }
      string eu4Tag = eu4country->getKey();
      double currWeight = ruler->safeGetFloat("shipWeight");

      currWeight /= ckShips;
      currWeight *= numShips;
      int shipsToCreate = (int)floor(0.5 + currWeight);
      Logger::logStream("navies")
          << nameAndNumber(ruler) << " of " << eu4Tag << " has ship weight "
          << currWeight << " and gets " << shipsToCreate << " ships of type "
          << shipType.first << "\n";
      if (0 == shipsToCreate) {
        continue;
      }
      Object* navy = eu4country->getNeededObject("navy");
      Object* navyId = nullptr;
      if (navy->safeGetObject("id") == nullptr) {
        if (!navyIds.empty()) {
          navyId = navyIds.back();
          navyIds.pop_back();
        } else {
          navyId = createUnitId(navyTypeNumber);
        }
        navy->setValue(navyId);
      }
      string name =
          remQuotes(ruler->safeGetString(birthNameString, "\"Nemo\""));
      navy->setLeaf("name", addQuotes(string("Navy of ") + name));
      string capitalTag = eu4country->safeGetString("capital");
      string location = capitalTag;
      if ((0 == navyLocations.count(capitalTag)) ||
          (forbid.count(capitalTag))) {
        for (EU4Province::Iter prov = eu4country->startProvince();
             prov != eu4country->finalProvince(); ++prov) {
          string provTag = (*prov)->getKey();
          if (forbid.count(provTag)) {
            continue;
          }
          if ((0 == forbid.count(location)) &&
              (0 == navyLocations.count(provTag))) {
            continue;
          }
          location = provTag;
          if (navyLocations.count(location)) {
            break;
          }
        }
      }

      Logger::logStream("navies") << "Putting navy of " << nameAndNumber(ruler)
                                  << " in " << location << "\n";
      navy->setLeaf("location", location);
      EU4Province* eu4prov = EU4Province::findByName(location);
      if (navyId != nullptr) {
        Object* unit = new Object(navyId);
        unit->setKey("unit");
        eu4prov->setValue(unit);
      }
      int created = 0;
      for (int i = 0; i < shipsToCreate; ++i) {
        Object* ship = new Object("ship");
        navy->setValue(ship);
        Object* shipId = 0;
        if (shipIds.empty()) {
          shipId = createUnitId(shipTypeNumber);
        } else {
          shipId = shipIds.back();
          shipIds.pop_back();
        }
        ship->setValue(shipId);
        ship->setLeaf("home", location);
        string name;
        if (!shipNames[eu4Tag].empty()) {
          name = shipNames[eu4Tag].back();
          shipNames[eu4Tag].pop_back();
        } else {
          name = createString("\"%s conversion ship %i\"", eu4Tag.c_str(),
                              ++created);
        }
        ship->setLeaf("name", name);
        ship->setLeaf("type", shipType.first);
        ship->setLeaf("strength", "1.000");
      }
    }
    Logger::logStream("navies") << LogOption::Undent;
  }
  
  Logger::logStream(LogStream::Info) << "Done with navies.\n" << LogOption::Undent;
  return true;
}

void findCorrespondence (CK2Province* ck2prov, EU4Province* eu4prov, map<string, map<string, int> >& corrMap, string key) {
  string ckThing = ck2prov->safeGetString(key, PlainNone);
  if (PlainNone == ckThing) return;
  string euThing = eu4prov->safeGetString(key, PlainNone);
  if (PlainNone == euThing) return;
  corrMap[ckThing][euThing]++;
}

bool heuristicNameMatch (const string& one, const string& two) {
  if (one == two) return true;
  int length = min(one.size(), two.size());
  double overlap = 0;
  for (int i = 0; i < length; ++i) {
    if (one[i] == two[i]) ++overlap;
  }
  overlap /= max(one.size(), two.size());
  return overlap > 0.8;
}

pair<string, string> findBest (map<string, map<string, int> >& corrMap) {
  pair<string, string> ret(PlainNone, PlainNone);
  int highest = 0;
  for (map<string, map<string, int> >::iterator corr = corrMap.begin(); corr != corrMap.end(); ++corr) {
    for (map<string, int>::iterator curr = corr->second.begin(); curr != corr->second.end(); ++curr) {
      if (heuristicNameMatch(curr->first, corr->first)) {
	Logger::logStream("cultures") << "Assigning "
				      << corr->first << " to "
				      << curr->first << " based on similar names.\n";
	return pair<string, string>(corr->first, curr->first);
      }
      if (curr->second <= highest) continue;
      highest    = curr->second;
      ret.first  = corr->first;
      ret.second = curr->first;
    }
  }

  Logger::logStream("cultures") << "Assigning " << ret.first << " to "
				<< ret.second << " based on overlap " << highest << " ";

  return ret;
}

void makeMap(map<string, map<string, int>>& cultures,
             map<string, vector<string>>& assigns, double splitCutoff = 2, Object* storage = 0) {
  map<string, map<string, int> > reverseMap;
  for (map<string, map<string, int> >::iterator cult = cultures.begin(); cult != cultures.end(); ++cult) {
    for (map<string, int>::iterator look = cult->second.begin(); look != cult->second.end(); ++look) {
      reverseMap[look->first][cult->first] += look->second;
    }
  }

  while (!cultures.empty()) {
    pair<string, string> best = findBest(cultures);
    string ckCulture = best.first;
    if (ckCulture == PlainNone) break;
    if (storage) storage->setLeaf(best.first, best.second);
    string euCulture = best.second;
    assigns[ckCulture].push_back(euCulture);
    double highestValue = cultures[ckCulture][euCulture];
    
    for (map<string, int>::iterator cand = cultures[ckCulture].begin(); cand != cultures[ckCulture].end(); ++cand) {
      if (cand->first == euCulture) continue;
      Logger::logStream("cultures") << "(" << cand->first << " " << cand->second << " ";
      if (cand->second < highestValue * splitCutoff) {
	Logger::logStream("cultures") << ") ";
        continue;
      }
      if (1 == reverseMap[cand->first].size()) {
	Logger::logStream("cultures") << "[sole]) ";
      }
      else {
	Logger::logStream("cultures") << "[" << cand->second / highestValue << "]) ";
      }
      assigns[ckCulture].push_back(cand->first);
    }
    Logger::logStream("cultures") << "\n";
    cultures.erase(ckCulture);
  }
}

void proselytise (map<string, vector<string> >& conversionMap, string key) {
  for (EU4Province::Iter eu4prov = EU4Province::start(); eu4prov != EU4Province::final(); ++eu4prov) {
    if (0 == (*eu4prov)->numCKProvinces()) continue;
    map<string, double> weights;
    double weightiest = 0;
    string ckBest;
    for (CK2Province::Iter ck2prov = (*eu4prov)->startProv(); ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
      string ckReligion = (*ck2prov)->safeGetString(key, PlainNone);
      if (PlainNone == ckReligion) continue;
      weights[ckReligion] += (*ck2prov)->getWeight(ProvinceWeight::Manpower);
      if (weights[ckReligion] <= weightiest) continue;
      weightiest = weights[ckReligion];
      ckBest = ckReligion;
    }
    if (ckBest.empty()) {
      Logger::logStream("cultures") << "No culture found for " << nameAndNumber(*eu4prov) << ", probably nomad fief. Ignoring.\n";
      continue;
    }
    string euBest = conversionMap[ckBest][0];
    if (key == "culture") {
      string existingCulture = (*eu4prov)->safeGetString("culture", PlainNone);
      if (find(conversionMap[ckBest].begin(), conversionMap[ckBest].end(), existingCulture) != conversionMap[ckBest].end()) {
	euBest = existingCulture;
      }
    }
    (*eu4prov)->resetLeaf(key, euBest);
    (*eu4prov)->resetHistory(key, euBest);
  }
}

void recursiveFindCulture (CK2Ruler* ruler, map<string, double>& religion, map<string, double>& culture) {
  int weight = 1;
  CK2Title* primary = ruler->getPrimaryTitle();
  if (primary) weight += *(primary->getLevel());
  string ckCulture  = ruler->getBelief("culture");
  if (ckCulture != PlainNone) culture[ckCulture] += weight;
  string ckReligion = ruler->getBelief("religion");
  if (ckReligion != PlainNone) religion[ckReligion] += weight;
  for (CK2Ruler::Iter vassal = ruler->startVassal(); vassal != ruler->finalVassal(); ++vassal) {
    recursiveFindCulture((*vassal), religion, culture);
  }
}

string findMax (map<string, double>& candidates) {
  if (candidates.empty()) return PlainNone;
  string ret = PlainNone;
  for (map<string, double>::iterator cand = candidates.begin(); cand != candidates.end(); ++cand) {
    if (cand->second > candidates[ret]) ret = cand->first;
  }
  return ret;
}

bool Converter::cultureAndReligion () {
  Logger::logStream(LogStream::Info) << "Beginning culture and religion.\n" << LogOption::Indent;
  map<string, map<string, int> > religions;
  map<string, map<string, int> > cultures;
  for (CK2Province::Iter ck2prov = CK2Province::start(); ck2prov != CK2Province::final(); ++ck2prov) {
    for (EU4Province::Iter eu4prov = (*ck2prov)->startEU4Province(); eu4prov != (*ck2prov)->finalEU4Province(); ++eu4prov) {
      findCorrespondence((*ck2prov), (*eu4prov), religions, "religion");
      findCorrespondence((*ck2prov), (*eu4prov), cultures, "culture");
    }
  }
  
  Object* dynamicReligion = configObject->getNeededObject("dynamicReligions");
  makeMap(religions, religionMap, 2, dynamicReligion);
  Object* overrideObject = customObject->getNeededObject("religion_overrides");
  objvec overrides = overrideObject->getLeaves();
  for (objiter override = overrides.begin(); override != overrides.end(); ++override) {
    string ckReligion = (*override)->getKey();
    string euReligion = (*override)->getLeaf();
    Logger::logStream("cultures") << "Override: " << ckReligion << " assigned to " << euReligion << ".\n";
    religionMap[ckReligion].clear();
    religionMap[ckReligion].push_back(euReligion);
    dynamicReligion->resetLeaf(ckReligion, euReligion);
  }

  makeMap(cultures, cultureMap, configObject->safeGetFloat("split_cutoff", 0.75));
  overrideObject = customObject->getNeededObject("culture_overrides");
  overrides = overrideObject->getLeaves();
  for (objiter override = overrides.begin(); override != overrides.end(); ++override) {
    string ckCulture = (*override)->getKey();
    string euCulture = (*override)->getLeaf();
    Logger::logStream("cultures") << "Override: " << ckCulture << " assigned to " << euCulture << ".\n";
    cultureMap[ckCulture].clear();
    cultureMap[ckCulture].push_back(euCulture);
  }

  string overrideCulture = configObject->safeGetString("overwrite_culture", PlainNone);
  if (overrideCulture == "no") overrideCulture = PlainNone;
  if (overrideCulture != PlainNone) {
    for (map<string, vector<string> >::iterator cult = cultureMap.begin(); cult != cultureMap.end(); ++cult) {
      cult->second.clear();
      cult->second.push_back(overrideCulture);
    }
  }
  proselytise(religionMap, "religion");
  proselytise(cultureMap, "culture");

  double acceptedCutoff = configObject->safeGetFloat("accepted_culture_threshold", 0.5);
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    CK2Ruler* ruler = (*eu4country)->getRuler();
    if (!ruler) continue;
    map<string, double> religionWeights;
    map<string, double> cultureWeights;
    recursiveFindCulture(ruler, religionWeights, cultureWeights);
    string ckRulerReligion = findMax(religionWeights);
    string ckRulerCulture  = findMax(cultureWeights);
    vector<string> acceptedCultures;
    for (map<string, double>::iterator cand = cultureWeights.begin(); cand != cultureWeights.end(); ++cand) {
      if (cand->first == ckRulerCulture) continue;
      if (cand->second > acceptedCutoff * cultureWeights[ckRulerCulture]) {
	acceptedCultures.push_back(cultureMap[cand->first][0]);
      }
    }
    Logger::logStream("cultures") << (*eu4country)->getKey() << ":\n" << LogOption::Indent;
    if (ckRulerReligion != PlainNone) {
      if (0 == religionMap[ckRulerReligion].size()) {
	Logger::logStream("cultures") << "Emergency-assigning " << ckRulerReligion << " to catholic.\n";
	religionMap[ckRulerReligion].push_back("catholic");
      }
      string euReligion = religionMap[ckRulerReligion][0];
      Logger::logStream("cultures")
          << "Religion: " << euReligion << " based on CK religion "
          << ckRulerReligion << ".\n";
      (*eu4country)->resetLeaf("religion", euReligion);
      (*eu4country)->resetHistory("religion", euReligion);
    }
    else {
      Logger::logStream(LogStream::Warn) << "Religion: Not found, leaving as "
					 << (*eu4country)->safeGetString("religion", PlainNone)
					 << ".\n";
    }

    if (ckRulerCulture != PlainNone) {
      if (0 == cultureMap[ckRulerCulture].size()) {
	Logger::logStream("cultures") << "Emergency-assigning " << ckRulerCulture
				     << " to norwegian.";
	cultureMap[ckRulerCulture].push_back("norwegian");
      }
      string euCulture = cultureMap[ckRulerCulture][0];
      if (1 < cultureMap[ckRulerCulture].size()) {
	// If there are multiple candidates, pick the one from the EU4 capital province.
	string capitalTag = (*eu4country)->safeGetString("capital");
	EU4Province* capital = EU4Province::findByName(capitalTag);
	if (capital) {
	  string capitalCulture = capital->safeGetString("culture");
	  if (find(cultureMap[ckRulerCulture].begin(), cultureMap[ckRulerCulture].end(), capitalCulture) != cultureMap[ckRulerCulture].end()) {
	    euCulture = capitalCulture;
	  }
	}
      }
      Logger::logStream("cultures") << "Culture: "
				    << euCulture
				    << " based on CK culture "
				    << ckRulerCulture
				    << ".\n";

      (*eu4country)->resetLeaf("primary_culture", euCulture);
      (*eu4country)->resetHistory("primary_culture", euCulture);
      (*eu4country)->unsetValue("accepted_culture");
      Object* history = (*eu4country)->getNeededObject("history");
      history->unsetValue("add_accepted_culture");
      if (acceptedCultures.size()) {
	Logger::logStream("cultures") << "Accepted cultures: ";
	for (vector<string>::iterator accepted = acceptedCultures.begin(); accepted != acceptedCultures.end(); ++accepted) {
	  Logger::logStream("cultures") << (*accepted) << " ";
	  (*eu4country)->setLeaf("accepted_culture", (*accepted));
	  history->setLeaf("add_accepted_culture", (*accepted));
	}
	Logger::logStream("cultures") << "\n";
      }
    }
    else {
      Logger::logStream("cultures") << "Culture: Not found, leaving as " << (*eu4country)->safeGetString("primary_culture", PlainNone) << "\n";
    }
    Logger::logStream("cultures") << LogOption::Undent;
  }

  Logger::logStream(LogStream::Info) << "Done with culture and religion.\n" << LogOption::Undent;
  return true;
}

Object* Converter::createTypedId (string keyword, string idType) {
  static const std::unordered_map<string, unsigned int> keyword_map = {
      {"monarch", 1}, {"leader", 2}, {"advisor", 3}, {"rebel", 4}};

  int number = -1;
  Object* counters = eu4Game->safeGetObject("id_counters");
  if (counters != nullptr) {
    auto entry = keyword_map.find(keyword);
    if (entry != keyword_map.end()) {
      unsigned int index = entry->second;
      number = counters->tokenAsInt(index);
      sprintf(strbuffer, "%i", number + 1);
      counters->resetToken(index, strbuffer);
    }
  }
  if (number == -1) {
    number = eu4Game->safeGetInt(keyword, 1);
    eu4Game->resetLeaf(keyword, number + 1);
  }

  Object* unitId = new Object("id");
  unitId->setLeaf("id", number);
  unitId->setLeaf("type", idType);
  return unitId;
}

Object* Converter::createUnitId (string unitType) {
  return createTypedId("unit", unitType);
}

Object* Converter::createMonarchId () {
  return createTypedId("monarch", "48");
}

bool Converter::hreAndPapacy () {
  Logger::logStream(LogStream::Info) << "Beginning HRE and papacy.\n" << LogOption::Indent;
  string hreOption = configObject->safeGetString("hre", "keep");
  const std::unordered_set<string> allowedOptions = {"keep", "remove", "dynasties"};
  if (allowedOptions.find(hreOption) == allowedOptions.end()) {
    Logger::logStream(LogStream::Warn)
        << "Unknown HRE option " << hreOption << ", defaulting to 'keep'.\n";
    hreOption = "keep";
  }

  std::unordered_set<string> hre_dynasties;
  std::set<string> hre_electors;
  string empTag = addQuotes("---");
  if (customObject && hreOption == "dynasties") {
    auto* members = customObject->getNeededObject("empire_members");
    for (int i = 0; i < members->numTokens(); ++i) {
      hre_dynasties.insert(members->getToken(i));
      Logger::logStream("hre") << members->getToken(i) << " is HRE dynasty.\n";
    }
    auto* electors = customObject->getNeededObject("electors");
    for (int i = 0; i < electors->numTokens(); ++i) {
      hre_electors.insert(electors->getToken(i));
      Logger::logStream("hre") << electors->getToken(i) << " is elector.\n";
    }
    if (!hre_electors.empty()) {
      empTag = addQuotes(electors->getToken(0));
      Logger::logStream("hre") << electors->getToken(0) << " is emperor.\n";
    }
  }

  if (hreOption != "keep") {
    Logger::logStream("hre") << "Removing bratwurst with option " << hreOption << ".\n";
    for (auto* eu4prov : EU4Province::getAll()) {
      if (hreOption == "dynasties") {
        auto* country = eu4prov->getEU4Country();
        CK2Ruler* ruler = nullptr;
        if (country) {
          ruler = country->getRuler();
        } else {
          Logger::logStream("hre")
              << "No country for " << nameAndNumber(eu4prov) << "\n";
        }
        string dynastyId = PlainNone;
        if (ruler) {
          dynastyId = ruler->safeGetString(dynastyString, PlainNone);
        } else {
          Logger::logStream("hre")
              << "No ruler for " << nameAndNumber(eu4prov) << "\n";
        }
        if (dynastyId != PlainNone &&
            hre_dynasties.find(dynastyId) != hre_dynasties.end()) {
          Logger::logStream("hre")
              << nameAndNumber(eu4prov) << " belongs to "
              << nameAndNumber(ruler) << " of HRE dynasty " << dynastyId
              << ", making part of HRE.\n";
          eu4prov->resetLeaf("hre", "yes");
          eu4prov->getNeededObject("history")->resetLeaf("hre", "yes");
        } else {
          Logger::logStream("hre") << "No HRE for " << nameAndNumber(eu4prov)
                                   << " " << dynastyId << "\n";
          eu4prov->unsetValue("hre");
        }
      } else {
        eu4prov->unsetValue("hre");
      }
    }
    for (auto* eu4country : EU4Country::getAll()) {
      eu4country->unsetValue("preferred_emperor");
      if (hre_electors.find(eu4country->getKey()) != hre_electors.end()) {
        eu4country->resetLeaf("is_elector", "yes");
      } else {
        eu4country->unsetValue("is_elector");
      }
    }
    Object* empire = eu4Game->safeGetObject("empire");
    if (empire) {
      auto* electorObject = empire->getNeededObject("electors");
      if (!hre_electors.empty()) {
        for (auto& tag : hre_electors) {
          electorObject->addToList(addQuotes(tag));
        }
      }
      empire->resetLeaf("emperor", empTag);
      empire->unsetValue("old_emperor");
    }
  }

  Object* papacy = eu4Game->getNeededObject("religions");
  papacy = papacy->getNeededObject("catholic");
  papacy = papacy->getNeededObject("papacy");
  papacy->resetLeaf("papal_state", addQuotes("PAP"));
  papacy->resetLeaf("controller", addQuotes("PAP"));
  papacy->resetLeaf("previous_controller", addQuotes("PAP"));
  papacy->resetLeaf("crusade_target", "\"\"");
  papacy->resetLeaf("crusade_start", "1.1.1");
  papacy->resetLeaf("last_excom", "1.1.1");
  papacy->resetLeaf("papacy_active", "yes");
  papacy->resetLeaf("weighted_cardinal", "1");
  papacy->resetLeaf("reform_desire", "0.000");
  papacy->resetLeaf("papal_investment", "0.000");
  papacy = papacy->getNeededObject("active_cardinals");
  papacy->clear();

  int createdCardinals = 0;
  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    CK2Ruler* sovereign = (*ruler)->getSovereignLiege();
    if (!sovereign) continue;
    EU4Country* eu4country = sovereign->getEU4Country();
    if (!eu4country) continue;

    objvec titles = (*ruler)->getValue("title");
    bool isCardinal = false;
    for (objiter title = titles.begin(); title != titles.end(); ++title) {
      if ((*title)->getLeaf() != "\"title_cardinal\"") continue;
      isCardinal = true;
      break;
    }

    if (!isCardinal) continue;
    Logger::logStream("hre") << nameAndNumber(*ruler) << " is cardinal in "
			     << eu4country->getKey() << ".\n";
    Object* cardinal = new Object("cardinal");
    papacy->setValue(cardinal);
    cardinal->setLeaf("location", eu4country->safeGetString("capital"));
    if (++createdCardinals >= 7) break;
  }

  Logger::logStream(LogStream::Info) << "Done with HRE and papacy.\n" << LogOption::Undent;
  return true;
}

bool Converter::redistributeDevelopment () {
  double totalBaseTax = 0;
  double totalBasePro = 0;
  double totalBaseMen = 0;
  for (EU4Province::Iter eu4prov = EU4Province::start(); eu4prov != EU4Province::final(); ++eu4prov) {
    if (0 == (*eu4prov)->numCKProvinces()) continue;
    totalBaseTax += (*eu4prov)->safeGetFloat("base_tax");
    totalBasePro += (*eu4prov)->safeGetFloat("base_production");
    totalBaseMen += (*eu4prov)->safeGetFloat("base_manpower");
  }

  Logger::logStream("provinces") << "Redistributing "
				 << totalBaseTax << " base tax, "
				 << totalBasePro << " base production, "
				 << totalBaseMen << " base manpower.\n";

  double totalCKtax = 0;
  double totalCKpro = 0;
  double totalCKmen = 0;
  for (CK2Province::Iter ck2prov = CK2Province::start(); ck2prov != CK2Province::final(); ++ck2prov) {
    totalCKtax += (*ck2prov)->getWeight(ProvinceWeight::Taxation);
    totalCKpro += (*ck2prov)->getWeight(ProvinceWeight::Production);
    totalCKmen += (*ck2prov)->getWeight(ProvinceWeight::Manpower);
  }

  Logger::logStream("provinces") << "CK totals "
				 << totalCKtax << " base tax, "
				 << totalCKpro << " base production, "
				 << totalCKmen << " base manpower.\n";

  totalBaseTax /= totalCKtax;
  totalBasePro /= totalCKpro;
  totalBaseMen /= totalCKmen;

  double afterTax = 0;
  double afterPro = 0;
  double afterMen = 0;
  bool useDoubles = (configObject->safeGetString("float_dev_values", "no") == "yes");
  for (EU4Province::Iter eu4prov = EU4Province::start(); eu4prov != EU4Province::final(); ++eu4prov) {
    if (0 == (*eu4prov)->numCKProvinces()) continue;
    double provTaxWeight = 0;
    double provProWeight = 0;
    double provManWeight = 0;
    Logger::logStream("provinces")
        << "Province " << nameAndNumber(*eu4prov) << "\n"
        << LogOption::Indent;
    for (CK2Province::Iter ck2prov = (*eu4prov)->startProv();
         ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
      double fraction = 1.0 / (*ck2prov)->numEU4Provinces();
      provTaxWeight +=
          fraction * (*ck2prov)->getWeight(ProvinceWeight::Taxation);
      provProWeight +=
          fraction * (*ck2prov)->getWeight(ProvinceWeight::Production);
      provManWeight +=
          fraction * (*ck2prov)->getWeight(ProvinceWeight::Manpower);
      Logger::logStream("provinces")
          << nameAndNumber(*ck2prov) << ": " << fraction << ", "
          << (*ck2prov)->getWeight(ProvinceWeight::Taxation) << ", "
          << (*ck2prov)->getWeight(ProvinceWeight::Production) << ", "
          << (*ck2prov)->getWeight(ProvinceWeight::Manpower) << "\n";
    }

    provTaxWeight *= totalBaseTax;
    provProWeight *= totalBasePro;
    provManWeight *= totalBaseMen;
    double amount = max(0.0, floor(provTaxWeight + 0.5));
    if (useDoubles) amount = provTaxWeight;
    (*eu4prov)->resetLeaf("base_tax", amount); afterTax += amount;
    amount = max(0.0, floor(provProWeight + 0.5));
    if (useDoubles) amount = provProWeight;
    (*eu4prov)->resetLeaf("base_production", amount); afterPro += amount;
    amount = max(0.0, floor(provManWeight + 0.5));
    if (useDoubles) amount = provManWeight;
    (*eu4prov)->resetLeaf("base_manpower", amount); afterMen += amount;
    Logger::logStream("provinces") << provTaxWeight << ", " << provProWeight
                                   << ", " << provManWeight << "\n";
    Logger::logStream("provinces")
        << (*eu4prov)->safeGetFloat("base_tax") << ", "
        << (*eu4prov)->safeGetFloat("base_production") << ", "
        << (*eu4prov)->safeGetFloat("base_manpower") << "\n"
        << LogOption::Undent;
  }

  Logger::logStream("provinces") << "After distribution: "
				 << afterTax << " base tax, "
				 << afterPro << " base production, "
				 << afterMen << " base manpower.\n";

  return true;
}

bool Converter::rankProvinceDevelopment () {
  static const string kDevSortKey = "development_weight";
  unordered_set<EU4Province*> unassignedEu4Provs;
  objvec sorted_eu4_devs;
  for (EU4Province::Iter eu4prov = EU4Province::start();
       eu4prov != EU4Province::final(); ++eu4prov) {
    if (0 == (*eu4prov)->numCKProvinces()) {
      continue;
    }
    unassignedEu4Provs.insert(*eu4prov);
    sorted_eu4_devs.emplace_back(new Object((*eu4prov)->getKey()));
    double dev = 0;
    for (const auto& key : {"base_tax", "base_production", "base_manpower"}) {
      double val = (*eu4prov)->safeGetFloat(key);
      dev += val;
      sorted_eu4_devs.back()->resetLeaf(key, val);
    }
    sorted_eu4_devs.back()->resetLeaf(kDevSortKey, dev);
    sorted_eu4_devs.back()->resetLeaf("name",
                                      (*eu4prov)->safeGetString("name"));
  }

  vector<CK2Province*> sorted_ck2_provs;
  vector<CK2Province*> wasted_ck2_provs;
  for (auto* ck2prov : CK2Province::getAll()) {
    if (0 == ck2prov->numEU4Provinces()) continue;
    double dev = ck2prov->getWeight(ProvinceWeight::Taxation);
    dev += ck2prov->getWeight(ProvinceWeight::Production);
    dev += ck2prov->getWeight(ProvinceWeight::Manpower);
    ck2prov->resetLeaf(kDevSortKey, dev);
    if (ck2prov->safeGetString("primary_settlement") == "\"---\"") {
      wasted_ck2_provs.push_back(ck2prov);
    } else {
      sorted_ck2_provs.push_back(ck2prov);
    }
  }

  ObjectDescendingSorter sorter(kDevSortKey);
  sort(sorted_ck2_provs.begin(), sorted_ck2_provs.end(), sorter);
  sort(wasted_ck2_provs.begin(), wasted_ck2_provs.end(), sorter);
  sort(sorted_eu4_devs.begin(), sorted_eu4_devs.end(), sorter);
  sorted_ck2_provs.insert(sorted_ck2_provs.end(), wasted_ck2_provs.begin(),
                          wasted_ck2_provs.end());

  vector<CK2Province*> backup_ck2_provs;
  int counter = 0;
  int eu4Index = 0;
  static const string kNumProvsUsed("used_for_eu_provs");
  while (!unassignedEu4Provs.empty()) {
    for (auto* ck2prov : sorted_ck2_provs) {
      int index = ck2prov->safeGetInt(kNumProvsUsed, 0);
      if (index >= ck2prov->numEU4Provinces()) {
        continue;
      }
      ck2prov->resetLeaf(kNumProvsUsed, 1 + index);
      double ck2Weight = ck2prov->safeGetFloat(kDevSortKey);
      if (index + 1 < ck2prov->numEU4Provinces()) {
        ck2prov->resetLeaf(kDevSortKey, 0.5 * ck2Weight);
        backup_ck2_provs.push_back(ck2prov);
      }
      EU4Province* eu4prov = ck2prov->eu4Province(index);
      if (unassignedEu4Provs.count(eu4prov) == 0) {
        // Already assigned.
        continue;
      }
      if (eu4Index >= (int) sorted_eu4_devs.size()) {
        Logger::logStream(LogStream::Warn) << "Exiting devs " << eu4Index << "\n";
        break;
      }
      Object* eu4dev = sorted_eu4_devs[eu4Index++];
      if (!eu4dev) {
        Logger::logStream(LogStream::Warn) << "Null dev " << eu4Index << "\n";
        break;
      }
      Logger::logStream("provinces")
          << "Assigning " << nameAndNumber(eu4prov) << " development ("
          << eu4dev->safeGetString("base_tax") << ", "
          << eu4dev->safeGetString("base_production") << ", "
          << eu4dev->safeGetString("base_manpower") << ") from "
          << nameAndNumber(eu4dev) << " based on conversion from "
          << nameAndNumber(ck2prov) << " with weight "
          << ck2Weight << "\n";
      for (const auto& key : {"base_tax", "base_production", "base_manpower"}) {
        eu4prov->resetLeaf(key, eu4dev->safeGetFloat(key));
      }
      if (ck2prov->safeGetString("primary_settlement") == "\"---\"") {
        Logger::logStream("provinces")
            << nameAndNumber(eu4prov)
            << " wasted by nomads due to conversion from "
            << nameAndNumber(ck2prov) << "\n";
        for (const auto& key : {"base_tax", "base_production", "base_manpower"}) {
          eu4prov->resetLeaf(key, 1.0);
        }
        Object* modifier = new Object("modifier");
        eu4prov->setValue(modifier);
        modifier->setLeaf("modifier", "province_razed");
        modifier->setLeaf("date", getConversionDate(20));
        eu4prov->resetLeaf("loot_remaining", 0.0);
        eu4prov->resetLeaf("devastation", 50.0);
      }
      unassignedEu4Provs.erase(eu4prov);
    }
    if (++counter > 100) {
      Logger::logStream(LogStream::Warn)
          << "Still trying to assign development to "
          << unassignedEu4Provs.size()
          << " provinces. Breaking out of the loop.\n";
      break;
    }
    sorted_ck2_provs = backup_ck2_provs;
    backup_ck2_provs.clear();
    sort(sorted_ck2_provs.begin(), sorted_ck2_provs.end(), sorter);
  }

  return true;
}

bool Converter::modifyProvinces () {
  Logger::logStream(LogStream::Info) << "Beginning province modifications.\n" << LogOption::Indent;
  bool success = false;
  if (configObject->safeGetString("redistribute_dev", "no") == "yes") {
    Logger::logStream(LogStream::Info) << "Using province weight.\n";
    success = redistributeDevelopment();
  } else {
    Logger::logStream(LogStream::Info) << "Using province rank.\n";
    success = rankProvinceDevelopment();
  }
  Logger::logStream(LogStream::Info) << "Done modifying provinces.\n" << LogOption::Undent;
  return success;
}

// Apparently making this definition local to the function confuses the compiler.
struct WeightDescendingSorter {
  WeightDescendingSorter(ProvinceWeight const* const w,
                         map<CK2Province*, double>* adj)
      : weight(w), adjustment(adj) {}
  bool operator() (CK2Province* one, CK2Province* two) {
    double weightOne = one->getWeight(weight);
    if (adjustment->count(one)) weightOne *= adjustment->at(one);
    double weightTwo = two->getWeight(weight);
    if (adjustment->count(two)) weightTwo *= adjustment->at(two);
    return weightOne > weightTwo;
  }
  ProvinceWeight const* const weight;
  map<CK2Province*, double> const* const adjustment;
};

bool Converter::moveBuildings () {
  Logger::logStream(LogStream::Info) << "Moving buildings.\n" << LogOption::Indent;
  if (!euBuildingObject) {
    Logger::logStream(LogStream::Warn)
        << "No EU building info. Buildings will not be moved.\n"
        << LogOption::Undent;
    return true;
  }

  map<string, string> makesObsolete;
  map<string, string> nextLevel;
  map<string, Object*> euBuildingTypes;
  int fort_types = 0;

  objvec euBuildings = euBuildingObject->getLeaves();
  for (auto* euBuilding : euBuildings) {
    int numToBuild = euBuilding->safeGetInt("extra");
    string buildingTag = euBuilding->getKey();
    for (auto* eu4prov : EU4Province::getAll()) {
      if (!eu4prov->converts()) continue;
      if (!eu4prov->hasBuilding(buildingTag)) continue;
      numToBuild++;
      eu4prov->removeBuilding(buildingTag);
    }
    if (0 == numToBuild) continue;
    Logger::logStream("buildings") << "Found "
				   << numToBuild << " "
				   << buildingTag << ".\n";
    euBuilding->setLeaf("num_to_build", numToBuild);
    euBuildingTypes[buildingTag] = euBuilding;
    if (euBuilding->safeGetString("influencing_fort", "no") == "yes") {
      ++fort_types;
    }
    string obsolifies = euBuilding->safeGetString("make_obsolete", PlainNone);
    if (obsolifies != PlainNone) {
      makesObsolete[buildingTag] = obsolifies;
      nextLevel[obsolifies] = buildingTag;
    }
  }

  CK2Province::Container provList = CK2Province::makeCopy();
  map<CK2Province*, double> adjustment;
  for (CK2Province::Iter prov = provList.begin(); prov != provList.end(); ++prov) {
    adjustment[*prov] = 1;
    for (const auto& tag : euBuildingTypes) {
      (*prov)->unsetValue(tag.first);
    }
  }

  std::map<string, std::unordered_set<string>> fort_owners_in_states;
  std::map<string, string> fort_influence;
  Object* influence = configObject->getNeededObject("fort_influence");
  for (auto* p : influence->getLeaves()) {
    for (int i = 0; i < p->numTokens(); ++i) {
      fort_influence[p->getToken(i)] = p->getKey();
    }
  }
  while (!euBuildingTypes.empty()) {
    Object* building = 0;
    string buildingTag;
    for (const auto& tag : euBuildingTypes) {
      buildingTag = tag.first;
      if (nextLevel[buildingTag] != "") continue;
      building = tag.second;
      if (fort_types > 0 &&
          building->safeGetString("influencing_fort", "no") == "no") {
        continue;
      }
      break;
    }
    if (!building) {
      Logger::logStream(LogStream::Warn) << "Could not find a building that's "
                                            "not obsolete. This should never "
                                            "happen. Bailing.\n";
      break;
    }
    string obsolifies = makesObsolete[buildingTag];
    if (obsolifies != "") nextLevel[obsolifies] = "";
    int numToBuild = building->safeGetInt("num_to_build", 0);
    if (building->safeGetString("influencing_fort", "no") == "yes") {
      --fort_types;
    }
    euBuildingTypes.erase(buildingTag);
    if (0 == numToBuild) continue;
    string sortBy = building->safeGetString("sort_by", PlainNone);
    ProvinceWeight const* const weight = ProvinceWeight::findByName(sortBy);
    if (!weight) continue;
    sort(provList.begin(), provList.end(), WeightDescendingSorter(weight, &adjustment));
    for (int i = 0; i < numToBuild; ++i) {
      if (0 == provList[i]->numEU4Provinces()) continue;
      EU4Province* eu4prov = *(provList[i]->startEU4Province());
      string tradeGood = eu4prov->safeGetString("trade_goods");
      if ((tradeGood == "gold") &&
          (building->safeGetString("allow_in_gold_provinces") == "no")) {
        Logger::logStream("buildings")
            << "No " << buildingTag << " in " << nameAndNumber(eu4prov)
            << " due to gold.\n";
        continue;
      }
      Object* manu = building->safeGetObject("manufactory");
      bool badGood = false;
      if (manu) {
	for (int i = 0; i < manu->numTokens(); ++i) {
	  if (manu->getToken(i) != tradeGood) continue;
	  badGood = true;
	  break;
	}
      }
      if (badGood) {
        Logger::logStream("buildings")
            << "No " << buildingTag << " in " << nameAndNumber(eu4prov)
            << " due to " << tradeGood << ".\n";
        continue;
      }
      string owner = eu4prov->safeGetString("owner", PlainNone);
      if (owner == PlainNone) {
        Logger::logStream("buildings")
            << "No " << buildingTag << " in unowned province "
            << nameAndNumber(eu4prov) << "\n";
        continue;
      }

      string area = eu4prov->safeGetString(kStateKey, PlainNone);
      if (building->safeGetString("influencing_fort", "no") == "yes") {
        auto province_tag = eu4prov->getKey();
        auto influence_tag = fort_influence[province_tag];
        if (!influence_tag.empty()) {
          auto* influence_prov = EU4Province::findByName(influence_tag);
          if (influence_prov) {
            if (influence_prov->hasBuilding(buildingTag) &&
                influence_prov->safeGetString("owner") == owner) {
              Logger::logStream("buildings")
                  << "No " << buildingTag << " in province "
                  << nameAndNumber(eu4prov) << " because neighbour "
                  << nameAndNumber(influence_prov) << " already has one.\n";
              continue;
            }
          } else {
            Logger::logStream("buildings")
                << "Could not find province " << influence_tag
                << ", allegedly influencing " << nameAndNumber(eu4prov) << "\n";
          }
        }
        auto* priority = influence->safeGetObject(province_tag);
        if (priority) {
          bool moved = false;
          for (int i = 0; i < priority->numTokens(); ++i) {
            auto* other = EU4Province::findByName(priority->getToken(i));
            if (!other || other->safeGetString("owner") != owner ||
                !other->hasBuilding(buildingTag)) {
              continue;
            }
            Logger::logStream("buildings")
                << nameAndNumber(eu4prov) << " has higher priority for "
                << buildingTag << " than " << nameAndNumber(other)
                << ", moving from latter to former.\n";
            other->removeBuilding(buildingTag);
            fort_owners_in_states[other->safeGetString(kStateKey, PlainNone)]
                .erase(owner);
            moved = true;
            break;
          }
          if (moved) {
            adjustment[provList[i]] *= 0.75;
            eu4prov->addBuilding(buildingTag);
            fort_owners_in_states[area].insert(owner);
            continue;
          }
        }
        if (fort_owners_in_states[area].count(owner)) {
          Logger::logStream("buildings")
              << "No " << buildingTag << " in province " << nameAndNumber(eu4prov)
              << " because it is part of " << area
              << " which already has a fort.\n";
          continue;
        }
        
        fort_owners_in_states[area].insert(owner);
      }
      Logger::logStream("buildings") << "Creating " << buildingTag << " in "
				     << nameAndNumber(eu4prov)
				     << " with adjusted weight "
				     << provList[i]->getWeight(weight) * adjustment[provList[i]]
				     << ".\n";
      adjustment[provList[i]] *= 0.75;
      eu4prov->addBuilding(buildingTag);
    }
  }

  Logger::logStream(LogStream::Info) << "Done with buildings.\n" << LogOption::Undent;
  return true;
}

bool Converter::moveCapitals () {
  Logger::logStream(LogStream::Info) << "Moving capitals.\n" << LogOption::Indent;
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    CK2Ruler* ruler = (*eu4country)->getRuler();
    if (!ruler) continue;
    string eu4Tag = (*eu4country)->getKey();
    Object* demesne = ruler->getNeededObject(demesneString);
    string capitalTag = remQuotes(demesne->safeGetString("capital", QuotedNone));
    CK2Province* ckCapital = CK2Province::getFromBarony(capitalTag);
    if (ckCapital) {
      EU4Province* newCapital = 0;
      for (EU4Province::Iter eu4prov = ckCapital->startEU4Province(); eu4prov != ckCapital->finalEU4Province(); ++eu4prov) {
	EU4Country* owner = (*eu4prov)->getEU4Country();
	if (owner != (*eu4country)) continue;
	newCapital = (*eu4prov);
	break;
      }
      if (newCapital) {
	Logger::logStream("countries") << "Setting capital of "
				       << eu4Tag
				       << " to "
				       << nameAndNumber(newCapital)
				       << " based on capital "
				       << capitalTag
				       << " of "
				       << nameAndNumber(ruler)
				       << ".\n";
	(*eu4country)->resetLeaf("capital", newCapital->getKey());
	(*eu4country)->resetLeaf("original_capital", newCapital->getKey());
	(*eu4country)->resetLeaf("trade_port", newCapital->getKey());
	(*eu4country)->resetHistory("capital", newCapital->getKey());
	continue;
      }
    }
    Logger::logStream(LogStream::Warn) << "Could not find province for "
				       << capitalTag
				       << ", allegedly capital of "
				       << nameAndNumber(ruler)
				       << "; it may have converted owned by someone else. Picking random owned province for capital of "
				       << eu4Tag
				       << ".\n";
    for (EU4Province::Iter prov = (*eu4country)->startProvince(); prov != (*eu4country)->finalProvince(); ++prov) {
      Logger::logStream("countries") << "Setting capital of "
				     << eu4Tag
				     << " to "
				     << nameAndNumber(*prov)
				     << ".\n";
      (*eu4country)->resetLeaf("capital", (*prov)->getKey());
      (*eu4country)->resetLeaf("original_capital", (*prov)->getKey());
      (*eu4country)->resetHistory("capital", (*prov)->getKey());
      break;      
    }
  }
  Logger::logStream(LogStream::Info) << "Done with capitals.\n" << LogOption::Undent;
  return true;
}

bool Converter::redistributeMana () {
  Logger::logStream(LogStream::Info) << "Redistributing mana.\n" << LogOption::Indent;
  map<string, string> keywords;
  keywords["wealth"] = "treasury";
  keywords[prestigeString] = "prestige";
  keywords[kDynastyPower] = kAwesomePower;
  map<string, doublet> globalAmounts;
  map<string, double> maxima = {{prestigeString, 100}};

  map<CK2Ruler*, int> counts;
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    if (!(*eu4country)->converts()) continue;
    CK2Ruler* ruler = (*eu4country)->getRuler();
    counts[ruler]++;
    if (ruler->getPrimaryTitle() != (*eu4country)->getTitle()) continue;
    for (const auto& keyword : keywords) {
      string ck2word = keyword.first;
      string eu4word = keyword.second;
      double amount = ruler->safeGetFloat(ck2word);
      if (amount > 0) globalAmounts[ck2word].x() += amount;
      amount = (*eu4country)->safeGetFloat(eu4word);
      if (amount > 0) globalAmounts[ck2word].y() += amount;
    }
  }

  for (const auto& keyword : keywords) {
    string ck2word = keyword.first;
    string eu4word = keyword.second;
    Logger::logStream("mana")
        << "Redistributing " << globalAmounts[ck2word].y() << " EU4 " << eu4word
        << " to " << globalAmounts[ck2word].x() << " CK2 " << ck2word << ".\n"
        << LogOption::Indent;

    double ratio =
        globalAmounts[ck2word].y() / (1 + globalAmounts[ck2word].x());
    for (auto* eu4country : EU4Country::getAll()) {
      CK2Ruler *ruler = eu4country->getRuler();
      if (!ruler) {
        continue;
      }
      double ck2Amount = ruler->safeGetFloat(ck2word);
      if (ck2Amount <= 0) {
        continue;
      }

      double distribution = 1;
      if (counts[ruler] > 1) {
        if (ruler->getPrimaryTitle() == eu4country->getTitle()) {
          distribution = 0.666;
        } else {
          distribution = 0.333 / (counts[ruler] - 1);
        }
      }
      double eu4Amount = ck2Amount * ratio * distribution;
      if (maxima.find(ck2word) != maxima.end() && maxima[ck2word] < eu4Amount) {
        eu4Amount = maxima[ck2word];
      }
      Logger::logStream("mana")
          << nameAndNumber(ruler) << " has " << ck2Amount << " " << ck2word
          << ", so " << eu4country->getKey() << " gets " << eu4Amount << " "
          << eu4word << ".\n";
      eu4country->resetLeaf(eu4word, eu4Amount);
    }

    Logger::logStream("mana") << LogOption::Undent;
  }


  objvec factions = ck2Game->getValue("active_faction");
  map<CK2Ruler*, vector<CK2Ruler*> > factionMap;
  for (auto* faction : factions) {
    Object* scope = faction->getNeededObject("scope");
    string creatorId = scope->safeGetString("char");
    CK2Ruler* creator = CK2Ruler::findByName(creatorId);
    CK2Ruler* target = 0;
    Object* targetTitle = scope->safeGetObject("title");
    if (targetTitle) {
      string titleTag;
      // Handle both "title = whatever" and "title = { title = whatever }" cases.
      if (targetTitle->isLeaf()) {
        titleTag = remQuotes(targetTitle->getLeaf());
      } else {
        titleTag = remQuotes(targetTitle->safeGetString("title"));
      }
      CK2Title* title = CK2Title::findByName(titleTag);
      if (title) {
        target = title->getRuler();
      }
    }
    if ((!target) && (creator)) target = creator->getLiege();
    if (!target) continue;
    if (creator) factionMap[target].push_back(creator);

    objvec backers = faction->getValue("backer");
    for (objiter backer = backers.begin(); backer != backers.end(); ++backer) {
      CK2Ruler* rebel = CK2Ruler::findByName((*backer)->getLeaf());
      if (rebel) factionMap[target].push_back(rebel);
    }
  }

  double maxClaimants = 0;
  for (auto* eu4country : EU4Country::getAll()) {
    if (!eu4country->converts()) continue;
    CK2Ruler* ruler = eu4country->getRuler();
    CK2Title* primary = ruler->getPrimaryTitle();
    double claimants = 0;
    for (CK2Character::CharacterIter claimant = primary->startClaimant();
         claimant != primary->finalClaimant(); ++claimant) {
      ++claimants;
    }
    if (claimants > maxClaimants) {
      maxClaimants = claimants;
    }
    eu4country->resetLeaf("claimants", claimants);
  }

  double minimumLegitimacy = configObject->safeGetFloat("minimum_legitimacy", 25);
  for (auto* eu4country : EU4Country::getAll()) {
    int totalPower = eu4country->safeGetFloat(kAwesomePower);
    eu4country->unsetValue(kAwesomePower);
    double claimants = eu4country->safeGetFloat("claimants");
    eu4country->unsetValue("claimants");
    if (!eu4country->converts()) continue;
    CK2Ruler* ruler = eu4country->getRuler();
    Object* powers = eu4country->getNeededObject("powers");
    powers->clear();
    if (customObject) {
      auto* scores = customObject->safeGetObject("custom_score");
      if (scores &&
          scores->safeGetInt(ruler->safeGetString(dynastyString, PlainNone),
                             -1000) != -1000) {
        Logger::logStream("mana")
            << nameAndNumber(ruler) << " is of custom dynasty "
            << getDynastyName(ruler) << ", giving flat mana.\n";
        totalPower = 300;
      }
    }
    totalPower /= 3;
    Logger::logStream("mana") << eu4country->getKey() << " gets "
                              << totalPower << " of each mana.\n";
    powers->addToList(totalPower);
    powers->addToList(totalPower);
    powers->addToList(totalPower);

    double totalFactionStrength = 0;
    for (auto* rebel : factionMap[ruler]) {
      totalFactionStrength += rebel->countBaronies();
    }
    totalFactionStrength /= (1 + ruler->countBaronies());
    int stability = 3;
    Logger::logStream("mana")
        << nameAndNumber(ruler) << " has faction strength "
        << totalFactionStrength;
    totalFactionStrength *= 6; // Half rebels, stability 0.
    stability -= (int) floor(totalFactionStrength);
    if (stability < -3) stability = -3;
    eu4country->resetLeaf("stability", stability);
    Logger::logStream("mana") << " so " << eu4country->getKey()
                              << " has stability " << stability << ".\n";

    eu4country->resetLeaf("legitimacy", "0.000");
    if (eu4country->getGovernmentType() == "\"republic\"") {
      eu4country->resetLeaf("republican_tradition", "100.000");
    }
    else {
      Logger::logStream("mana")
          << claimants << " claims on " << ruler->getPrimaryTitle()->getKey()
          << ", hence ";
      claimants /= maxClaimants;
      double legitimacy =
          100 * (1.0 - claimants + 0.01 * minimumLegitimacy * claimants);
      eu4country->resetLeaf("legitimacy", legitimacy);
      Logger::logStream("mana")
          << eu4country->getKey() << " has legitimacy " << legitimacy << ".\n";
    }
  }

  keywords.clear();
  keywords["autonomy"] = "local_autonomy";
  globalAmounts.clear();
  
  for (CK2Province::Iter ck2prov = CK2Province::start(); ck2prov != CK2Province::final(); ++ck2prov) {
    CK2Title* title = (*ck2prov)->getCountyTitle();
    if (!title) continue;
    int distance = 0;
    int dejure = 0;
    int culture = 0;
    int religion = 0;
    CK2Ruler* ruler = title->getRuler();
    CK2Ruler* countyRuler = ruler;
    while ((ruler) && (!ruler->getEU4Country())) {
      ++distance;
      ruler = ruler->getLiege();
    }

    if (!ruler) {
      distance = 3;
      dejure = 3;
      culture = 1;
      religion = 1;
    }
    else {
      CK2Ruler* liege = title->getRuler();
      while ((liege) && (liege != ruler)) {
	++dejure;
	title = title->getDeJureLiege();
	if (title) liege = title->getRuler();
	else liege = 0;
      }

      if (countyRuler) {
	if (ruler != countyRuler) {
	  if (ruler->getBelief("culture") != countyRuler->getBelief("culture")) culture = 1;
	  if (ruler->getBelief("religion") != countyRuler->getBelief("religion")) religion = 1;
	}
      }
      else {
	culture = religion = 1;
      }
    }

    double autonomy = distance + dejure + culture + religion;
    Logger::logStream("mana") << nameAndNumber(*ck2prov) << " gets "
			      << autonomy << " autonomy from "
			      << distance << " vassal distance, "
			      << dejure << " dejure distance, "
			      << culture << " culture difference, "
			      << religion << " religion difference.\n";
    (*ck2prov)->resetLeaf("autonomy", autonomy);
  }

  for (EU4Province::Iter eu4prov = EU4Province::start(); eu4prov != EU4Province::final(); ++eu4prov) {
    if (0 == (*eu4prov)->numCKProvinces()) continue;
    for (CK2Province::Iter ck2prov = (*eu4prov)->startProv(); ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
      for (map<string, string>::iterator keyword = keywords.begin(); keyword != keywords.end(); ++keyword) {
	string ck2word = keyword->first;
	string eu4word = keyword->second;
	double amount = (*ck2prov)->safeGetFloat(ck2word);
	if (amount > 0) globalAmounts[ck2word].x() += amount;
	amount = (*eu4prov)->safeGetFloat(eu4word);
	if (amount > 0) globalAmounts[ck2word].y() += amount;
      }
    }
  }

  for (map<string, string>::iterator keyword = keywords.begin(); keyword != keywords.end(); ++keyword) {
    string ck2word = keyword->first;
    string eu4word = keyword->second;      
    Logger::logStream("mana") << "Redistributing " << globalAmounts[ck2word].y() << " EU4 " << eu4word
			      << " across " << globalAmounts[ck2word].x() << " CK2 " << ck2word << ".\n";
    for (EU4Province::Iter eu4prov = EU4Province::start(); eu4prov != EU4Province::final(); ++eu4prov) {
      if (0 == (*eu4prov)->numCKProvinces()) continue;
      double current = 0;
      for (CK2Province::Iter ck2prov = (*eu4prov)->startProv(); ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
	double amount = (*ck2prov)->safeGetFloat(ck2word);
	if (amount > 0) current += (*ck2prov)->safeGetFloat(ck2word);
      }
      current /= globalAmounts[ck2word].x();
      current *= globalAmounts[ck2word].y();
      (*eu4prov)->resetLeaf(eu4word, current);
    }
  }

  vector<string> techAreas;
  techAreas.push_back("mil_tech");
  techAreas.push_back("adm_tech");
  techAreas.push_back("dip_tech");
  for (unsigned int i = 0; i < techAreas.size(); ++i) {
    string eu4tech = techAreas[i];
    vector<int> eu4Values;
    CK2Ruler::Container rulers;
    for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
      if (!(*eu4country)->converts()) continue;
      CK2Ruler* ruler = (*eu4country)->getRuler();
      rulers.push_back(ruler);
      Object* tech = (*eu4country)->getNeededObject("technology");
      eu4Values.push_back(tech->safeGetInt(eu4tech, 3));

      double ckTech = 0;
      int provinces = 0;
      for (EU4Province::Iter eu4prov = (*eu4country)->startProvince(); eu4prov != (*eu4country)->finalProvince(); ++eu4prov) {
	for (CK2Province::Iter ck2prov = (*eu4prov)->startProv(); ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
	  ++provinces;
	  tech = (*ck2prov)->safeGetObject("technology");
	  if (!tech) continue;
	  tech = tech->safeGetObject("tech_levels");
	  if (!tech) continue;
	  for (unsigned int idx = i*6; idx < (i+1)*6; ++idx) {
	    ckTech += tech->tokenAsFloat(idx);
	  }
	}
      }
      if (0 == provinces) continue;
      ckTech /= provinces;
      ruler->resetLeaf("tech_value", ckTech);
    }

    sort(eu4Values.begin(), eu4Values.end()); // NB, ascending order.
    reverse(eu4Values.begin(), eu4Values.end());
    while (eu4Values.size() < rulers.size()) eu4Values.push_back(eu4Values.back());
    sort(rulers.begin(), rulers.end(), ObjectDescendingSorter("tech_value"));
    int previous = 1e6;
    for (unsigned int idx = 0; idx < rulers.size(); ++idx) {
      int curr = eu4Values[idx];
      CK2Ruler* ruler = rulers[idx];
      EU4Country* eu4country = ruler->getEU4Country();
      if (previous > curr) {
	Logger::logStream("mana") << nameAndNumber(ruler) << " at index " << idx << " with "
				  << ruler->safeGetFloat("tech_value") << " " << eu4tech << " points gives "
				  << eu4country->getKey() << " level " << curr << ".\n";
      }
      previous = curr;
      Object* tech = eu4country->getNeededObject("technology");
      tech->resetLeaf(eu4tech, curr);
    }
  }

  Logger::logStream(LogStream::Info) << "Done with mana.\n" << LogOption::Undent;
  return true;
}

bool Converter::resetHistories () {
  Logger::logStream(LogStream::Info) << "Clearing histories and flags.\n" << LogOption::Indent;
  vector<string> objectsToClear;
  objectsToClear.push_back("history");
  objectsToClear.push_back("cores");
  vector<string> valuesToRemove;
  valuesToRemove.push_back("unit");
  for (auto* eu4prov : EU4Province::getAll()) {
    if (!eu4prov->converts()) {
      continue;
    }

    for (auto tag : objectsToClear) {
      Object* obj = eu4prov->getNeededObject(tag);
      obj->clear();
    }

    for (auto tag : valuesToRemove) {
      eu4prov->unsetValue(tag);
    }
    Object* history = eu4prov->getNeededObject("history");
    history->resetLeaf("discovered_by", addQuotes("western"));

    Object* discovered = eu4prov->safeGetObject("discovered_dates");
    if ((discovered) && (1 < discovered->numTokens())) {
      discovered->resetToken(1, "1.1.1");
    }
    discovered = eu4prov->safeGetObject("discovered_by");
    if (discovered) {
      vector<string> extraTags;
      for (int i = 0; i < discovered->numTokens(); ++i) {
	string tag = discovered->getToken(i);
	if (EU4Country::findByName(tag)) continue;
	extraTags.push_back(tag);
      }
      discovered->clear();
      for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
	if (!(*eu4country)->getRuler()) continue;
	discovered->addToList((*eu4country)->getKey());
      }
      for (vector<string>::iterator tag = extraTags.begin(); tag != extraTags.end(); ++tag) {
	discovered->addToList(*tag);
      }
    }
  }

  for (auto* eu4country : EU4Country::getAll()) {
    if (!eu4country->converts()) {
      continue;
    }
    eu4country->getNeededObject("history")->clear();
  }
  
  Logger::logStream(LogStream::Info) << "Done with clearing.\n" << LogOption::Undent;
  return true;
}

bool Converter::setCores () {
  // Cores on the following provinces:
  // 1. You hold a barony in the province (including the province itself).
  // 2. You hold the de-jure duchy.
  // 3. The de-jure kingdom or empire is your primary title.
  // Claims on:
  // 2. You have a CK claim on its de-jure liege, or hold the title.

  // Cores on every owned province and on de-jure vassals.
  
  Logger::logStream(LogStream::Info) << "Beginning cores and claims.\n" << LogOption::Indent;

  map<EU4Province*, map<EU4Country*, int> > claimsMap;
  for (CK2Province::Iter ck2prov = CK2Province::start(); ck2prov != CK2Province::final(); ++ck2prov) {
    Logger::logStream("cores") << "Seeking core for "
			       << nameAndNumber(*ck2prov)
			       << ".\n"
			       << LogOption::Indent;
    CK2Title* title = (*ck2prov)->getCountyTitle();
    while (title) {
      Logger::logStream("cores") << "Looking at title " << title->getKey();
      CK2Ruler* ruler = title->getRuler();
      if (ruler) {
	Logger::logStream("cores") << " with ruler " << nameAndNumber(ruler);
	EU4Country* country = ruler->getEU4Country();
	if (country) {
	  Logger::logStream("cores") << " and country " << country->getKey();
	  CK2Title* primary = ruler->getPrimaryTitle();
	  // Claims due to holding de-jure liege title.
	  for (auto* eu4prov : (*ck2prov)->eu4Provinces()) {
	    claimsMap[eu4prov][country]++;
	  }
	  if ((primary == title) || (*(title->getLevel()) < *TitleLevel::Kingdom)) {
	    Logger::logStream("cores") << ".\n" << LogOption::Indent;
	    for (auto* eu4prov : (*ck2prov)->eu4Provinces()) {
	      Logger::logStream("cores") << nameAndNumber(eu4prov)
					 << " is core of "
					 << country->getKey()
					 << " because of "
					 << title->getKey()
					 << ".\n";
	      country->setAsCore(eu4prov);
	    }
	    Logger::logStream("cores") << LogOption::Undent;
	  }
	  else {
	    Logger::logStream("cores") << " but level is " << title->getLevel()->getName()
				       << " and primary is "
				       << (primary ? primary->getKey() : string("none"))
				       << ".\n";
	  }
	}
	else {
	  Logger::logStream("cores") << " who has no EU4 country.\n";
	}
      }
      else {
	Logger::logStream("cores") << " which has no ruler.\n";
      }

      for (CK2Character::CharacterIter claimant = title->startClaimant(); claimant != title->finalClaimant(); ++claimant) {
	EU4Country* eu4country = (*claimant)->getEU4Country();
	if (!eu4country) continue;
	for (EU4Province::Iter eu4prov = (*ck2prov)->startEU4Province(); eu4prov != (*ck2prov)->finalEU4Province(); ++eu4prov) {
	  claimsMap[*eu4prov][eu4country]++;
	}
      }

      title = title->getDeJureLiege();
    }
    for (objiter barony = (*ck2prov)->startBarony();
         barony != (*ck2prov)->finalBarony(); ++barony) {
      CK2Title* baronyTitle = CK2Title::findByName((*barony)->getKey());
      if (!baronyTitle) continue;
      CK2Ruler* baron = baronyTitle->getRuler();
      if (!baron) continue;
      EU4Country* eu4country = baron->getEU4Country();
      if (eu4country) {
	for (EU4Province::Iter eu4prov = (*ck2prov)->startEU4Province(); eu4prov != (*ck2prov)->finalEU4Province(); ++eu4prov) {
	  Logger::logStream("cores") << nameAndNumber(*eu4prov)
				     << " is core of "
				     << eu4country->getKey()
				     << " because of "
				     << baronyTitle->getKey()
				     << ".\n";
	  eu4country->setAsCore(*eu4prov);
	}
      }
      // Iterate up the liege list for claims.
      CK2Title* title = baronyTitle;
      while (title) {
	CK2Ruler* ruler = title->getRuler();
	eu4country = ruler->getEU4Country();
	if (eu4country) {
	  for (EU4Province::Iter eu4prov = (*ck2prov)->startEU4Province(); eu4prov != (*ck2prov)->finalEU4Province(); ++eu4prov) {
	    claimsMap[*eu4prov][eu4country]++;
	  }
	}
	title = title->getLiege();
      }
    }
    Logger::logStream("cores") << LogOption::Undent;    
  }

  for (map<EU4Province*, map<EU4Country*, int> >::iterator claim = claimsMap.begin(); claim != claimsMap.end(); ++claim) {
    EU4Province* eu4prov = claim->first;
    for (map<EU4Country*, int>::iterator strength = claim->second.begin(); strength != claim->second.end(); ++strength) {
      EU4Country* eu4country = strength->first;
      if (eu4prov->hasCore(eu4country->getKey())) continue;
      int numClaims = strength->second;
      if (1 > numClaims) continue;
      int startYear = year(eu4Game->safeGetString("date", "1444.1.1"));
      int claimYear = startYear - 25;
      claimYear += numClaims * 5;
      if (claimYear > startYear) claimYear = startYear;
      string date = createString("%i.1.1", claimYear);
      Object* history = eu4prov->getNeededObject("history");
      Object* add_claim = new Object(date);
      history->setValue(add_claim);
      add_claim->setLeaf("add_claim", addQuotes(eu4country->getKey()));
    }
  }

  Logger::logStream(LogStream::Info) << "Done with cores and claims.\n" << LogOption::Undent;
  return true;
}

bool Converter::setupDiplomacy () {
  Logger::logStream(LogStream::Info) << "Beginning diplomacy.\n" << LogOption::Indent;
  Object* euDipObject = eu4Game->getNeededObject("diplomacy");
  euDipObject->clear();
  vector<string> keyWords;
  keyWords.push_back("friends");
  keyWords.push_back("subjects");
  for (EU4Country::Iter eu4 = EU4Country::start(); eu4 != EU4Country::final(); ++eu4) {
    for (vector<string>::iterator key = keyWords.begin(); key != keyWords.end(); ++key) {
      Object* toClear = (*eu4)->getNeededObject(*key);
      toClear->clear();
    }
    (*eu4)->unsetValue("luck");
    (*eu4)->unsetValue("is_subject");
    (*eu4)->unsetValue("is_lesser_in_union");
    (*eu4)->unsetValue("overlord");
    (*eu4)->unsetValue("previous_monarch");
    (*eu4)->resetLeaf("num_of_subjects", 0);
    (*eu4)->resetLeaf("num_of_allies", 0);
    (*eu4)->resetLeaf("num_of_royal_marriages", 0);

    Object* active_relations = (*eu4)->getNeededObject("active_relations");
    objvec relations = active_relations->getLeaves();
    for (objiter relation = relations.begin(); relation != relations.end(); ++relation) {
      (*relation)->clear();
    }
  }

  string startDate = eu4Game->safeGetString("date", "1444.1.1");

  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final();
       ++ruler) {
    EU4Country* vassalCountry = (*ruler)->getEU4Country();
    if (!vassalCountry) continue;
    if ((*ruler)->isSovereign()) continue;
    CK2Ruler* liege = (*ruler)->getSovereignLiege();
    if (!liege) continue;
    EU4Country* liegeCountry = liege->getEU4Country();
    if (liegeCountry == vassalCountry) continue;
    Object* vassal = new Object("dependency");
    euDipObject->setValue(vassal);
    vassal->setLeaf("first", addQuotes(liegeCountry->getName()));
    vassal->setLeaf("second", addQuotes(vassalCountry->getName()));
    vassal->setLeaf("subject_type", addQuotes("vassal"));
    vassalCountry->resetLeaf("liberty_desire", "0.000");

    for (auto& key : keyWords) {
      Object* toAdd = liegeCountry->getNeededObject(key);
      toAdd->addToList(addQuotes(vassalCountry->getName()));
    }
    
    Logger::logStream("diplomacy") << vassalCountry->getName()
				   << " is vassal of "
				   << liegeCountry->getName()
				   << " based on characters "
				   << nameAndNumber(*ruler)
				   << " and "
				   << nameAndNumber(liege)
				   << ".\n";
  }

  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    CK2Title* primary = (*ruler)->getPrimaryTitle();
    EU4Country* overlord = primary->getEU4Country();
    if (!overlord) continue;
    for (CK2Title::Iter title = (*ruler)->startTitle(); title != (*ruler)->finalTitle(); ++title) {
      if ((*title) == primary) continue;
      EU4Country* subject = (*title)->getEU4Country();
      if (!subject) continue;
      if (subject == overlord) continue;
      Logger::logStream("diplomacy") << subject->getName()
				     << " is lesser in union with "
				     << overlord->getName() << " due to "
				     << nameAndNumber(*ruler)
				     << ".\n";
      subject->resetLeaf("overlord", addQuotes(overlord->getKey()));
      subject->resetLeaf("liberty_desire", "0.000");
      Object* unionObject = new Object("union");
      euDipObject->setValue(unionObject);
      unionObject->setLeaf("first", addQuotes(overlord->getKey()));
      unionObject->setLeaf("second", addQuotes(subject->getKey()));
      unionObject->setLeaf("start_date",
                           remQuotes((*ruler)->safeGetString(
                               birthDateString, addQuotes(startDate))));
      unionObject->setLeaf("subject_type", addQuotes("personal_union"));

      for (vector<string>::iterator key = keyWords.begin(); key != keyWords.end(); ++key) {
	Object* toAdd = overlord->getNeededObject(*key);
	toAdd->addToList(addQuotes(subject->getName()));
      }
    }
  }

  Logger::logStream(LogStream::Info) << "Done with diplomacy.\n" << LogOption::Undent;
  return true;
}
  
bool Converter::transferProvinces () {
  Logger::logStream(LogStream::Info) << "Beginning province transfer.\n" << LogOption::Indent;

  bool debugNames = (configObject->safeGetString("debug_names", "no") == "yes");
  std::unordered_set<EU4Province*> deferred;
  for (auto* eu4prov : EU4Province::getAll()) {
    if (0 == eu4prov->numCKProvinces()) continue; // ROTW or water.
    map<EU4Country*, double> weights;
    double rebelWeight = 0;
    string debugName;
    for (auto* ck2Prov : eu4prov->ckProvs()) {
      CK2Title* countyTitle = ck2Prov->getCountyTitle();
      if (!countyTitle) {
	Logger::logStream(LogStream::Warn) << "Could not find county title of CK province "
					   << ck2Prov->getName()
					   << ", ignoring.\n";
	continue;
      }
      CK2Ruler* ruler = countyTitle->getRuler();
      if (!ruler) {
	Logger::logStream(LogStream::Warn) << "County " << countyTitle->getName()
					   << " has no CK ruler? Ignoring.\n";
	continue;
      }
      CK2Ruler* sovereign = countyTitle->getSovereign();
      EU4Country* eu4country = 0;
      if (sovereign) {
	CK2Title* primary = sovereign->getPrimaryTitle();
	CK2Title* titleToUse = primary;
	for (CK2Title::Iter title = sovereign->startTitle(); title != sovereign->finalTitle(); ++title) {
	  if (primary == (*title)) continue;
	  if (!(*title)->isDeJureOverlordOf(countyTitle)) continue;
	  if (!(*title)->getEU4Country()) continue;
	  titleToUse = (*title);
	  break;
	}
	eu4country = titleToUse->getEU4Country();
        Logger::logStream("provinces")
            << countyTitle->getName() << " assigned to "
            << eu4country->getName() << " due to "
            << (titleToUse == primary ? "regular liege chain to primary "
                                      : "being de-jure in union title ")
            << titleToUse->getName() << ".\n";
      }
      if (!eu4country) {
        // Look for tributary overlord.
	CK2Ruler* suzerain = ruler->getSuzerain();
	while (suzerain) {
	  if (suzerain->getEU4Country()) break;
	  suzerain = suzerain->getSuzerain();
	}
	if ((suzerain) && (suzerain->getEU4Country())) {
	  eu4country = suzerain->getEU4Country();
	  Logger::logStream("provinces") << countyTitle->getName()
					 << " assigned to "
					 << eu4country->getName()
					 << " due to tributary overlordship.\n";
	}
      }
      if (!eu4country) {
	// A rebel?
	for (CK2Ruler::Iter enemy = ruler->startEnemy(); enemy != ruler->finalEnemy(); ++enemy) {
	  eu4country = (*enemy)->getEU4Country();
	  if (eu4country) break;
	}
	if (eu4country) {
	  rebelWeight += ck2Prov->getWeight(ProvinceWeight::Manpower);
	  Logger::logStream("provinces") << countyTitle->getName()
					 << " assigned "
					 << eu4country->getName()
					 << " from war - assumed rebel.\n";
	}
      }
      if (!eu4country) {
	// Same dynasty.
	CK2Ruler* biggest = 0;
	string dynasty = ruler->safeGetString(dynastyString, "dsa");
	for (CK2Ruler::Iter cand = CK2Ruler::start(); cand != CK2Ruler::final(); ++cand) {
	  if (!(*cand)->getEU4Country()) continue;
	  if (dynasty != (*cand)->safeGetString(dynastyString, "fds")) continue;
	  if ((*cand) == ruler) continue;
	  if ((biggest) && (biggest->countBaronies() > (*cand)->countBaronies())) continue;
	  biggest = (*cand);
	}
	if (biggest) {
	  eu4country = biggest->getEU4Country();
	  Logger::logStream("provinces") << countyTitle->getName()
					 << " assigned "
					 << eu4country->getName()
					 << " from same dynasty.\n";
	}
      }
      if (!eu4country) continue;
      weights[eu4country] += ck2Prov->getWeight(ProvinceWeight::Manpower);
      debugName +=
          createString("%s (%.1f) ", countyTitle->getName().c_str(),
                       ck2Prov->getWeight(ProvinceWeight::Manpower));
    }
    if (0 == weights.size()) {
      Logger::logStream(LogStream::Warn) << "Could not find any candidates for "
					 << nameAndNumber(eu4prov)
					 << "; deferring.\n";
      deferred.insert(eu4prov);
      continue;
    }
    EU4Country* best = 0;
    double highest = -1;
    for (const auto& cand : weights) {
      if (cand.second < highest) continue;
      best = cand.first;
      highest = cand.second;
    }
    Logger::logStream("provinces") << nameAndNumber(eu4prov) << " assigned to "
                                   << best->getName() << "\n";
    if (debugNames) {
      eu4prov->resetLeaf("name", addQuotes(debugName));
    }
    eu4prov->assignCountry(best);
    best->setAsCore(eu4prov);
  }

  if (!deferred.empty()) {
    Logger::logStream("provinces")
        << deferred.size() << " deferred provinces:\n";
  }
  for (auto* eu4prov : deferred) {
    auto& area_provinces = area_province_map[eu4prov->safeGetString(kStateKey)];
    unordered_map<string, int> weights;
    for (auto* other_prov : area_provinces) {
      if (deferred.find(other_prov) != deferred.end()) {
        continue;
      }
      weights[remQuotes(other_prov->safeGetString("owner", QuotedNone))]++;
    }
    string ownerTag = PlainNone;
    int highest = -1;
    for (const auto& cand : weights) {
      if (cand.second < highest)
        continue;
      ownerTag = cand.first;
      highest = cand.second;
    }
    EU4Country* best = nullptr;
    if (ownerTag == PlainNone) {
      ownerTag = remQuotes(eu4prov->safeGetString("owner"));
      Logger::logStream(LogStream::Warn)
          << "Could not find any candidates for " << nameAndNumber(eu4prov)
          << " even after deferring; giving up and converting as owned by "
             "input owner "
          << ownerTag << ".\n";
    } else {
      Logger::logStream("provinces")
          << nameAndNumber(eu4prov) << " assigned to " << ownerTag << "\n";
    }
    best = EU4Country::findByName(ownerTag);
    if (best) {
      eu4prov->assignCountry(best);
      best->setAsCore(eu4prov);
    }
  }

  Logger::logStream(LogStream::Info) << "Done with province transfer.\n" << LogOption::Undent;
  return true;
}

bool Converter::warsAndRebels () {
  Logger::logStream(LogStream::Info) << "Beginning wars.\n" << LogOption::Indent;
  objvec euWars = eu4Game->getValue("active_war");
  for (objiter euWar = euWars.begin(); euWar != euWars.end(); ++euWar) {
    vector<string> convertingTags;
    objvec attackers = (*euWar)->getValue("attacker");
    for (objiter attacker = attackers.begin(); attacker != attackers.end(); ++attacker) {
      string tag = remQuotes((*attacker)->getLeaf());
      EU4Country* eu4country = EU4Country::findByName(tag);
      if (!eu4country->converts()) continue;
      convertingTags.push_back(tag);
    }
    objvec defenders = (*euWar)->getValue("defender");
    for (objiter defender = defenders.begin(); defender != defenders.end(); ++defender) {
      string tag = remQuotes((*defender)->getLeaf());
      EU4Country* eu4country = EU4Country::findByName(tag);
      if (!eu4country->converts()) continue;
      convertingTags.push_back(tag);
    }
    if (convertingTags.empty()) continue;
    Logger::logStream("war") << "Removing " << (*euWar)->safeGetString("name")
			     << " because of participants";
    for (vector<string>::iterator tag = convertingTags.begin();
         tag != convertingTags.end(); ++tag) {
      Logger::logStream("war") << (*tag) << " ";
    }
    Logger::logStream("war") << "\n";
    eu4Game->removeObject(*euWar);
  }

  bool addParticipants = false;
  bool joined_war = true;
  Object* dlcs = eu4Game->getNeededObject("dlc_enabled");
  for (int i = 0; i < dlcs->numTokens(); ++i) {
    string dlc = remQuotes(dlcs->getToken(i));
    if ((dlc == "Art of War") || (dlc == "Common Sense") ||
        (dlc == "Conquest of Paradise") || (dlc == "The Cossacks")) {
      addParticipants = true;
    }
    if (dlc == "The Cossacks") {
      joined_war = false;
    }
  }

  Object* before = eu4Game->getNeededObject("income_statistics");
  CK2War::Container rebelCandidates;
  for (CK2War::Iter ckWar = CK2War::start(); ckWar != CK2War::final(); ++ckWar) {
    string warName = (*ckWar)->safeGetString("name", QuotedNone);
    objvec ckDefenders = (*ckWar)->getValue("defender");
    EU4Country::Container euDefenders;
    for (objiter ckDefender = ckDefenders.begin(); ckDefender != ckDefenders.end(); ++ckDefender) {
      string defenderId = (*ckDefender)->getLeaf();
      CK2Ruler* defender = CK2Ruler::findByName(defenderId);
      if (!defender) continue;
      EU4Country* eu4Defender = defender->getEU4Country();
      if (!eu4Defender) continue;
      if (!eu4Defender->converts()) continue;
      euDefenders.push_back(eu4Defender);
    }
    if (euDefenders.empty()) {
      Logger::logStream("war")
          << "Skipping " << warName << " because no defenders converted.\n";
      continue;
    }

    objvec ckAttackers = (*ckWar)->getValue("attacker");
    EU4Country::Container euAttackers;
    for (objiter ckAttacker = ckAttackers.begin(); ckAttacker != ckAttackers.end(); ++ckAttacker) {
      string attackerId = (*ckAttacker)->getLeaf();
      CK2Ruler* attacker = CK2Ruler::findByName(attackerId);
      if (!attacker) continue;
      EU4Country* eu4Attacker = attacker->getEU4Country();
      if (!eu4Attacker) continue;
      if (!eu4Attacker->converts()) continue;
      euAttackers.push_back(eu4Attacker);
    }
    if (euAttackers.empty()) {
      Logger::logStream("war")
          << "Skipping " << warName << " because no attackers converted.\n";
      rebelCandidates.push_back(*ckWar);
      continue;
    }

    Object* ck2cb = (*ckWar)->safeGetObject("casus_belli");
    if (!ck2cb) {
      Logger::logStream("war") << "Skipping " << warName << " because no CB.\n";
      continue;
    }

    Object* disputedTitle = ck2cb->safeGetObject("landed_title");
    if (!disputedTitle) {
      Logger::logStream("war")
          << "Skipping " << warName << " because no disputed title.\n";
      rebelCandidates.push_back(*ckWar);
      continue;
    }

    string disputedTag =
        remQuotes(disputedTitle->safeGetString("title", QuotedNone));
    if (disputedTag == PlainNone && disputedTitle->isLeaf()) {
      disputedTag = disputedTitle->getLeaf();
    }
    CK2Title* title = CK2Title::findByName(disputedTag);
    if (!title) {
      Logger::logStream("war")
          << "Skipping " << warName << ", could not identify disputed title "
          << disputedTag << ".\n";
      continue;
    }

    CK2Ruler* attackedRuler = title->getSovereign();
    if (attackedRuler) {
      EU4Country* attackedCountry = attackedRuler->getEU4Country();
      if ((attackedCountry) &&
	  (find(euAttackers.begin(), euAttackers.end(), attackedCountry) == euAttackers.end()) &&
	  (find(euDefenders.begin(), euDefenders.end(), attackedCountry) == euDefenders.end())) {
	euDefenders.push_back(attackedCountry);
      }
    }

    Object* euWar = new Object("active_war");
    euWar->setLeaf("name", warName);
    Object* history = euWar->getNeededObject("history");
    history->setLeaf("name", warName);
    string startDateString = eu4Game->safeGetString("date", "1444.1.1");
    euWar->setLeaf("action", startDateString);
    Object* startDate = history->getNeededObject(startDateString);
    for (EU4Country::Iter eu4attacker = euAttackers.begin(); eu4attacker != euAttackers.end(); ++eu4attacker) {
      euWar->setLeaf("attacker", addQuotes((*eu4attacker)->getKey()));
      if (joined_war) euWar->setLeaf("joined_war", addQuotes((*eu4attacker)->getKey()));
      euWar->setLeaf("original_attacker", addQuotes((*eu4attacker)->getKey()));
      startDate->setLeaf("add_attacker", addQuotes((*eu4attacker)->getKey()));
      if (addParticipants) {
	Object* participant = new Object("participants");
	euWar->setValue(participant);
	participant->setLeaf("value", "1.000");
	participant->setLeaf("tag", addQuotes((*eu4attacker)->getKey()));
      }
    }
    string targetProvince = "";
    CK2Province* fromBarony = 0;
    if (TitleLevel::Barony == title->getLevel()) fromBarony = CK2Province::getFromBarony(disputedTag);
    for (EU4Country::Iter eu4defender = euDefenders.begin(); eu4defender != euDefenders.end(); ++eu4defender) {
      euWar->setLeaf("defender", addQuotes((*eu4defender)->getKey()));
      if (joined_war) euWar->setLeaf("joined_war", addQuotes((*eu4defender)->getKey()));
      euWar->setLeaf("original_defender", addQuotes((*eu4defender)->getKey()));
      startDate->setLeaf("add_defender", addQuotes((*eu4defender)->getKey()));
      if (addParticipants) {
	Object* participant = new Object("participants");
	euWar->setValue(participant);
	participant->setLeaf("value", "1.000");
	participant->setLeaf("tag", addQuotes((*eu4defender)->getKey()));
      }

      if (targetProvince != "") continue;
      for (EU4Province::Iter eu4prov = (*eu4defender)->startProvince(); eu4prov != (*eu4defender)->finalProvince(); ++eu4prov) {
	bool useThisProvince = false;
	for (CK2Province::Iter ck2prov = (*eu4prov)->startProv(); ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
	  if (fromBarony == (*ck2prov)) {
	    useThisProvince = true;
	    break;
	  }
	  CK2Title* countyTitle = (*ck2prov)->getCountyTitle();
	  if (countyTitle == title) {
	    useThisProvince = true;
	    break;
	  }
	  if (countyTitle->getDeJureLevel(title->getLevel()) == title) {
	    useThisProvince = true;
	    break;
	  }
	  while (countyTitle) {
	    countyTitle = countyTitle->getLiege();
	    if (countyTitle == title) {
	      useThisProvince = true;
	      break;
	    }
	  }
	  if (useThisProvince) break;
	}
	if (!useThisProvince) continue;
	targetProvince = (*eu4prov)->getKey();
	break;
      }
    }

    if (targetProvince == "") {
      Logger::logStream("war")
          << "Skipping " << warName << ", could not find province from "
          << disputedTag << ".\n";
      continue;
    }

    string ckCasusBelli = remQuotes(
        (*ckWar)->getNeededObject("casus_belli")->safeGetString("casus_belli"));
    Object* wargoal = history->getNeededObject("war_goal");

    if (ckCasusBelli == "crusade") {
      Logger::logStream("war")
          << "Converting " << warName << " as crusade.\n";
      wargoal->setLeaf("type", "\"superiority_crusade\"");
      wargoal->setLeaf("casus_belli", "\"cb_crusade\"");
      Object* superiority = euWar->getNeededObject("superiority");
      superiority->setLeaf("type", "\"superiority_crusade\"");
      superiority->setLeaf("casus_belli", "\"cb_crusade\"");
    } else {
      Logger::logStream("war")
          << "Converting " << warName << " with target province "
          << targetProvince << ".\n";
      wargoal->setLeaf("type", "\"take_claim\"");
      wargoal->setLeaf("province", targetProvince);
      wargoal->setLeaf("casus_belli", "\"cb_conquest\"");
      Object* takeProvince = euWar->getNeededObject("take_province");
      takeProvince->setLeaf("type", "\"take_claim\"");
      takeProvince->setLeaf("province", targetProvince);
      takeProvince->setLeaf("casus_belli", "\"cb_conquest\"");
    }

    eu4Game->setValue(euWar, before);
  }

  Object* rebelCountry = eu4Game->getNeededObject("countries")->safeGetObject("REB");
  if (!rebelCountry) {
    Logger::logStream(LogStream::Info)
        << "Could not find rebel country, no rebels or heresies.\n"
        << LogOption::Undent;
    return true;
  }

  objvec factions = eu4Game->getValue("rebel_faction");
  objvec rebel_leaders = rebelCountry->getValue("leader");
  objvec rebel_armies = rebelCountry->getValue("army");
  for (auto* faction : factions) {
    EU4Country* country = EU4Country::findByName(
        remQuotes(faction->safeGetString("country", QuotedNone)));
    if (!country) continue;
    if (country->isROTW()) continue;
    Object* rebel_leader = faction->safeGetObject("leader");

    if (rebel_leader) {
      int rebel_leader_id = rebel_leader->safeGetInt("id");
      for (auto* leader : rebel_leaders) {
        if (rebel_leader_id != leader->safeGetInt("id")) continue;
        rebelCountry->removeObject(leader);
      }
      for (auto* army : rebel_armies) {
        Object* army_leader = army->safeGetObject("leader");
        if (army_leader && rebel_leader_id != army_leader->safeGetInt("id")) {
          continue;
        }
        rebelCountry->removeObject(army);
      }
    }

    auto provinces = faction->getNeededObject("possible_provinces");
    int faction_id = faction->getNeededObject("id")->safeGetInt("id");
    for (int i = 0; i < provinces->numTokens(); ++i) {
      auto* province = EU4Province::findByName(provinces->getToken(i));
      if (province == nullptr) continue;
      auto prov_rebels = province->getValue("rebel_faction");
      for (auto* rebel_id : prov_rebels) {
        if (rebel_id->safeGetInt("id") != faction_id) continue;
        province->removeObject(rebel_id);
      }
    }
    eu4Game->removeObject(faction);
  }

  Object* cbConversion = configObject->getNeededObject("rebel_faction_types");
  before = eu4Game->safeGetObject("religions");
  string birthDate = eu4Game->safeGetString("date", "1444.11.11");
  objvec generalSkills = configObject->getNeededObject("generalSkills")->getLeaves();

  for (auto* cand : rebelCandidates) {
    string ckCasusBelli =
        remQuotes(cand->getNeededObject("casus_belli")
                      ->safeGetString("casus_belli", QuotedNone));
    string euRevoltType = cbConversion->safeGetString(ckCasusBelli, PlainNone);
    string warName = cand->safeGetString("name");
    if (euRevoltType == PlainNone) {
      Logger::logStream("war")
          << "Not making " << warName << " a rebellion due to CB "
          << ckCasusBelli << ".\n";
      continue;
    }
    EU4Country* revolter = nullptr;
    if (euRevoltType == "nationalist_rebels") {
      if (independenceRevolts.empty()) {
        Logger::logStream("war")
            << "Skipping " << warName
            << " as rebellion due to lack of revolter tags.\n";
        continue;
      }
      revolter = *independenceRevolts.begin();
      independenceRevolts.erase(revolter);
    }

    objvec ckDefenders = cand->getValue("defender");
    EU4Country* target = 0;
    for (objiter ckDefender = ckDefenders.begin(); ckDefender != ckDefenders.end(); ++ckDefender) {
      string defenderId = (*ckDefender)->getLeaf();
      CK2Ruler* defender = CK2Ruler::findByName(defenderId);
      if (!defender) continue;
      EU4Country* eu4Defender = defender->getEU4Country();
      if (!eu4Defender) continue;
      if (!eu4Defender->converts()) continue;
      target = eu4Defender;
      break;
    }
    if (!target) {
      Logger::logStream("war")
          << "Skipping " << warName
          << " as rebellion because no defenders converted.\n";
      continue;
    }

    Object* faction = new Object("rebel_faction");
    Object* factionId = createTypedId("rebel", "50");
    faction->setValue(factionId);
    faction->setLeaf("type", addQuotes(euRevoltType));
    faction->setLeaf("name", warName);
    faction->setLeaf("progress", "0.000");
    string euReligion = target->safeGetString("religion");
    faction->setLeaf("heretic",
                     configObject->getNeededObject("rebel_heresies")
                         ->safeGetString(euReligion, "Lollard"));
    faction->setLeaf("country", addQuotes(target->getKey()));
    faction->setLeaf("independence", revolter ? addQuotes(revolter->getKey()) : "\"\"");
    faction->setLeaf("sponsor", "\"---\"");
    faction->setLeaf("religion", addQuotes(euReligion));
    faction->setLeaf("government", addQuotes(target->safeGetString("government")));
    faction->setLeaf("province", target->safeGetString("capital"));
    faction->setLeaf("seed", "931983089");
    CK2Ruler* ckRebel = CK2Ruler::findByName(cand->safeGetString("attacker"));
    if (!ckRebel) {
      Logger::logStream("war") << "Skipping " << warName << " for lack of a leader.\n";
      continue;
    }
    Logger::logStream("war") << "Converting " << warName << " as rebel type "
                             << euRevoltType << ".\n";
    if (revolter) {
      Logger::logStream("war")
          << "Independence target is " << revolter->getKey() << ".\n";
    }
    eu4Game->setValue(faction, before);
    Object* general = createLeader(ckRebel, "general", generalSkills, birthDate);
    general->setKey("general");
    faction->setValue(general);
    Object* id = createTypedId("leader", "49");
    general->setValue(id);
    Object* leader = new Object(id);
    leader->setKey("leader");
    faction->setValue(leader);
    rebelCountry->setValue(leader);

    Object* armyId = createUnitId("50");
    Object* factionArmyId = new Object(armyId);
    factionArmyId->setKey("army");
    faction->setValue(factionArmyId);
    Object* army = new Object("army");
    army->setValue(armyId);
    rebelCountry->setValue(army);
    army->setLeaf("name", warName);
    army->setValue(leader);
    string rebelLocation = target->safeGetString("capital");
    double rebelTroops = 0;
    Object* rebelDemesne = ckRebel->getNeededObject(demesneString);
    string capTag =
        remQuotes(rebelDemesne->safeGetString("capital", QuotedNone));
    CK2Province* capital = CK2Province::getFromBarony(capTag);
    if (capital && capital->numEU4Provinces() > 0) {
      EU4Province* euCapital = capital->eu4Province(0);
      rebelLocation = euCapital->getKey();
    }
    vector<CK2Ruler*> rebels = {ckRebel};
    std::set<CK2Ruler*> safety;  // CK2 has weird edge cases.
    Logger::logStream("war") << warName << " gets troops:\n" << LogOption::Indent;
    while (!rebels.empty()) {
      auto* currentRebel = rebels.back();
      rebels.pop_back();
      for (auto* vassal : currentRebel->getVassals()) {
        if (safety.count(vassal)) {
          continue;
        }
        safety.insert(vassal);
        rebels.push_back(vassal);
      }
      for (auto* title : currentRebel->getTitles()) {
        auto* ck2prov = CK2Province::getFromBarony(title->getKey());
        if (ck2prov == nullptr) {
          continue;
        }
        Object* barony = ck2prov->safeGetObject(title->getKey());
        if (!barony) {
          continue;
        }
        Object* levy = barony->safeGetObject("levy");
        if (!levy) {
          continue;
        }
        double distanceFactor =
            1.0 / (1 + title->distanceToSovereign());
        double amount = calculateTroopWeight(levy, nullptr);
        amount *= distanceFactor;
        rebelTroops += amount;
        Logger::logStream("war") << amount << " from " << title->getKey() << "\n";
      }
      rebelDemesne = currentRebel->safeGetObject(demesneString);
      if (!rebelDemesne) {
        continue;
      }
      objvec rebelArmies = rebelDemesne->getValue("army");
      for (auto* rebelArmy : rebelArmies) {
        objvec units = rebelArmy->getValue("sub_unit");
        double armyTroops = 0;
        for (auto* unit : units) {
          armyTroops +=
              calculateTroopWeight(unit->getNeededObject("troops"), nullptr);
        }
        rebelTroops += armyTroops;
        Logger::logStream("war")
            << armyTroops << " from " << rebelArmy->safeGetString("name") << "\n";
      }
    }
    army->setLeaf("location", rebelLocation);
    int rebelRegiments = (int)floor(
        rebelTroops * configObject->safeGetFloat("regimentsPerTroop") + 0.5);
    if (rebelRegiments < 3) {
      rebelRegiments = 3;
    }
    Logger::logStream("war") << "Total " << rebelTroops << " giving "
                             << rebelRegiments << " regiments.\n"
                             << LogOption::Undent;
    for (int i = 0; i < rebelRegiments; ++i) {
      Object* regiment = new Object("regiment");
      army->setValue(regiment);
      regiment->setValue(createUnitId("50"));
      regiment->setLeaf("name", "\"Rebel Regiment\"");
      regiment->setLeaf("home", rebelLocation);
      regiment->setLeaf("type", configObject->safeGetString("infantry_type", "\"western_medieval_infantry\""));
      regiment->setLeaf("morale", "2.000");
      regiment->setLeaf("strength", "1.000");
    }

    Object* provinces = faction->getNeededObject("provinces");
    Object* possibles = faction->getNeededObject("possible_provinces");
    provinces->setObjList();
    possibles->setObjList();
    Object* provinceFactionId = new Object(factionId);
    provinceFactionId->setKey("rebel_faction");
    Object* occupationId = new Object(factionId);
    occupationId->setKey("occupying_rebel_faction");
    if (revolter) {
      revolter->unsetValue("core_provinces");
    }
    for (auto* eu4prov : target->getProvinces()) {
      double occupation = 0;
      double total = 0;
      for (auto* ck2prov : eu4prov->ckProvs()) {
        total++;
        auto* title = ck2prov->getCountyTitle();
        while (title) {
          if (title->getRuler() == ckRebel) {
            occupation++;
            break;
          }
          title = title->getLiege();
        }
      }
      if (occupation > 0) {
        possibles->addToList(eu4prov->getKey());
      }
      if (occupation / total < 0.5) {
        continue;
      }
      if (revolter) {
        revolter->setAsCore(eu4prov);
      }
      provinces->addToList(eu4prov->getKey());
      eu4prov->resetLeaf("controller", "\"REB\"");
      eu4prov->setValue(provinceFactionId);
      eu4prov->setValue(occupationId);
      Object* occupation_date = new Object(eu4Game->safeGetString("date", "1444.1.1"));
      eu4prov->getNeededObject("history")->setValue(occupation_date);
      Object* controller = new Object("controller");
      controller->setLeaf("tag", "\"REB\"");
      occupation_date->setValue(controller);
    }
  }

  Object* religions = configObject->safeGetObject("dynamicReligions");
  if (!religions) {
    Logger::logStream(LogStream::Warn)
        << "Could not find dynamic religions, skipping heretic rebels.\n";
  } else {
    for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
      if (!(*eu4country)->converts()) continue;
      Logger::logStream("war")
          << "Searching for heresies in " << (*eu4country)->getKey() << ":\n"
          << LogOption::Indent;
      CK2Ruler* ruler = (*eu4country)->getRuler();
      string ckReligion = ruler->getBelief("religion");
      string euReligion = religions->safeGetString(ckReligion);
      set<EU4Province*> eu4Provinces;
      for (EU4Province::Iter eu4prov = (*eu4country)->startProvince();
           eu4prov != (*eu4country)->finalProvince(); ++eu4prov) {
        for (CK2Province::Iter ck2prov = (*eu4prov)->startProv();
             ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
          string provCkReligion = (*ck2prov)->safeGetString("religion");
	  string provEuReligion = religions->safeGetString(provCkReligion);
	  if ((provEuReligion == euReligion) && (provCkReligion != ckReligion)) {
	    Logger::logStream("war") << nameAndNumber(*eu4prov) << " based on " << provCkReligion
				     << " in " << nameAndNumber(*ck2prov) << ".\n";
	    eu4Provinces.insert(*eu4prov);
	  }
	}
      }

      if (eu4Provinces.empty()) {
        Logger::logStream("war") << LogOption::Undent;
        continue;
      }
      Object* faction = new Object("rebel_faction");
      Object* factionId = createTypedId("rebel", "50");
      faction->setValue(factionId);
      faction->setLeaf("type", "heretic_rebels");
      string heresy = configObject->getNeededObject("rebel_heresies")->safeGetString(euReligion, "Lollard");
      faction->setLeaf("name", addQuotes(remQuotes(heresy) + " Rebels"));
      faction->setLeaf("progress", "0.000");
      faction->setLeaf("heretic", heresy);
      faction->setLeaf("country", addQuotes((*eu4country)->getKey()));
      faction->setLeaf("independence", "\"\"");
      faction->setLeaf("sponsor", "\"---\"");
      faction->setLeaf("religion", addQuotes(euReligion));
      faction->setLeaf("government", addQuotes((*eu4country)->safeGetString("government")));
      faction->setLeaf("province", (*(eu4Provinces.begin()))->getKey());
      faction->setLeaf("seed", "931983089");
      Object* provinces = faction->getNeededObject("provinces");
      Object* possibles = faction->getNeededObject("possible_provinces");
      provinces->setObjList();
      possibles->setObjList();
      Object* provinceFactionId = new Object(factionId);
      provinceFactionId->setKey("rebel_faction");
      for (set<EU4Province*>::iterator eu4prov = eu4Provinces.begin(); eu4prov != eu4Provinces.end(); ++eu4prov) {
	provinces->addToList((*eu4prov)->getKey());
	possibles->addToList((*eu4prov)->getKey());
	(*eu4prov)->setValue(provinceFactionId);
      }
      faction->setLeaf("active", "no");
      eu4Game->setValue(faction, before);
      Logger::logStream("war")
          << "Created heretic rebel faction " << faction->safeGetString("name") << "\n"
          << LogOption::Undent;
    }
  }

  Logger::logStream(LogStream::Info) << "Done with wars.\n" << LogOption::Undent;
  return true;
}

/******************************* End conversions ********************************/

/*******************************  Begin calculators ********************************/

double calcAvg (Object* ofthis) {
  if (!ofthis) return 0; 
  int num = ofthis->numTokens();
  if (0 == num) return 0;
  double ret = 0;
  for (int i = 0; i < num; ++i) {
    ret += ofthis->tokenAsFloat(i);
  }
  ret /= num;
  return ret; 
}

/******************************* End calculators ********************************/

void Converter::convert () {
  if (!ck2Game) {
    Logger::logStream(LogStream::Info) << "No file loaded.\n";
    return; 
  }

  loadFiles();
  // Last leaf needs special treatment.
  objvec leaves = eu4Game->getLeaves();
  Object* final = leaves.back();
  eu4Game->removeObject(final);
  ofstream* debug = Logger::getLogFile();

  if (debug) (*debug) << "createCK2Objects" << std::endl;
  if (!createCK2Objects()) return;
  if (debug) (*debug) << "createEU4Objects" << std::endl;
  if (!createEU4Objects()) return;
  if (debug) (*debug) << "createProvinceMap" << std::endl;
  if (!createProvinceMap()) return;
  if (debug) (*debug) << "createCountryMap" << std::endl;
  if (!createCountryMap()) return;
  if (debug) (*debug) << "resetHistories" << std::endl;
  if (!resetHistories()) return;
  if (debug) (*debug) << "calculateProvinceWeights" << std::endl;
  if (!calculateProvinceWeights()) return;
  if (debug) (*debug) << "transferProvinces" << std::endl;
  if (!transferProvinces()) return;
  if (debug) (*debug) << "setCores" << std::endl;
  if (!setCores()) return;
  if (debug) (*debug) << "moveCapitals" << std::endl;
  if (!moveCapitals()) return;
  if (debug) (*debug) << "modifyProvinces" << std::endl;
  if (!modifyProvinces()) return;
  if (debug) (*debug) << "setupDiplomacy" << std::endl;
  if (!setupDiplomacy()) return;
  if (debug) (*debug) << "adjustBalkanisation" << std::endl;
  if (!adjustBalkanisation()) return;
  if (debug) (*debug) << "moveBuildings" << std::endl;
  if (!moveBuildings()) return;
  if (debug) (*debug) << "cleanEU4Nations" << std::endl;
  if (!cleanEU4Nations()) return;
  if (debug) (*debug) << "createArmies" << std::endl;
  if (!createArmies()) return;
  if (debug) (*debug) << "createNavies" << std::endl;
  if (!createNavies()) return;
  if (debug) (*debug) << "cultureAndReligion" << std::endl;
  if (!cultureAndReligion()) return;
  if (debug) (*debug) << "createGovernments" << std::endl;
  if (!createGovernments()) return;
  if (debug) (*debug) << "createCharacters" << std::endl;
  if (!createCharacters()) return;
  if (debug) (*debug) << "redistributeMana" << std::endl;
  if (!redistributeMana()) return;
  if (debug) (*debug) << "hreAndPapacy" << std::endl;
  if (!hreAndPapacy()) return;
  if (debug) (*debug) << "warsAndRebels" << std::endl;
  if (!warsAndRebels()) return;
  if (debug) (*debug) << "displayStats" << std::endl;
  displayStats();

  if (debug) (*debug) << "calculateDynasticScores" << std::endl;
  calculateDynasticScores();
  if (debug) (*debug) << "cleanUp" << std::endl;
  cleanUp();
  if (debug) (*debug) << "Done, writing" << std::endl;

  Logger::logStream(LogStream::Info) << "Done with conversion, writing to Output/converted.eu4.\n";
  Parser::EqualsSign = "="; // No whitespace around equals, thanks Paradox.
  ofstream writer;
  writer.open(".\\Output\\converted.eu4");
  Parser::topLevel = eu4Game;
  writer << "EU4txt\n"; // Gah, don't ask me...
  writer << (*eu4Game);
  // No closing endline, thanks Paradox.
  writer << final->getKey() << "=" << final->getLeaf();
  Logger::logStream(LogStream::Info) << "Done writing.\n";
}

