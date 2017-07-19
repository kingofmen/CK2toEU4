#include <algorithm>
#include <cstdio>
#include <direct.h>
#include <deque>
#include <iostream> 
#include <string>
#include <set>
#include <unordered_map>

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
ConverterJob const *const ConverterJob::CheckProvinces =
    new ConverterJob("check_provinces", false);
ConverterJob const *const ConverterJob::LoadFile =
    new ConverterJob("loadfile", true);

const string kDynastyPower = "dynasty_power";
const string kAwesomePower = "awesome_power";

const string kOldDemesne = "demesne";
const string kNewDemesne = "dmn";
string demesneString;

const string kOldTradePost = "tradepost";
const string kNewTradePost = "trade_post";
string tradePostString;

const string kOldBirthName = "birth_name";
const string kNewBirthName = "bn";
string birthNameString;

Converter::Converter (Window* ow, string fn)
  : ck2FileName(fn)
  , ck2Game(0)
  , eu4Game(0)
  , ckBuildingObject(0)
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
  while (true) {
    if (jobsToDo.empty()) {
      sleep(5);
      continue;
    }

    ConverterJob const* const job = jobsToDo.front();
    jobsToDo.pop();
    if (ConverterJob::Convert        == job) convert();
    if (ConverterJob::DebugParser    == job) debugParser();
    if (ConverterJob::LoadFile       == job) loadFile();
    if (ConverterJob::CheckProvinces == job) checkProvinces();
  }
}

void Converter::loadFile () {
  if (ck2FileName == "") return;
  Parser::ignoreString = "CK2txt";
  Parser::specialCases["de_jure_liege=}"] = "";
  ck2Game = loadTextFile(ck2FileName);
  Parser::ignoreString = "";
  Parser::specialCases.clear();
  Logger::logStream(LogStream::Info) << "Ready to convert.\n";
}

void Converter::cleanUp () {
  // Restore the initial '-' in province tags.
  string minus("-");
  for (EU4Province::Iter prov = EU4Province::start(); prov != EU4Province::final(); ++prov) {
    (*prov)->object->setKey(minus + (*prov)->getKey());
    (*prov)->unsetValue("fort_level");
    (*prov)->unsetValue("base_fort_level");
    (*prov)->unsetValue("influencing_fort");
    (*prov)->unsetValue("fort_influencing");
    (*prov)->unsetValue("estate");
  }

  string infantryType = configObject->safeGetString("infantry_type", "\"western_medieval_infantry\"");
  string cavalryType = configObject->safeGetString("cavalry_type", "\"western_medieval_knights\"");
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    (*eu4country)->unsetValue("needs_heir");
    (*eu4country)->unsetValue(EU4Country::kNoProvinceMarker);
    if (!(*eu4country)->getRuler()) continue;
    (*eu4country)->resetLeaf("infantry", infantryType);
    (*eu4country)->resetLeaf("cavalry", cavalryType);
    (*eu4country)->resetLeaf("army_tradition", "0.000");
    (*eu4country)->resetLeaf("navy_tradition", "0.000");
    (*eu4country)->resetLeaf("papal_influence", "0.000");
    (*eu4country)->resetLeaf("mercantilism", (*eu4country)->safeGetString("government") == "merchant_republic" ? "0.250" : "0.100");
    (*eu4country)->resetLeaf("last_bankrupt", "1.1.1");
    (*eu4country)->resetLeaf("last_focus_move", "1.1.1");
    (*eu4country)->resetLeaf("last_conversion", "1.1.1");
    (*eu4country)->resetLeaf("last_sacrifice", "1.1.1");
    (*eu4country)->resetLeaf("last_debate", "1.1.1");
    (*eu4country)->resetLeaf("wartax", "1.1.1");
    (*eu4country)->resetLeaf("update_opinion_cache", "yes");
    (*eu4country)->resetLeaf("needs_refresh", "yes");
    (*eu4country)->resetLeaf("manpower", "10.000");
    (*eu4country)->resetLeaf("technology_group", "western");
    (*eu4country)->resetHistory("technology_group", "western");
    objvec estates = (*eu4country)->getValue("estate");
    for (objiter estate = estates.begin(); estate != estates.end(); ++estate) {
      (*estate)->unsetValue("province");
      (*estate)->resetLeaf("loyalty", "40.000");
      (*estate)->resetLeaf("total_loyalty", "40.000");
      (*estate)->resetLeaf("influence", "40.000");
      (*estate)->resetLeaf("province_influence", "0.000");
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

void Converter::debugParser() {
  objvec parsed = ck2Game->getLeaves();
  Logger::logStream(LogStream::Info) << "Last parsed object:\n"
                                     << parsed.back();
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

string nameAndNumber (ObjectWrapper* prov) {
  return prov->getKey() + " (" + remQuotes(prov->safeGetString("name", "\"could not find name\"")) + ")";
}

string nameAndNumber (CK2Character* ruler) {
  return ruler->safeGetString(birthNameString) + " (" + ruler->getKey() + ")";
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
  if (!createCK2Objects())
    return;
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
      break;
    }
    if (newObject) {
      break;
    }
  }
}

/********************************  End helpers  **********************/

/******************************** Begin initialisers **********************/

bool Converter::createCK2Objects () {
  Logger::logStream(LogStream::Info) << "Creating CK2 objects\n" << LogOption::Indent;
  Object* wrapperObject = ck2Game->safeGetObject("provinces");
  if (!wrapperObject) {
    Logger::logStream(LogStream::Error)
        << "Could not find provinces object, cannot continue.\n"
        << LogOption::Undent;
    return false;
  }

  objvec provinces = wrapperObject->getLeaves();
  tradePostString = kNewTradePost;
  for (objiter province = provinces.begin(); province != provinces.end();
       ++province) {
    new CK2Province(*province);
    if (tradePostString == kNewTradePost &&
        (*province)->safeGetObject(kOldTradePost) != nullptr) {
      Logger::logStream(LogStream::Info)
          << "Detected old trade-post string " << kOldTradePost
          << ". Proceeding with that.\n";
      tradePostString = kOldTradePost;
    }
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
    string deJureTag = PlainNone;
    if (deJureObj) deJureTag = remQuotes(deJureObj->safeGetString("title", QuotedNone));
    if ((deJureTag == PlainNone) && (deJureMap.count((*ckCountry)->getName()))) {
      deJureTag = deJureMap[(*ckCountry)->getName()];
    }
    if (deJureTag == PlainNone) {
      continue;
    }
    (*ckCountry)->setDeJureLiege(CK2Title::findByName(deJureTag));
  }

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

  Object* dynasties = ck2Game->safeGetObject("dynasties");
  for (CK2Title::Iter ckCountry = CK2Title::start(); ckCountry != CK2Title::final(); ++ckCountry) {
    string holderId = (*ckCountry)->safeGetString("holder", PlainNone);
    if (PlainNone == holderId) continue;
    CK2Ruler* ruler = CK2Ruler::findByName(holderId);
    if (!ruler) {
      Object* character = characters->safeGetObject(holderId);
      if (!character) {
	Logger::logStream(LogStream::Warn) << "Could not find character " << holderId
					   << ", holder of title " << (*ckCountry)->getKey()
					   << ". Ignoring.\n";
	continue;
      }
      ruler = new CK2Ruler(character, dynasties);
    }

    ruler->addTitle(*ckCountry);
  }

  Logger::logStream(LogStream::Info) << "Created " << CK2Ruler::totalAmount() << " CK2 rulers.\n";
  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    (*ruler)->createLiege();
  }

  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    (*ruler)->countBaronies();
    (*ruler)->createClaims();
  }

  map<string, int> dynastyPower;
  objvec allChars = characters->getLeaves();
  for (objiter ch = allChars.begin(); ch != allChars.end(); ++ch) {
    if ((*ch)->safeGetString("d_d", PlainNone) != PlainNone) {
      // Dead character, check for dynasty power.
      string dynastyId = (*ch)->safeGetString("dynasty", PlainNone);
      if (PlainNone == dynastyId) continue;
      objvec holdings = (*ch)->getValue("old_holding");
      for (objiter holding = holdings.begin(); holding != holdings.end(); ++holding) {
	CK2Title* title = CK2Title::findByName(remQuotes((*holding)->getLeaf()));
	if (!title) continue;
	dynastyPower[dynastyId] += (1 + *title->getLevel());
      }
      continue;
    }

    objvec claims = (*ch)->getValue("claim");    
    CK2Ruler* father = CK2Ruler::findByName((*ch)->safeGetString("father"));
    CK2Ruler* mother = CK2Ruler::findByName((*ch)->safeGetString("mother"));
    CK2Ruler* employer = 0;
    if (((*ch)->safeGetString("job_title", PlainNone) != PlainNone) ||
	((*ch)->safeGetString("title", PlainNone) == "\"title_commander\"") ||
	((*ch)->safeGetString("title", PlainNone) == "\"title_high_admiral\"")) {
      employer = CK2Ruler::findByName((*ch)->safeGetString("host"));
    }
    if ((!father) && (!mother) && (!employer) && (0 == claims.size())) continue;
    CK2Character* current = new CK2Character((*ch), dynasties);
    if (father) father->personOfInterest(current);
    if (mother) mother->personOfInterest(current);
    if (employer) employer->personOfInterest(current);
    current->createClaims();
  }

  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    string dynastyId = (*ruler)->safeGetString("dynasty", PlainNone);
    if (PlainNone == dynastyId) continue;
    (*ruler)->resetLeaf(kDynastyPower, dynastyPower[dynastyId]);
  }
  
  // Diplomacy
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
    Logger::logStream(LogStream::Error) << "Could not find provinces object, cannot continue.\n" << LogOption::Undent;
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

  wrapperObject = eu4Game->safeGetObject("countries");
  if (!wrapperObject) {
    Logger::logStream(LogStream::Error) << "Could not find countries object, cannot continue.\n" << LogOption::Undent;
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

void convertTitle (CK2Title* title, CK2Ruler* ruler, map<EU4Country*, vector<EU4Province*> >& initialProvincesMap, set<EU4Country*>& candidateCountries) {
  if (title->getEU4Country()) return;
  EU4Country* bestCandidate = 0;
  vector<pair<string, string> > bestOverlapList;
  for (set<EU4Country*>::iterator cand = candidateCountries.begin(); cand != candidateCountries.end(); ++cand) {
    vector<EU4Province*> eu4Provinces = initialProvincesMap[*cand];
    vector<pair<string, string> > currOverlapList;
    for (vector<EU4Province*>::iterator eu4Province = eu4Provinces.begin(); eu4Province != eu4Provinces.end(); ++eu4Province) {
      for (CK2Province::Iter ck2Prov = (*eu4Province)->startProv(); ck2Prov != (*eu4Province)->finalProv(); ++ck2Prov) {
	CK2Title* countyTitle = (*ck2Prov)->getCountyTitle();
	if (title->isDeJureOverlordOf(countyTitle)) {
	  currOverlapList.push_back(pair<string, string>(countyTitle->getName(), nameAndNumber(*eu4Province)));
	}
      }
    }
    if ((0 < bestOverlapList.size()) && (currOverlapList.size() <= bestOverlapList.size())) continue;
    bestCandidate = (*cand);
    bestOverlapList = currOverlapList;
  }
  if (3 > bestOverlapList.size()) {
    for (set<EU4Country*>::iterator cand = candidateCountries.begin(); cand != candidateCountries.end(); ++cand) {
      vector<EU4Province*> eu4Provinces = initialProvincesMap[*cand];
      vector<pair<string, string> > currOverlapList;
      for (vector<EU4Province*>::iterator eu4Province = eu4Provinces.begin(); eu4Province != eu4Provinces.end(); ++eu4Province) {
	for (CK2Province::Iter ck2Prov = (*eu4Province)->startProv(); ck2Prov != (*eu4Province)->finalProv(); ++ck2Prov) {
	  CK2Title* countyTitle = (*ck2Prov)->getCountyTitle();
	  if (ruler->hasTitle(countyTitle, true)) {
	    currOverlapList.push_back(pair<string, string>(countyTitle->getName(), nameAndNumber(*eu4Province)));
	  }
	}
      }
      if ((0 < bestOverlapList.size()) && (currOverlapList.size() <= bestOverlapList.size())) continue;
      bestCandidate = (*cand);
      bestOverlapList = currOverlapList;
    }
  }
  
  Logger::logStream("countries") << nameAndNumber(ruler) << " as ruler of " << title->getName()
				 << " converts to " << bestCandidate->getKey()
				 << " based on overlap " << bestOverlapList.size()
				 << " with these counties: ";
  for (vector<pair<string, string> >::iterator overlap = bestOverlapList.begin(); overlap != bestOverlapList.end(); ++overlap) {
    Logger::logStream("countries") << (*overlap).first << " -> " << (*overlap).second << " ";
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
      Logger::logStream(LogStream::Warn) << "Attempted override "
					 << cktag << " -> "
					 << eutag << ", but could not find CK title. Ignoring.\n";
      continue;
    }
    EU4Country* euCountry = EU4Country::findByName(eutag);
    if (!euCountry) {
      Logger::logStream(LogStream::Warn) << "Attempted override "
					 << cktag << " -> "
					 << eutag << ", but could not find EU country. Ignoring.\n";
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

  for (map<EU4Country*, vector<EU4Province*> >::iterator eu4 = initialProvincesMap.begin(); eu4 != initialProvincesMap.end(); ++eu4) {
    if (forbiddenCountries.count((*eu4).first)) continue;
    string badProvinceTag = "";
    bool hasGoodProvince = false;
    for (vector<EU4Province*>::iterator prov = (*eu4).second.begin(); prov != (*eu4).second.end(); ++prov) {
      if (0 < (*prov)->numCKProvinces()) hasGoodProvince = true;
      else {
	badProvinceTag = (*prov)->getKey();
	break;
      }
    }
    if (badProvinceTag != "") {
      Logger::logStream("countries") << "Disregarding " << (*eu4).first->getKey() << " because it owns " << badProvinceTag << "\n";
      forbiddenCountries.insert((*eu4).first);
    }
    else if (hasGoodProvince) {
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
    Logger::logStream(LogStream::Error) << "Found " << candidateCountries.size()
					<< " EU4 countries; cannot continue with so few.\n" << LogOption::Undent;
    return false;
  }

  vector<vector<CK2Ruler*> > rulers(TitleLevel::totalAmount());
  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    CK2Title* primary = (*ruler)->getPrimaryTitle();
    if (!primary) continue;
    rulers[*primary->getLevel()].push_back(*ruler);
  }

  int maxUnions = configObject->safeGetInt("max_unions", 2);
  map<CK2Ruler*, int> unions;
  int maxSmallVassals = configObject->safeGetInt("max_vassals_per_kingdom", 3);
  bool subInfeudate = (configObject->safeGetString("permit_subinfeudation") == "yes");
  map<CK2Ruler*, map<CK2Title*, int> > totalVassals;

  const unsigned int kSovereignOnly = 0;
  for (TitleLevel::rIter level = TitleLevel::rstart(); level != TitleLevel::rfinal(); ++level) {
    for (unsigned int doVassals = kSovereignOnly; doVassals < 2; ++doVassals) {
      bool sovereignsOnly = (doVassals == kSovereignOnly);
      for (vector<CK2Ruler*>::iterator ruler = rulers[**level].begin(); ruler != rulers[**level].end(); ++ruler) {
	CK2Title* primary = (*ruler)->getPrimaryTitle();
	Logger::logStream("countries") << nameAndNumber(*ruler) << " of " << primary->getName() << "\n";
	if (!primary) {
	  // Should not happen.
	  Logger::logStream("countries") << "Skipping " << nameAndNumber(*ruler)
					 << " due to no primary title.\n";
	  continue;
	}
	if (0 == (*ruler)->countBaronies()) {
	  // Rebels, adventurers, and suchlike riffraff.
	  if (sovereignsOnly) {
	    Logger::logStream("countries") << "Skipping " << nameAndNumber(*ruler)
					   << " of " << primary->getName()
					   << " due to zero baronies.\n";
	  }
	  continue;
	}

	if ((*ruler)->isSovereign()) {
	  if (candidateCountries.empty()) {
	    if (!primary->getSovereignTitle()) {
	      Logger::logStream(LogStream::Error) << "Ran out of EU4 countries and could not find de-jure to absorb "
						  << primary->getName() << ".\n";
	      return false;
	    }
	    continue;
	  }
	  if (sovereignsOnly) {
	    if (primary->getEU4Country()) continue;
	    Logger::logStream("countries") << "Converting " << primary->getName() << " because it is primary title of sovereign.\n";
	    convertTitle(primary, (*ruler), initialProvincesMap, candidateCountries);
	  }
	  else {
	    for (CK2Title::Iter title = (*ruler)->startTitle(); title != (*ruler)->finalTitle(); ++title) {
	      if (primary == (*title)) continue;
	      if (primary->getLevel() == TitleLevel::Empire) {
		if (*primary->getLevel() < *TitleLevel::Kingdom) continue;
	      }
	      else if (primary->getLevel() != (*title)->getLevel()) continue;
	      if ((*title)->getEU4Country()) continue;
	      Logger::logStream("countries") << "Converting " << (*title)->getName() << " as union.\n";
	      convertTitle((*title), (*ruler), initialProvincesMap, candidateCountries);
	      unions[*ruler]++;
	      if (unions[*ruler] >= maxUnions) {
		Logger::logStream("countries") << "Skipping " << (*title)->getName() << ", over union limit.\n";
		break;
	      }
	      if (candidateCountries.empty()) break;
	    }
	  }
	}
	else if (!sovereignsOnly) {
	  // Vassal. Convert if there's room.
	  if (primary->getEU4Country()) continue;
	  CK2Ruler* sovereign = primary->getSovereign();
	  CK2Title* kingdom = primary->getDeJureLevel(TitleLevel::Kingdom);
	  if (!sovereign) {
	    Logger::logStream("countries") << "Could not find sovereign for "
					   << nameAndNumber(*ruler) << " of "
					   << primary->getName()
					   << "\n";
	    continue;
	  }

	  CK2Ruler* immediateLiege = (*ruler)->getLiege();
	  if ((!immediateLiege->isSovereign()) && (!subInfeudate)) {
	    Logger::logStream("countries") << "Skipping " << primary->getName() << " to avoid subinfeudation.\n";
	    continue;
	  }
	  bool notTooMany = (totalVassals[sovereign][kingdom] < maxSmallVassals);
	  bool important = *(primary->getLevel()) >= *(TitleLevel::Kingdom);
	  if (notTooMany || important) {
	    Logger::logStream("countries") << "Converting " << primary->getName() << " as vassal, "
					   << (notTooMany ? (nameAndNumber(sovereign) + " not over limit in " + (kingdom ? kingdom->getName() : PlainNone)) :
					       string("too big to ignore"))
					   << ".\n";
	    convertTitle(primary, (*ruler), initialProvincesMap, candidateCountries);
	    totalVassals[sovereign][kingdom]++;
	  }
	  else {
	    Logger::logStream("countries") << "Skipping " << primary->getName() << ", too small.\n";
	  }
	}

	if (candidateCountries.empty()) break;
      }
    }
    if (candidateCountries.empty()) {
      break;
    }
  }

  Logger::logStream(LogStream::Info) << "Done with country mapping.\n" << LogOption::Undent;
  return true; 
}

bool Converter::createProvinceMap () {
  if (!provinceMapObject) {
    Logger::logStream(LogStream::Error) << "Error: Could not find province-mapping object.\n";
    return false; 
  }

  Logger::logStream(LogStream::Info) << "Starting province mapping\n" << LogOption::Indent;
  // We have a map from county titles to EU4 provinces.
  // We also have a list of CK2 provinces that know what
  // their primary settlement is; it's a barony. The barony
  // knows its liege, which will be a county. So we go
  // CK2 province -> CK2 barony -> CK2 county -> EU4 province.

  map<string, string> baronyToCountyMap;
  for (CK2Title::Iter title = CK2Title::start(); title != CK2Title::final(); ++title) {
    if (TitleLevel::Barony != (*title)->getLevel()) continue;
    CK2Title* liege = (*title)->getLiege();
    if (!liege) {
      Logger::logStream(LogStream::Error) << "Barony " << (*title)->getName() << " has no liege? Ignoring.\n";
      continue;
    }
    if (TitleLevel::County != liege->getLevel()) {
      // Not an error, indicates the family palace of a merchant republic.
      continue;
    }

    baronyToCountyMap[(*title)->getName()] = liege->getName();
  }

  for (CK2Province::Iter ckprov = CK2Province::start(); ckprov != CK2Province::final(); ++ckprov) {
    string baronytag = remQuotes((*ckprov)->safeGetString("primary_settlement", QuotedNone));
    if (baronytag == "---") continue; // Indicates wasteland.
    if (PlainNone == baronytag) {
      Logger::logStream(LogStream::Warn) << "Could not find primary settlement of "
					 << nameAndNumber(*ckprov)
					 << ", ignoring.\n";
      continue;
    }

    if (0 == baronyToCountyMap.count(baronytag)) {
      Logger::logStream(LogStream::Warn) << "Could not find county liege of barony "
					 << addQuotes(baronytag)
					 << " in county "
					 << nameAndNumber(*ckprov)
					 << ", ignoring.\n";
      continue;
    }

    string countytag = baronyToCountyMap[baronytag];
    CK2Title* county = CK2Title::findByName(countytag);
    if (!county) {
      Logger::logStream(LogStream::Warn) << "Could not find county "
					 << countytag
					 << ", allegedly location of barony "
					 << baronytag
					 << ".\n";
    }
    (*ckprov)->setCountyTitle(county);
    objvec conversions = provinceMapObject->getValue(countytag);
    if (0 == conversions.size()) {
      Logger::logStream(LogStream::Warn) << "Could not find EU4 equivalent for province "
					 << nameAndNumber(*ckprov)
					 << ", ignoring.\n";
      continue;
    }

    for (objiter conversion = conversions.begin(); conversion != conversions.end(); ++conversion) {
      string eu4id = (*conversion)->getLeaf();
      EU4Province* target = EU4Province::findByName(eu4id);
      if (!target) {
	Logger::logStream(LogStream::Warn) << "Could not find EU4 province " << eu4id
					   << ", (missing DLC in input save?), skipping "
					   << nameAndNumber(*ckprov) << ".\n";
	continue;
      }
      (*ckprov)->assignProvince(target);
      Logger::logStream("provinces") << nameAndNumber(*ckprov)
				     << " mapped to EU4 province "
				     << nameAndNumber(target)
				     << ".\n";
    }
  }

  Logger::logStream(LogStream::Info) << "Done with province mapping.\n" << LogOption::Undent;
  return true; 
}

void Converter::loadFiles () {
  string dirToUse = remQuotes(configObject->safeGetString("maps_dir", ".\\maps\\"));
  Logger::logStream(LogStream::Info) << "Directory: \"" << dirToUse << "\"\n" << LogOption::Indent;

  Parser::ignoreString = "EU4txt";
  eu4Game = loadTextFile(dirToUse + "input.eu4");
  Parser::ignoreString = "";
  provinceMapObject = loadTextFile(dirToUse + "provinces.txt");
  deJureObject = loadTextFile(dirToUse + "de_jure_lieges.txt");
  ckBuildingObject = loadTextFile(dirToUse + "ck_buildings.txt");
  euBuildingObject = loadTextFile(dirToUse + "eu_buildings.txt");
  Object* ckTraitObject = loadTextFile(dirToUse + "ck_traits.txt");
  CK2Character::ckTraits = ckTraitObject->getLeaves();
  advisorTypesObject = loadTextFile(dirToUse + "advisors.txt");
  if (!advisorTypesObject) {
    Logger::logStream(LogStream::Warn) << "Warning: Did not find advisors.txt. Proceeding without advisor types info. No advisors will be created.\n";
    advisorTypesObject = new Object("dummy_advisors");
  }
  
  string overrideFileName = remQuotes(configObject->safeGetString("custom", QuotedNone));
  if ((PlainNone != overrideFileName) && (overrideFileName != "NOCUSTOM")) customObject = loadTextFile(dirToUse + overrideFileName);
  else customObject = new Object("custom");
  countryMapObject = customObject->getNeededObject("country_overrides");
  setDynastyNames(loadTextFile(dirToUse + "dynasties.txt"));

  Logger::logStream(LogStream::Info) << "Done loading input files\n" << LogOption::Undent;
}

void Converter::setDynastyNames (Object* dynastyNames) {
  if (!dynastyNames) {
    Logger::logStream(LogStream::Warn) << "Warning: Did not find dynasties.txt. Many famous dynasties will be nameless.\n";
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

double getVassalPercentage (map<EU4Country*, vector<EU4Country*> >::iterator lord, map<EU4Country*, int>& ownerMap) {
  double total = ownerMap[lord->first];
  double vassals = 0;
  for (vector<EU4Country*>::iterator subject = lord->second.begin(); subject != lord->second.end(); ++subject) {
    double curr = ownerMap[*subject];
    total += curr;
    vassals += curr;
  }
  if (0 == total) return 0;
  return vassals / total;
}

void constructOwnerMap (map<EU4Country*, int>* ownerMap) {
  for (EU4Province::Iter eu4prov = EU4Province::start(); eu4prov != EU4Province::final(); ++eu4prov) {
    if (0 == (*eu4prov)->numCKProvinces()) continue;
    EU4Country* owner = EU4Country::getByName(remQuotes((*eu4prov)->safeGetString("owner")));
    if (!owner) continue;
    (*ownerMap)[owner]++;
  }
}

bool Converter::adjustBalkanisation () {
  Logger::logStream(LogStream::Info) << "Adjusting balkanisation.\n" << LogOption::Indent;

  map<EU4Country*, vector<EU4Country*> > subjectMap;
  Object* diplomacy = eu4Game->getNeededObject("diplomacy");
  objvec relations = diplomacy->getLeaves();
  for (objiter relation = relations.begin(); relation != relations.end(); ++relation) {
    string key = (*relation)->getKey();
    if ((key != "vassal") && (key != "union")) continue;
    EU4Country* overlord = EU4Country::getByName(remQuotes((*relation)->safeGetString("first", QuotedNone)));
    if (!overlord) continue;
    EU4Country* subject = EU4Country::getByName(remQuotes((*relation)->safeGetString("second", QuotedNone)));
    if (!subject) continue;
    subjectMap[overlord].push_back(subject);
  }

  map<EU4Country*, int> ownerMap;
  constructOwnerMap(&ownerMap);

  double maxBalkan = configObject->safeGetFloat("max_balkanisation", 0.40);
  double minBalkan = configObject->safeGetFloat("min_balkanisation", 0.30);
  int balkanThreshold = configObject->safeGetInt("balkan_threshold");
  for (map<EU4Country*, vector<EU4Country*> >::iterator lord = subjectMap.begin(); lord != subjectMap.end(); ++lord) {
    if (0 == lord->second.size()) continue;
    EU4Country* overlord = lord->first;
    double vassalPercentage = getVassalPercentage(lord, ownerMap);
    bool dent = false;
    bool printed = false;
    while (vassalPercentage < minBalkan) {
      if (ownerMap[overlord] <= balkanThreshold) {
	Logger::logStream("countries") << ownerMap[overlord] << " provinces are too few to balkanise.\n";
	break;
      }
      if (!printed) {
	Logger::logStream("countries") << "Vassal percentage " << vassalPercentage << ", balkanising "
				       << overlord->getKey() << ".\n" << LogOption::Indent;
	printed = true;
	dent = true;
      }

      set<EU4Country*> attempted;
      bool success = false;
      while (attempted.size() < lord->second.size()) {
	EU4Country* target = lord->second[0];
	for (vector<EU4Country*>::iterator subject = lord->second.begin(); subject != lord->second.end(); ++subject) {
	  if (attempted.count(*subject)) continue;
	  if (ownerMap[*subject] >= ownerMap[target]) continue;
	  target = (*subject);
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
	for (vector<CK2Title*>::iterator kingdom = targetKingdoms.begin(); kingdom != targetKingdoms.end(); ++kingdom) {
	  for (EU4Province::Iter eu4prov = overlord->startProvince(); eu4prov != overlord->finalProvince(); ++eu4prov) {
	    bool acceptable = false;
	    for (CK2Province::Iter ck2prov = (*eu4prov)->startProv(); ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
	      CK2Title* countyTitle = (*ck2prov)->getCountyTitle();
	      if (!countyTitle) continue;
	      CK2Title* curr = countyTitle->getDeJureLevel(TitleLevel::Kingdom);
	      if (curr != (*kingdom)) continue;
	      acceptable = true;
	      break;
	    }
	    if (!acceptable) continue;
	    Logger::logStream("countries") << "Reassigned " << (*eu4prov)->getName() << " to "
					   << target->getKey() << "\n";
	    (*eu4prov)->assignCountry(target);
	    target->setAsCore(*eu4prov);
	    overlord->removeCore(*eu4prov);
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
	Logger::logStream("countries") << "Vassal percentage " << vassalPercentage << ", reblobbing "
				       << overlord->getKey() << ".\n" << LogOption::Indent;
	dent = true;
	printed = true;
      }

      set<EU4Country*> attempted;
      bool success = false;

      while (attempted.size() < lord->second.size()) {
	EU4Country* target = lord->second[0];
	for (vector<EU4Country*>::iterator subject = lord->second.begin(); subject != lord->second.end(); ++subject) {
	  if (attempted.count(*subject)) continue;
	  if (0 == ownerMap[*subject]) {
	    attempted.insert(*subject);
	    continue;
	  }
	  if ((ownerMap[target] > 0) && (ownerMap[*subject] >= ownerMap[target])) continue;
	  target = (*subject);
	}
	if (attempted.count(target)) break;
	attempted.insert(target);
	if (0 == ownerMap[target]) continue;
	EU4Province::Iter iter = target->startProvince();
	EU4Province* province = *iter;
	if (province->getKey() == target->safeGetString("capital")) {
	  ++iter;
	  if (iter != target->finalProvince()) province = *iter;
	}

	Logger::logStream("countries") << "Reabsorbed " << province->getName()
				       << " from " << target->getName()
				       << ".\n";
	province->assignCountry(overlord);
	overlord->setAsCore(province);
	target->removeCore(province);

	ownerMap[overlord]++;
	ownerMap[target]--;
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
      Logger::logStream("countries") << "Final vassal percentage " << vassalPercentage << ": (" << overlord->getName();
      for (EU4Province::Iter prov = overlord->startProvince(); prov != overlord->finalProvince(); ++prov) {
	Logger::logStream("countries") << " " << (*prov)->getName();
      }
      Logger::logStream("countries") << ") ";
      for (vector<EU4Country*>::iterator subject = lord->second.begin(); subject != lord->second.end(); ++subject) {
	Logger::logStream("countries") << "(" << (*subject)->getName();
	for (EU4Province::Iter prov = (*subject)->startProvince(); prov != (*subject)->finalProvince(); ++prov) {
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
  objvec buildingTypes = ckBuildingObject->getLeaves();
  if (10 > buildingTypes.size()) {
    Logger::logStream(LogStream::Warn) << "Only found "
				       << buildingTypes.size()
				       << " types of buildings. Proceeding, but dubiously.\n";
  }
  for (objiter bt = buildingTypes.begin(); bt != buildingTypes.end(); ++bt) {
    double weight = (*bt)->safeGetFloat("gold_cost");
    weight += (*bt)->safeGetFloat("build_time") / 36.5;
    (*bt)->setLeaf("weight", weight);
  }
    
  Object* weightObject = configObject->getNeededObject("buildings");
  Object* troops = configObject->getNeededObject("troops");
  for (CK2Province::Iter ck2prov = CK2Province::start(); ck2prov != CK2Province::final(); ++ck2prov) {
    (*ck2prov)->calculateWeights(weightObject, troops, buildingTypes);
    Logger::logStream("provinces") << nameAndNumber(*ck2prov)
				   << " has weights production "
				   << (*ck2prov)->getWeight(ProvinceWeight::Production)
				   << ", taxation "
				   << (*ck2prov)->getWeight(ProvinceWeight::Taxation)
				   << ", manpower "
				   << (*ck2prov)->getWeight(ProvinceWeight::Manpower)
				   << ".\n";
  }

  // Initialise EU4Country iterator over baronies.
  for (CK2Province::Iter ck2prov = CK2Province::start(); ck2prov != CK2Province::final(); ++ck2prov) {
    for (objiter barony = (*ck2prov)->startBarony(); barony != (*ck2prov)->finalBarony(); ++barony) {
      CK2Title* baronyTitle = CK2Title::findByName((*barony)->getKey());
      if (!baronyTitle) continue;
      CK2Ruler* ruler = baronyTitle->getRuler();
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

bool Converter::cleanEU4Nations () {
  Logger::logStream(LogStream::Info) << "Beginning nation cleanup.\n" << LogOption::Indent;
  Object* keysToClear = configObject->getNeededObject("keys_to_clear");
  keysToClear->addToList("owned_provinces");
  keysToClear->addToList("core_provinces");

  Object* keysToRemove = configObject->getNeededObject("keys_to_remove");
  Object* zeroProvKeys = configObject->getNeededObject("keys_to_remove_on_zero_provs");

  map<EU4Country*, int> ownerMap;
  constructOwnerMap(&ownerMap);

  Object* diplomacy = eu4Game->getNeededObject("diplomacy");
  objvec dipObjects = diplomacy->getLeaves();
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
    Logger::logStream("countries") << (*eu4country)->getKey() << " has no provinces, removing.\n";
    (*eu4country)->resetLeaf(EU4Country::kNoProvinceMarker, "yes");
    for (int i = 0; i < zeroProvKeys->numTokens(); ++i) {
      (*eu4country)->unsetValue(zeroProvKeys->getToken(i));
    }
    objvec dipsToRemove;
    for (objiter dip = dipObjects.begin(); dip != dipObjects.end(); ++dip) {
      string first = remQuotes((*dip)->safeGetString("first"));
      string second = (*dip)->safeGetString("second");
      if ((first != (*eu4country)->getKey()) && (remQuotes(second) != (*eu4country)->getKey())) continue;
      dipsToRemove.push_back(*dip);
      EU4Country* overlord = EU4Country::getByName(first);
      overlord->getNeededObject("friends")->remToken(second);
      overlord->getNeededObject("vassals")->remToken(second);
      overlord->getNeededObject("subjects")->remToken(second);
      overlord->getNeededObject("lesser_union_partners")->remToken(second);
    }
    for (objiter rem = dipsToRemove.begin(); rem != dipsToRemove.end(); ++rem) {
      diplomacy->removeObject(*rem);
    }
  }

  for (EU4Province::Iter eu4prov = EU4Province::start(); eu4prov != EU4Province::final(); ++eu4prov) {
    if (!(*eu4prov)->converts()) continue;
    EU4Country* owner = EU4Country::getByName(remQuotes((*eu4prov)->safeGetString("owner")));
    if (owner) owner->getNeededObject("owned_provinces")->addToList((*eu4prov)->getKey());
    objvec cores = (*eu4prov)->getValue("core");
    for (objiter core = cores.begin(); core != cores.end(); ++core) {
      EU4Country* coreHaver = EU4Country::getByName(remQuotes((*core)->getLeaf()));
      if (coreHaver) coreHaver->setAsCore(*eu4prov);
    }
  }

  Logger::logStream(LogStream::Info) << "Done with nation cleanup.\n" << LogOption::Undent;
  return true;
}

double Converter::calculateTroopWeight (Object* levy, Logger* logstream) {
  static Object* troopWeights = configObject->getNeededObject("troops");
  static objvec troopTypes = troopWeights->getLeaves();
  double ret = 0;
  for (objiter ttype = troopTypes.begin(); ttype != troopTypes.end(); ++ttype) {
    string key = (*ttype)->getKey();
    Object* strength = levy->safeGetObject(key);
    if (!strength) continue;
    double amount = strength->tokenAsFloat(1);
    if (logstream) (*logstream) << key << ": " << amount << "\n";
    amount *= troopWeights->safeGetFloat(key);
    ret += amount;
  }
  return ret;
}

bool Converter::createArmies () {
  Logger::logStream(LogStream::Info) << "Beginning army creation.\n" << LogOption::Indent;
  objvec armyIds;
  objvec regimentIds;
  set<string> armyLocations;
  string armyType = "54";
  string regimentType = "54";
  double totalCkTroops = 0;
  int countries = 0;

  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    if (!(*eu4country)->converts()) {
      (*eu4country)->unsetValue("army");
      continue;
    }
    objvec armies = (*eu4country)->getValue("army");
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
    (*eu4country)->unsetValue("army");

    Logger::logStream("armies") << (*eu4country)->getKey() << " levies: \n" << LogOption::Indent;
    double ck2troops = 0;
    for (EU4Province::Iter eu4prov = (*eu4country)->startProvince(); eu4prov != (*eu4country)->finalProvince(); ++eu4prov) {
      Logger::logStream("armies") << (*eu4prov)->safeGetString("name") << ":\n" << LogOption::Indent;
      double weighted = 0;
      for (CK2Province::Iter ck2prov = (*eu4prov)->startProv(); ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
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
	  if (sovereign != (*eu4country)->getRuler()) {
	    if ((sovereign) && (sovereign->getEU4Country())) {
	      Logger::logStream("armies") << "Ignoring, part of " << sovereign->getEU4Country()->getKey() << ".\n";
	    }
	    else {
	      Logger::logStream("armies") << "Ignoring, no sovereign.\n";
	    }
	    continue;
	  }
	  double distanceFactor = 1.0 / (1 + baronyTitle->distanceToSovereign());
	  Logger::logStream("armies") << "(" << distanceFactor << "):\n" << LogOption::Indent;
	  double amount = calculateTroopWeight(levy, &(Logger::logStream("armies")));
	  Logger::logStream("armies") << LogOption::Undent;
	  amount *= distanceFactor;
	  amount *= weightFactor;
	  ck2troops += amount;
	  weighted += amount;
	}
      }
      Logger::logStream("armies") << "Province total: " << weighted << "\n" << LogOption::Undent;
    }
    Logger::logStream("armies") << (*eu4country)->getKey() << " has " << ck2troops << " weighted levies.\n" << LogOption::Undent;
    (*eu4country)->resetLeaf("ck_troops", ck2troops);
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
  string dynastyNumber = ruler->safeGetString("dynasty", PlainNone);
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

void Converter::makeMonarch (CK2Character* ruler, CK2Ruler* king, const string& keyword, Object* bonusTraits) {
  Object* monarchDef = new Object(keyword);
  monarchDef->setLeaf("name", ruler->safeGetString(birthNameString, "\"Some Guy\""));
  CK2Title* primary = king->getPrimaryTitle();
  monarchDef->setLeaf("country", addQuotes(primary->getEU4Country()->getKey()));

  map<string, map<string, double> > sources;
  sources["MIL"]["ck_martial"] = max(0, ruler->getAttribute(CKAttribute::Martial) / 5);
  sources["DIP"]["ck_diplomacy"] = max(0, ruler->getAttribute(CKAttribute::Diplomacy) / 5);
  sources["ADM"]["ck_stewardship"] = max(0, ruler->getAttribute(CKAttribute::Stewardship) / 5);
  for (map<string, map<string, double> >::iterator area = sources.begin(); area != sources.end(); ++area) {
    Object* bonus = bonusTraits->getNeededObject(area->first);
    objvec bonoi = bonus->getLeaves();
    for (objiter bon = bonoi.begin(); bon != bonoi.end(); ++bon) {
      string desc = (*bon)->getKey();
      if (!ruler->hasTrait(desc)) continue;
      area->second[desc] = bonus->safeGetInt(desc);
    }
  }
  string gameDate = eu4Game->safeGetString("date", "1444.1.1");
  double age = ruler->getAge(gameDate);
  if (age < 16) {
    age = 16 / max(1.0, age);
    for (map<string, map<string, double> >::iterator area = sources.begin(); area != sources.end(); ++area) {
      area->second["ck_martial"] *= age;
      area->second["ck_diplomacy"] *= age;
      area->second["ck_stewardship"] *= age;
    }
  }
  Logger::logStream("characters") << keyword << " " << nameAndNumber(ruler)
				  << " with " << "\n" << LogOption::Indent;
  for (map<string, map<string, double> >::iterator area = sources.begin(); area != sources.end(); ++area) {
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
  Logger::logStream("characters") << LogOption::Undent;
  if (ruler->safeGetString("female", "no") == "yes") monarchDef->setLeaf("female", "yes");
  Object* monarchId = createMonarchId();
  monarchDef->setValue(monarchId);
  monarchDef->setLeaf("dynasty", getDynastyName(ruler));
  monarchDef->setLeaf("birth_date", remQuotes(ruler->safeGetString("birth_date", addQuotes(gameDate))));

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
}

Object* createLeader (CK2Character* base, const string& leaderType, const objvec& generalSkills, const string& birthDate) {
  Object* leader = new Object("leader");
  leader->setLeaf("name", getFullName(base));
  leader->setLeaf("type", leaderType);
  for (vector<Object*>::const_iterator skill = generalSkills.begin(); skill != generalSkills.end(); ++skill) {
    string keyword = (*skill)->getKey();
    int amount = 0;
    for (int i = 0; i < (*skill)->numTokens(); ++i) {
      if (!base->hasTrait((*skill)->getToken(i))) continue;
      ++amount;
    }
    Logger::logStream("characters") << " " << keyword << ": " << amount << (amount > 0 ? " from " : "");
    for (int i = 0; i < (*skill)->numTokens(); ++i) {
      if (!base->hasTrait((*skill)->getToken(i))) continue;
      Logger::logStream("characters") << (*skill)->getToken(i) << " ";
    }
    leader->setLeaf(keyword, amount);
    Logger::logStream("characters") << "\n";
  }
  leader->setLeaf("activation", birthDate);
  return leader;
}

void Converter::makeLeader (Object* eu4country, const string& leaderType, CK2Character* base, const objvec& generalSkills, const string& birthDate) {
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
  Object* bonusTraits = configObject->getNeededObject("bonusTraits");
  Object* active_advisors = eu4Game->getNeededObject("active_advisors");
  map<string, objvec> allAdvisors;
  objvec advisorTypes = advisorTypesObject->getLeaves();
  objvec generalSkills = configObject->getNeededObject("generalSkills")->getLeaves();
  string birthDate = eu4Game->safeGetString("date", "1444.11.11");

  int numAdvisors = 0;
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    if (!(*eu4country)->converts()) continue;
    Object* country_advisors = active_advisors->getNeededObject((*eu4country)->getKey());
    country_advisors->clear();

    CK2Ruler* ruler = (*eu4country)->getRuler();
    if (ruler->safeGetString("madeCharacters", PlainNone) == "yes") continue;
    ruler->setLeaf("madeCharacters", "yes");
    string capitalTag = (*eu4country)->safeGetString("capital");
    EU4Province* capital = EU4Province::findByName(capitalTag);
    if (!capital) continue;
    string eu4Tag = (*eu4country)->getKey();
    (*eu4country)->unsetValue("advisor");
    Logger::logStream("characters") << "Creating characters for " << eu4Tag << ":\n" << LogOption::Indent;
    makeMonarch(ruler, ruler, "monarch", bonusTraits);

    (*eu4country)->unsetValue("heir");
    if ((*eu4country)->safeGetString("needs_heir", "no") == "yes") {
      CK2Character* ckHeir = ruler->getOldestChild();
      if (ckHeir) makeMonarch(ckHeir, ruler, "heir", bonusTraits);
    }

    Object* history = capital->getNeededObject("history");
    for (CouncilTitle::Iter council = CouncilTitle::start(); council != CouncilTitle::final(); ++council) {
      CK2Character* councillor = ruler->getCouncillor(*council);
      Logger::logStream("characters") << (*council)->getName() << ": " << (councillor ? nameAndNumber(councillor) : "None\n");
      if (!councillor) continue;

      Object* advisor = new Object("advisor");
      advisor->setLeaf("name", getFullName(councillor));
      int points = -1;
      Object* advObject = 0;
      set<string> decisionTraits;
      for (objiter advType = advisorTypes.begin(); advType != advisorTypes.end(); ++advType) {
	Object* mods = (*advType)->getNeededObject("traits");
	objvec traits = mods->getLeaves();
	int currPoints = 0;
	set<string> yesTraits;
	for (objiter trait = traits.begin(); trait != traits.end(); ++trait) {
	  string traitName = (*trait)->getKey();
	  if ((!councillor->hasTrait(traitName)) && (!councillor->hasModifier(traitName))) continue;
	  currPoints += mods->safeGetInt(traitName);
	  yesTraits.insert(traitName);
	}
	if (currPoints < points) continue;
	points = currPoints;
	advObject = (*advType);
	decisionTraits = yesTraits;
      }
      string advisorType = advObject->getKey();
      advisor->setLeaf("type", advisorType);
      allAdvisors[advisorType].push_back(advisor);
      int skill = 0;
      for (CKAttribute::Iter att = CKAttribute::start(); att != CKAttribute::final(); ++att) {
	int mult = 1;
	if ((*att)->getName() == advObject->safeGetString("main_attribute")) mult = 2;
	skill += councillor->getAttribute(*att) * mult;
      }
      advisor->setLeaf("skill", skill);
      Logger::logStream("characters") << " " << advisorType << " based on traits ";
      for (set<string>::iterator t = decisionTraits.begin(); t != decisionTraits.end(); ++t) {
	Logger::logStream("characters") << (*t) << " ";
      }
      Logger::logStream("characters") << "\n";
      advisor->setLeaf("location", capitalTag);
      advisor->setLeaf("date", "1444.1.1");
      advisor->setLeaf("hire_date", "1.1.1");
      advisor->setLeaf("death_date", "9999.1.1");
      advisor->setLeaf("female", councillor->safeGetString("female", "no"));
      Object* id = createTypedId("advisor", "51");
      advisor->setValue(id);
      Object* birth = new Object(birthDate);
      birth->setValue(advisor);
      history->setValue(birth);
      Object* active = new Object(id);
      active->setKey("advisor");
      country_advisors->setValue(active);
      ++numAdvisors;
    }

    CK2Character* bestGeneral = 0;
    int highestSkill = -1;
    for (CK2Character::CharacterIter commander = ruler->startCommander(); commander != ruler->finalCommander(); ++commander) {
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
    Logger::logStream("characters") << "General: " << (bestGeneral ? nameAndNumber(bestGeneral) : "None") << "\n";
    if (bestGeneral) {
      Logger::logStream("characters") << LogOption::Indent;
      makeLeader((*eu4country)->object, string("general"), bestGeneral, generalSkills, birthDate);
      Logger::logStream("characters") << LogOption::Undent;
    }

    CK2Character* admiral = ruler->getAdmiral();
    Logger::logStream("characters") << "Admiral: " << (admiral ? nameAndNumber(admiral) : "None") << "\n";
    if (admiral) {
      Logger::logStream("characters") << LogOption::Indent;
      makeLeader((*eu4country)->object, string("admiral"), admiral, generalSkills, birthDate);
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
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    CK2Ruler* ruler = (*eu4country)->getRuler();
    if (!ruler) continue;
    CK2Title* primary = ruler->getPrimaryTitle();
    string government_rank = "1";
    double totalDevelopment = 0;
    for (EU4Province::Iter eu4prov = (*eu4country)->startProvince(); eu4prov != (*eu4country)->finalProvince(); ++eu4prov) {
      totalDevelopment += (*eu4prov)->safeGetFloat("base_tax");
      totalDevelopment += (*eu4prov)->safeGetFloat("base_manpower");
      totalDevelopment += (*eu4prov)->safeGetFloat("base_production");
    }
    if (totalDevelopment > empireThreshold) government_rank = "3";
    else if (totalDevelopment > kingdomThreshold) government_rank = "2";
    (*eu4country)->resetLeaf("government_rank", government_rank);
    (*eu4country)->resetHistory("government_rank", government_rank);
    string ckGovernment = ruler->safeGetString("government", "feudal_government");
    Object* govInfo = govObject->safeGetObject(ckGovernment);
    string euGovernment = (*eu4country)->safeGetString("government");
    if (govInfo) {
      Logger::logStream("governments") << nameAndNumber(ruler) << " of " << (*eu4country)->getKey()
				       << " has CK government " << ckGovernment;
      if (primary) {
	string succession = primary->safeGetString("succession", PlainNone);
	Object* successionObject = govInfo->safeGetObject(succession);
	if (successionObject) {
	  Logger::logStream("governments") << " (" << succession << ")";
	  govInfo = successionObject;
	}
      }
      euGovernment = govInfo->safeGetString("eugov");
      Logger::logStream("governments") << " giving EU government " << euGovernment
				       << " of rank " << government_rank
				       << " based on total development " << totalDevelopment
				       << ".\n";
      (*eu4country)->resetLeaf("needs_heir", govInfo->safeGetString("heir", "no"));
    }
    else {
      Logger::logStream(LogStream::Warn) << "No information about CK government "
					 << ckGovernment
					 << ", leaving "
					 << (*eu4country)->getKey()
					 << " as "
					 << euGovernment
					 << ".\n";
    }
    (*eu4country)->resetLeaf("government", euGovernment);
    (*eu4country)->resetHistory("government", euGovernment);
  }  
  Logger::logStream(LogStream::Info) << "Done with governments.\n" << LogOption::Undent;
  return true;
}
bool Converter::createNavies () {
  Logger::logStream(LogStream::Info) << "Beginning navy creation.\n" << LogOption::Indent;
  objvec navyIds;
  objvec shipIds;
  set<string> navyLocations;
  vector<string> shipTypeStrings;
  map<string, vector<string> > shipNames;
  string navyType = "54";
  string shipType = "54";
  
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    if (!(*eu4country)->converts()) {
      (*eu4country)->unsetValue("navy");
      continue;
    }
    objvec navies = (*eu4country)->getValue("navy");
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
	shipTypeStrings.push_back((*ship)->safeGetString("type"));
	shipNames[(*eu4country)->getKey()].push_back((*ship)->safeGetString("name"));
      }
    }
    (*eu4country)->unsetValue("navy");
  }

  if (0 < shipIds.size()) shipType = shipIds.back()->safeGetString("type", shipType);
  if (0 < navyIds.size()) navyType = navyIds.back()->safeGetString("type", navyType);
  
  double totalShips = 0;
  Object* shipWeights = configObject->getNeededObject("ships");
  objvec shipTypes = shipWeights->getLeaves();
  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    EU4Country* eu4country = (*ruler)->getEU4Country();
    if (!eu4country) continue;
    double currShip = 0;
    for (objiter barony = eu4country->startBarony(); barony != eu4country->finalBarony(); ++barony) {
      Object* baronyLevy = (*barony)->getNeededObject("levy");
      for (objiter ttype = shipTypes.begin(); ttype != shipTypes.end(); ++ttype) {
	string key = (*ttype)->getKey();
	Object* strength = baronyLevy->safeGetObject(key);
	if (!strength) continue;
	double amount = strength->tokenAsFloat(1);
	amount *= shipWeights->safeGetFloat(key);
	currShip += amount;
      }
    }
    totalShips += currShip;
    (*ruler)->resetLeaf("shipWeight", currShip);    
  }

  if (0 == totalShips) {
    Logger::logStream(LogStream::Warn) << "Warning: Found no liege-levy ships. No navies will be created.\n" << LogOption::Undent;
    return true;
  }

  Object* forbidden = configObject->getNeededObject("forbidNavies");
  set<string> forbid;
  for (int i = 0; i < forbidden->numTokens(); ++i) forbid.insert(forbidden->getToken(i));
  
  int numShips = shipIds.size();
  Logger::logStream("navies") << "Total weighted navy strength "
			      << totalShips
			      << ", EU4 ships "
			      << numShips
			      << ".\n";
  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    EU4Country* eu4country = (*ruler)->getEU4Country();
    if (!eu4country) continue;
    if (!eu4country->converts()) continue;
    string eu4Tag = eu4country->getKey();
    double currWeight = (*ruler)->safeGetFloat("shipWeight");
    Logger::logStream("navies") << nameAndNumber(*ruler)
				<< " of " << eu4Tag
				<< " has ship weight "
				<< currWeight;
    currWeight /= totalShips;
    currWeight *= numShips;
    int shipsToCreate = (int) floor(0.5 + currWeight);
    Logger::logStream("navies") << " and gets "
				<< shipsToCreate
				<< " ships.\n";
    if (0 == shipsToCreate) continue;
    Object* navy = eu4country->getNeededObject("navy");
    Object* navyId = 0;
    if (!navyIds.empty()) {
      navyId = navyIds.back();
      navyIds.pop_back();
    }
    else {
      navyId = createUnitId(navyType);
    }
    navy->setValue(navyId);
    string name = remQuotes((*ruler)->safeGetString(birthNameString, "\"Nemo\""));
    navy->setLeaf("name", addQuotes(string("Navy of ") + name));
    string capitalTag = eu4country->safeGetString("capital");
    string location = capitalTag;
    if ((0 == navyLocations.count(capitalTag)) || (forbid.count(capitalTag))) {
      for (EU4Province::Iter prov = eu4country->startProvince(); prov != eu4country->finalProvince(); ++prov) {
	string provTag = (*prov)->getKey();
	if (forbid.count(provTag)) continue;
	if ((0 == forbid.count(location)) && (0 == navyLocations.count(provTag))) continue;
	location = provTag;
	if (navyLocations.count(location)) break;
      }
    }

    navy->setLeaf("location", location);
    EU4Province* eu4prov = EU4Province::findByName(location);
    Object* unit = new Object(navyId);
    unit->setKey("unit");
    eu4prov->setValue(unit);
    int created = 0;
    for (int i = 0; i < shipsToCreate; ++i) {
      Object* ship = new Object("ship");
      navy->setValue(ship);
      Object* shipId = 0;
      if (shipIds.empty()) {
	shipId = createUnitId(shipType);
      }
      else {
	shipId = shipIds.back();
	shipIds.pop_back();
      }
      ship->setValue(shipId);
      ship->setLeaf("home", location);
      string name;
      if (!shipNames[eu4Tag].empty()) {
	name = shipNames[eu4Tag].back();
	shipNames[eu4Tag].pop_back();
      }
      else {
	name = createString("\"%s conversion ship %i\"", eu4Tag.c_str(), ++created);
      }
      ship->setLeaf("name", name);
      name = shipTypeStrings.back();
      if (shipTypeStrings.size() > 1) shipTypeStrings.pop_back();
      ship->setLeaf("type", name);
      ship->setLeaf("strength", "1.000");
    }
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

void makeMap (map<string, map<string, int> >& cultures, map<string, vector<string> >& assigns, Object* storage = 0) {
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
      if (cand->second > highestValue * 0.75) {
	Logger::logStream("cultures") << "[" << cand->second / highestValue << "]) ";
      }
      else if (1 == reverseMap[cand->first].size()) {
	Logger::logStream("cultures") << "[sole]) ";
      }
      else {
	Logger::logStream("cultures") << ") ";
	continue;
      }
      assigns[ckCulture].push_back(cand->first);
    }
    Logger::logStream("cultures") << "\n";
    cultures.erase(ckCulture);
  }
}

void proselytise (map<string, vector<string> >& religionMap, string key) {
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
    string euBest = religionMap[ckBest][0];
    if (key == "culture") {
      string existingCulture = (*eu4prov)->safeGetString("culture", PlainNone);
      if (find(religionMap[ckBest].begin(), religionMap[ckBest].end(), existingCulture) != religionMap[ckBest].end()) {
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
  
  map<string, vector<string> > religionMap;
  Object* dynamicReligion = configObject->getNeededObject("dynamicReligions");
  makeMap(religions, religionMap, dynamicReligion);
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

  map<string, vector<string> > cultureMap;
  makeMap(cultures, cultureMap);
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
	acceptedCultures.push_back(cand->first);
      }
    }
    Logger::logStream("cultures") << (*eu4country)->getKey() << ":\n" << LogOption::Indent;
    if (ckRulerReligion != PlainNone) {
      if (0 == religionMap[ckRulerReligion].size()) {
	Logger::logStream("cultures") << "Emergency-assigning " << ckRulerReligion << " to catholic.\n";
	religionMap[ckRulerReligion].push_back("catholic");
      }
      string euReligion = religionMap[ckRulerReligion][0];
      Logger::logStream("cultures") << "Religion: " << euReligion << " based on CK religion " << ckRulerReligion << ".\n";
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
      if (acceptedCultures.size()) {
	Logger::logStream("cultures") << "Accepted cultures: ";
	for (vector<string>::iterator accepted = acceptedCultures.begin(); accepted != acceptedCultures.end(); ++accepted) {
	  Logger::logStream("cultures") << (*accepted) << " ";
	  (*eu4country)->setLeaf("accepted_culture", (*accepted));
	  (*eu4country)->resetHistory("accepted_culture", (*accepted));
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
  Object* unitId = new Object("id");
  int number = eu4Game->safeGetInt(keyword, 1);
  unitId->setLeaf("id", number);
  eu4Game->resetLeaf(keyword, ++number);
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
  if (hreOption == "remove") {
    Logger::logStream("hre") << "Removing bratwurst.\n";
    for (EU4Province::Iter eu4prov = EU4Province::start(); eu4prov != EU4Province::final(); ++eu4prov) {
      (*eu4prov)->unsetValue("hre");
    }
    for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
      (*eu4country)->unsetValue("preferred_emperor");
      (*eu4country)->unsetValue("is_elector");
    }
    eu4Game->unsetValue("emperor");
    eu4Game->unsetValue("old_emperor");
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

bool Converter::modifyProvinces () {
  Logger::logStream(LogStream::Info) << "Beginning province modifications.\n" << LogOption::Indent;
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
    for (CK2Province::Iter ck2prov = (*eu4prov)->startProv(); ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
      double fraction = 1.0 / (*ck2prov)->numEU4Provinces();
      provTaxWeight += fraction * (*ck2prov)->getWeight(ProvinceWeight::Taxation);
      provProWeight += fraction * (*ck2prov)->getWeight(ProvinceWeight::Production);
      provManWeight += fraction * (*ck2prov)->getWeight(ProvinceWeight::Manpower);
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
  }

  Logger::logStream("provinces") << "After distribution: "
				 << afterTax << " base tax, "
				 << afterPro << " base production, "
				 << afterMen << " base manpower.\n";

  Logger::logStream(LogStream::Info) << "Done modifying provinces.\n" << LogOption::Undent;
  return true;
}

// Apparently making this definition local to the function confuses the compiler.
struct WeightDescendingSorter {
  WeightDescendingSorter (ProvinceWeight const* const w, map<CK2Province*, double>* adj) : weight(w), adjustment(adj) {}
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
    Logger::logStream(LogStream::Warn) << "No EU building info. Buildings will not be moved.\n" << LogOption::Undent;
    return true;
  }

  map<string, string> makesObsolete;
  map<string, string> nextLevel;
  map<string, Object*> euBuildingTypes;
  
  objvec euBuildings = euBuildingObject->getLeaves();
  for (objiter euBuilding = euBuildings.begin(); euBuilding != euBuildings.end(); ++euBuilding) {
    int numToBuild = (*euBuilding)->safeGetInt("extra");
    string buildingTag = (*euBuilding)->getKey();
    for (EU4Province::Iter eu4prov = EU4Province::start(); eu4prov != EU4Province::final(); ++eu4prov) {
      if ((*eu4prov)->safeGetString(buildingTag, "no") != "yes") continue;
      numToBuild++;
      (*eu4prov)->unsetValue(buildingTag);
    }
    if (0 == numToBuild) continue;
    Logger::logStream("buildings") << "Found "
				   << numToBuild << " "
				   << buildingTag << ".\n";
    (*euBuilding)->setLeaf("num_to_build", numToBuild);
    euBuildingTypes[buildingTag] = (*euBuilding);
    string obsolifies = (*euBuilding)->safeGetString("make_obsolete", PlainNone);
    if (obsolifies != PlainNone) {
      makesObsolete[buildingTag] = obsolifies;
      nextLevel[obsolifies] = buildingTag;
    }
  }

  CK2Province::Container provList = CK2Province::makeCopy();
  map<CK2Province*, double> adjustment;
  for (CK2Province::Iter prov = provList.begin(); prov != provList.end(); ++prov) {
    adjustment[*prov] = 1;
    for (map<string, Object*>::iterator tag = euBuildingTypes.begin(); tag != euBuildingTypes.end(); ++tag) {
      (*prov)->unsetValue((*tag).first);
    }
  }
  
  while (!euBuildingTypes.empty()) {
    Object* building = 0;
    string buildingTag;
    for (map<string, Object*>::iterator tag = euBuildingTypes.begin(); tag != euBuildingTypes.end(); ++tag) {
      buildingTag = (*tag).first;
      if (nextLevel[buildingTag] != "") continue;
      building = (*tag).second;
      break;
    }
    if (!building) {
      Logger::logStream(LogStream::Warn) << "Could not find a building that's not obsolete. This should never happen. Bailing.\n";
      break;
    }
    string obsolifies = makesObsolete[buildingTag];
    if (obsolifies != "") nextLevel[obsolifies] = "";
    int numToBuild = building->safeGetInt("num_to_build", 0);
    euBuildingTypes.erase(buildingTag);
    if (0 == numToBuild) continue;
    string sortBy = building->safeGetString("sort_by", PlainNone);
    ProvinceWeight const* const weight = ProvinceWeight::findByName(sortBy);
    if (!weight) continue;
    sort(provList.begin(), provList.end(), WeightDescendingSorter(weight, &adjustment));
    for (int i = 0; i < numToBuild; ++i) {
      if (0 == provList[i]->numEU4Provinces()) continue;
      EU4Province* target = *(provList[i]->startEU4Province());
      string tradeGood = target->safeGetString("trade_goods");
      if ((tradeGood == "gold") && (building->safeGetString("allow_in_gold_provinces") == "no")) {
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
      if (badGood) continue;
      Logger::logStream("buildings") << "Creating " << buildingTag << " in "
				     << nameAndNumber(target)
				     << " with adjusted weight "
				     << provList[i]->getWeight(weight) * adjustment[provList[i]]
				     << ".\n";
      adjustment[provList[i]] *= 0.75;
      target->setLeaf(buildingTag, "yes");
      if (building->safeGetString("influencing_fort", "no") == "yes") {
	target->setLeaf("influencing_fort", "yes");
	Object* modifier = building->safeGetObject("modifier");
	if (modifier) {
	  string fortLevel = modifier->safeGetString("fort_level", PlainNone);
	  if (fortLevel != PlainNone) target->setLeaf("fort_level", fortLevel);
	}
      }
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
				       << "based on capital "
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
  keywords["prestige"] = "prestige";
  keywords[kDynastyPower] = kAwesomePower;
  map<string, doublet> globalAmounts;

  map<CK2Ruler*, int> counts;
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    if (!(*eu4country)->converts()) continue;
    CK2Ruler* ruler = (*eu4country)->getRuler();
    counts[ruler]++;
    if (ruler->getPrimaryTitle() != (*eu4country)->getTitle()) continue;
    for (map<string, string>::iterator keyword = keywords.begin(); keyword != keywords.end(); ++keyword) {
      string ck2word = keyword->first;
      string eu4word = keyword->second;
      double amount = ruler->safeGetFloat(ck2word);
      if (amount > 0) globalAmounts[ck2word].x() += amount;
      amount = (*eu4country)->safeGetFloat(eu4word);
      if (amount > 0) globalAmounts[ck2word].y() += amount;
    }
  }

  for (const auto &keyword : keywords) {
    string ck2word = keyword.first;
    string eu4word = keyword.second;
    Logger::logStream("mana")
        << "Redistributing " << globalAmounts[ck2word].y() << " EU4 " << eu4word
        << " to " << globalAmounts[ck2word].x() << " CK2 " << ck2word << ".\n"
        << LogOption::Indent;

    double ratio =
        globalAmounts[ck2word].y() / (1 + globalAmounts[ck2word].x());
    for (EU4Country::Iter eu4country = EU4Country::start();
         eu4country != EU4Country::final(); ++eu4country) {
      CK2Ruler *ruler = (*eu4country)->getRuler();
      if (!ruler)
        continue;
      double ck2Amount = ruler->safeGetFloat(ck2word);
      if (ck2Amount <= 0)
        continue;

      double distribution = 1;
      if (counts[ruler] > 1) {
        if (ruler->getPrimaryTitle() == (*eu4country)->getTitle())
          distribution = 0.666;
        else
          distribution = 0.333 / (counts[ruler] - 1);
      }
      double eu4Amount = ck2Amount * ratio * distribution;
      Logger::logStream("mana")
          << nameAndNumber(ruler) << " has " << ck2Amount << " " << ck2word
          << ", so " << (*eu4country)->getKey() << " gets " << eu4Amount << " "
          << eu4word << ".\n";
      (*eu4country)->resetLeaf(eu4word, eu4Amount);
    }

    Logger::logStream("mana") << LogOption::Undent;
  }


  objvec factions = ck2Game->getValue("active_faction");
  map<CK2Ruler*, vector<CK2Ruler*> > factionMap;
  for (objiter faction = factions.begin(); faction != factions.end(); ++faction) {
    Object* scope = (*faction)->getNeededObject("scope");
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

    objvec backers = (*faction)->getValue("backer");
    for (objiter backer = backers.begin(); backer != backers.end(); ++backer) {
      CK2Ruler* rebel = CK2Ruler::findByName((*backer)->getLeaf());
      if (rebel) factionMap[target].push_back(rebel);
    }
  }

  double maxClaimants = 0;
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    if (!(*eu4country)->converts()) continue;
    CK2Ruler* ruler = (*eu4country)->getRuler();
    CK2Title* primary = ruler->getPrimaryTitle();
    double claimants = 0;
    for (CK2Character::CharacterIter claimant = primary->startClaimant();
         claimant != primary->finalClaimant(); ++claimant) {
      ++claimants;
    }
    if (claimants > maxClaimants) {
      maxClaimants = claimants;
    }
    (*eu4country)->resetLeaf("claimants", claimants);
  }

  double minimumLegitimacy = configObject->safeGetFloat("minimum_legitimacy", 0.25);
  for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
    int totalPower = (*eu4country)->safeGetFloat(kAwesomePower);
    (*eu4country)->unsetValue(kAwesomePower);
    double claimants = (*eu4country)->safeGetFloat("claimants");
    (*eu4country)->unsetValue("claimants");
    if (!(*eu4country)->converts()) continue;
    CK2Ruler* ruler = (*eu4country)->getRuler();
    Object* powers = (*eu4country)->getNeededObject("powers");
    powers->clear();
    totalPower /= 3;
    powers->addToList(totalPower);
    powers->addToList(totalPower);
    powers->addToList(totalPower);

    double totalFactionStrength = 0;
    for (vector<CK2Ruler*>::iterator rebel = factionMap[ruler].begin(); rebel != factionMap[ruler].end(); ++rebel) {
      totalFactionStrength += (*rebel)->countBaronies();
    }
    totalFactionStrength /= (1 + ruler->countBaronies());
    int stability = 3;
    Logger::logStream("mana") << nameAndNumber(ruler) << " has faction strength " << totalFactionStrength;
    totalFactionStrength *= 18;
    stability -= (int) floor(totalFactionStrength);
    if (stability < -3) stability = -3;
    (*eu4country)->resetLeaf("stability", stability);
    Logger::logStream("mana") << " so " << (*eu4country)->getKey() << " has stability " << stability << ".\n";

    (*eu4country)->resetLeaf("legitimacy", "0.000");
    if (((*eu4country)->safeGetString("government") == "merchant_republic") ||
	((*eu4country)->safeGetString("government") == "noble_republic")) {
      (*eu4country)->resetLeaf("republican_tradition", "1.000");
    }
    else {
      Logger::logStream("mana") << claimants << " claims on " << ruler->getPrimaryTitle()->getKey() << ", hence ";
      claimants /= maxClaimants;
      //double legitimacy = minimumLegitimacy + (1.0 - claimants) * (1.0 - minimumLegitimacy);
      double legitimacy = 1.0 - claimants + minimumLegitimacy * claimants;
      (*eu4country)->resetLeaf("legitimacy", legitimacy);
      Logger::logStream("mana") << (*eu4country)->getKey() << " has legitimacy " << legitimacy << ".\n";
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
  vector<string> valuesToRemove;
  valuesToRemove.push_back("core");
  valuesToRemove.push_back("unit");
  for (EU4Province::Iter eu4province = EU4Province::start(); eu4province != EU4Province::final(); ++eu4province) {
    if (0 == (*eu4province)->numCKProvinces()) continue;
    for (vector<string>::iterator tag = objectsToClear.begin(); tag != objectsToClear.end(); ++tag) {
      Object* history = (*eu4province)->getNeededObject(*tag);
      history->clear();
      history->resetLeaf("discovered_by", addQuotes("western"));
    }

    for (vector<string>::iterator tag = valuesToRemove.begin(); tag != valuesToRemove.end(); ++tag) {
      (*eu4province)->unsetValue(*tag);
    }

    Object* discovered = (*eu4province)->safeGetObject("discovered_dates");
    if ((discovered) && (1 < discovered->numTokens())) {
      discovered->resetToken(1, "1.1.1");
    }
    discovered = (*eu4province)->safeGetObject("discovered_by");
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
      Logger::logStream("cores") << "Looking at title "
				 << title->getKey();
      CK2Ruler* ruler = title->getRuler();
      if (ruler) {
	Logger::logStream("cores") << " with ruler " << nameAndNumber(ruler);
	EU4Country* country = ruler->getEU4Country();
	if (country) {
	  Logger::logStream("cores") << " and country " << country->getKey();
	  CK2Title* primary = ruler->getPrimaryTitle();

	  // Claims due to holding de-jure liege title.
	  for (EU4Province::Iter eu4prov = (*ck2prov)->startEU4Province(); eu4prov != (*ck2prov)->finalEU4Province(); ++eu4prov) {
	    claimsMap[*eu4prov][country]++;
	  }
	  if ((primary == title) || (*(title->getLevel()) < *TitleLevel::Kingdom)) {
	    Logger::logStream("cores") << ".\n" << LogOption::Indent;
	    for (EU4Province::Iter eu4prov = (*ck2prov)->startEU4Province(); eu4prov != (*ck2prov)->finalEU4Province(); ++eu4prov) {
	      Logger::logStream("cores") << nameAndNumber(*eu4prov)
					 << " is core of "
					 << country->getKey()
					 << " because of "
					 << title->getKey()
					 << ".\n";
	      country->setAsCore(*eu4prov);
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
    for (objiter barony = (*ck2prov)->startBarony(); barony != (*ck2prov)->finalBarony(); ++barony) {
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
      eu4prov->setLeaf("claim", addQuotes(eu4country->getKey()));
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
  keyWords.push_back("vassals");
  keyWords.push_back("lesser_union_partners");
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
    (*eu4)->resetLeaf("num_of_unions", 0);

    Object* active_relations = (*eu4)->getNeededObject("active_relations");
    objvec relations = active_relations->getLeaves();
    for (objiter relation = relations.begin(); relation != relations.end(); ++relation) {
      (*relation)->clear();
    }
  }

  string startDate = eu4Game->safeGetString("date", "1444.1.1");
  int startYear = year(startDate);

  keyWords.clear();
  keyWords.push_back("friends");
  keyWords.push_back("subjects");
  keyWords.push_back("vassals");

  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    EU4Country* vassalCountry = (*ruler)->getEU4Country();
    if (!vassalCountry) continue;
    if ((*ruler)->isSovereign()) continue;
    CK2Ruler* liege = (*ruler)->getSovereignLiege();
    if (!liege) continue;
    EU4Country* liegeCountry = liege->getEU4Country();
    if (liegeCountry == vassalCountry) continue;
    Object* vassal = new Object("vassal");
    euDipObject->setValue(vassal);
    vassal->setLeaf("first", addQuotes(liegeCountry->getName()));
    vassal->setLeaf("second", addQuotes(vassalCountry->getName()));
    vassal->setLeaf("end_date", "1836.1.1");
    vassal->setLeaf("cancel", "no");
    vassal->setLeaf("start_date", remQuotes((*ruler)->safeGetString("birth_date", addQuotes(startDate))));
    vassalCountry->resetLeaf("liberty_desire", "0.000");

    for (vector<string>::iterator key = keyWords.begin(); key != keyWords.end(); ++key) {
      Object* toAdd = liegeCountry->getNeededObject(*key);
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

  keyWords.clear();
  keyWords.push_back("friends");
  keyWords.push_back("subjects");
  keyWords.push_back("lesser_union_partners");

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
      subject->resetLeaf("is_subject", "yes");
      subject->resetLeaf("is_lesser_in_union", "yes");
      subject->resetLeaf("overlord", addQuotes(overlord->getKey()));
      subject->resetLeaf("liberty_desire", "0.000");
      subject->resetLeaf("num_of_unions", 1);
      overlord->resetLeaf("num_of_unions", 1 + overlord->safeGetInt("num_of_unions"));
      Object* unionObject = new Object("union");
      euDipObject->setValue(unionObject);
      unionObject->setLeaf("first", addQuotes(overlord->getKey()));
      unionObject->setLeaf("second", addQuotes(subject->getKey()));
      unionObject->setLeaf("end_date", createString("%i.1.1", startYear + 50));
      unionObject->setLeaf("cancel", "no");
      unionObject->setLeaf("start_date", remQuotes((*ruler)->safeGetString("birth_date", addQuotes(startDate))));

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
  for (EU4Province::Iter eu4Prov = EU4Province::start(); eu4Prov != EU4Province::final(); ++eu4Prov) {
    if (0 == (*eu4Prov)->numCKProvinces()) continue; // ROTW or water.
    map<EU4Country*, double> weights;
    double rebelWeight = 0;
    string debugName;
    for (CK2Province::Iter ck2Prov = (*eu4Prov)->startProv(); ck2Prov != (*eu4Prov)->finalProv(); ++ck2Prov) {
      CK2Title* countyTitle = (*ck2Prov)->getCountyTitle();
      if (!countyTitle) {
	Logger::logStream(LogStream::Warn) << "Could not find county title of CK province "
					   << (*ck2Prov)->getName()
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
	Logger::logStream("provinces") << countyTitle->getName()
				       << " assigned to "
				       << eu4country->getName()
				       << " due to " << (titleToUse == primary ? "regular liege chain to primary " : "being de-jure in union title ")
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
	  rebelWeight += (*ck2Prov)->getWeight(ProvinceWeight::Manpower);
	  Logger::logStream("provinces") << countyTitle->getName()
					 << " assigned "
					 << eu4country->getName()
					 << " from war - assumed rebel.\n";
	}
      }
      if (!eu4country) {
	// Same dynasty.
	CK2Ruler* biggest = 0;
	string dynasty = ruler->safeGetString("dynasty", "dsa");
	for (CK2Ruler::Iter cand = CK2Ruler::start(); cand != CK2Ruler::final(); ++cand) {
	  if (!(*cand)->getEU4Country()) continue;
	  if (dynasty != (*cand)->safeGetString("dynasty", "fds")) continue;
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
      weights[eu4country] += (*ck2Prov)->getWeight(ProvinceWeight::Manpower);
      debugName += createString("%s (%.1f) ", countyTitle->getName().c_str(), (*ck2Prov)->getWeight(ProvinceWeight::Manpower));
    }
    if (0 == weights.size()) {
      string ownerTag = remQuotes((*eu4Prov)->safeGetString("owner"));
      Logger::logStream(LogStream::Warn) << "Could not find any candidates for "
					 << nameAndNumber(*eu4Prov)
					 << ", it will convert belonging to "
					 << ownerTag
					 << "\n";
      EU4Country* inputOwner = EU4Country::findByName(ownerTag);
      if (inputOwner) inputOwner->addProvince(*eu4Prov);
      continue;
    }
    EU4Country* best = 0;
    double highest = -1;
    for (map<EU4Country*, double>::iterator cand = weights.begin(); cand != weights.end(); ++cand) {
      if (cand->second < highest) continue;
      best = cand->first;
      highest = cand->second;
    }
    Logger::logStream("provinces") << nameAndNumber(*eu4Prov) << " assigned to " << best->getName() << "\n";
    if (debugNames) (*eu4Prov)->resetLeaf("name", addQuotes(debugName));
    (*eu4Prov)->assignCountry(best);
    best->setAsCore(*eu4Prov);
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
    for (vector<string>::iterator tag = convertingTags.begin(); tag != convertingTags.end(); ++tag) {
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
    if ((dlc =="Art of War") || (dlc == "Common Sense") || (dlc == "Conquest of Paradise") || (dlc == "The Cossacks")) {
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
      Logger::logStream("war") << "Skipping " << warName << " because no defenders converted.\n";
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
      Logger::logStream("war") << "Skipping " << warName << " because no attackers converted.\n";
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
      Logger::logStream("war") << "Skipping " << warName << " because no disputed title.\n";
      rebelCandidates.push_back(*ckWar);
      continue;
    }
    string disputedTag = remQuotes(disputedTitle->safeGetString("title", QuotedNone));
    CK2Title* title = CK2Title::findByName(disputedTag);
    if (!title) {
      Logger::logStream("war") << "Skipping " << warName << ", could not identify disputed title " << disputedTag << ".\n";
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
    Object* startDate = history->getNeededObject(eu4Game->safeGetString("date", "1444.1.1"));
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
      Logger::logStream("war") << "Skipping " << warName << ", could not find province from " << disputedTag << ".\n";
      continue;
    }

    Logger::logStream("war") << "Converting " << warName << " with target province " << targetProvince << ".\n";
    eu4Game->setValue(euWar, before);
    Object* wargoal = history->getNeededObject("war_goal");
    wargoal->setLeaf("type", "\"take_claim\"");
    wargoal->setLeaf("province", targetProvince);
    wargoal->setLeaf("casus_belli", "\"cb_conquest\"");
    Object* takeProvince = euWar->getNeededObject("take_province");
    takeProvince->setLeaf("type", "\"take_claim\"");
    takeProvince->setLeaf("province", targetProvince);
    takeProvince->setLeaf("casus_belli", "\"cb_conquest\"");
  }

  objvec factions = eu4Game->getValue("rebel_faction");
  for (objiter faction = factions.begin(); faction != factions.end(); ++faction) {
    EU4Country* country = EU4Country::findByName(remQuotes((*faction)->safeGetString("country", QuotedNone)));
    if (!country) continue;
    if (!country->converts()) continue;
    eu4Game->removeObject(*faction);
  }

  Object* cbConversion = configObject->getNeededObject("rebel_faction_types");
  before = eu4Game->safeGetObject("religions");
  string birthDate = eu4Game->safeGetString("date", "1444.11.11");
  objvec generalSkills = configObject->getNeededObject("generalSkills")->getLeaves();
  Object* rebelCountry = eu4Game->getNeededObject("countries")->safeGetObject("REB");
  if (!rebelCountry) {
    Logger::logStream(LogStream::Info) << "Could not find rebel country, no rebel factions created.\n" << LogOption::Undent;
    return true;
  }
  for (CK2War::Iter cand = rebelCandidates.begin(); cand != rebelCandidates.end(); ++cand) {
    string ckCasusBelli = remQuotes((*cand)->getNeededObject("casus_belli")->safeGetString("casus_belli", QuotedNone));
    string euRevoltType = cbConversion->safeGetString(ckCasusBelli, PlainNone);
    string warName = (*cand)->safeGetString("name");
    if (euRevoltType == PlainNone) {
      Logger::logStream("war") << "Not making " << warName << " a rebellion due to CB " << ckCasusBelli << ".\n";
      continue;
    }

    objvec ckDefenders = (*cand)->getValue("defender");
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
      Logger::logStream("war") << "Skipping " << warName << " because no defenders converted.\n";
      continue;
    }

    Object* faction = new Object("rebel_faction");
    Object* factionId = createTypedId("rebel", "50");
    faction->setValue(factionId);
    faction->setLeaf("fraction", "0.000");
    faction->setLeaf("type", addQuotes(euRevoltType));
    faction->setLeaf("name", warName);
    faction->setLeaf("progress", "0.000");
    string euReligion = target->safeGetString("religion");
    faction->setLeaf("heretic", configObject->getNeededObject("rebel_heresies")->safeGetString(euReligion, "Lollard"));
    faction->setLeaf("country", addQuotes(target->getKey()));
    faction->setLeaf("independence", "\"\"");
    faction->setLeaf("sponsor", "\"---\"");
    faction->setLeaf("religion", addQuotes(euReligion));
    faction->setLeaf("government", addQuotes(target->safeGetString("government")));
    faction->setLeaf("province", target->safeGetString("capital"));
    faction->setLeaf("seed", "931983089");
    CK2Ruler* ckRebel = CK2Ruler::findByName((*cand)->safeGetString("attacker"));
    if (!ckRebel) {
      Logger::logStream("war") << "Skipping " << warName << " for lack of a leader.\n";
      continue;
    }
    Logger::logStream("war") << "Converting " << warName << " as rebel type " << euRevoltType << ".\n";
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
    Object* rebelDemesne = ckRebel->safeGetObject(demesneString);
    set<EU4Province*> eu4Provinces;
    double rebelTroops = 0;
    if (rebelDemesne) {
      objvec rebelArmies = rebelDemesne->getValue("army");
      for (objiter rebelArmy = rebelArmies.begin(); rebelArmy != rebelArmies.end(); ++rebelArmy) {
	objvec units = (*rebelArmy)->getValue("sub_unit");
	for (objiter unit = units.begin(); unit != units.end(); ++unit) {
	  rebelTroops += calculateTroopWeight((*unit)->getNeededObject("troops"), 0);
	}
	CK2Province* ckLocation = CK2Province::findByName((*rebelArmy)->safeGetString("location", PlainNone));
	if (!ckLocation) continue;
	for (EU4Province::Iter eu4prov = ckLocation->startEU4Province(); eu4prov != ckLocation->finalEU4Province(); ++eu4prov) {
	  eu4Provinces.insert(*eu4prov);
	}
      }
    }
    if (!eu4Provinces.empty()) rebelLocation = (*eu4Provinces.begin())->getKey();
    army->setLeaf("location", rebelLocation);
    int rebelRegiments = (int) floor(rebelTroops * configObject->safeGetFloat("regimentsPerTroop") + 0.5);
    if (rebelRegiments < 3) rebelRegiments = 3;
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
    for (set<EU4Province*>::iterator eu4prov = eu4Provinces.begin(); eu4prov != eu4Provinces.end(); ++eu4prov) {
      provinces->addToList((*eu4prov)->getKey());
      possibles->addToList((*eu4prov)->getKey());
      (*eu4prov)->setValue(provinceFactionId);
    }
  }

  Object* religions = configObject->safeGetObject("dynamicReligions");
  if (!religions) {
    Logger::logStream(LogStream::Warn) << "Could not find dynamic religions, skipping heretic rebels.\n";
  }
  else {
    for (EU4Country::Iter eu4country = EU4Country::start(); eu4country != EU4Country::final(); ++eu4country) {
      if (!(*eu4country)->converts()) continue;
      Logger::logStream("war") << "Searching for heresies in " << (*eu4country)->getKey() << ":\n" << LogOption::Indent;
      CK2Ruler* ruler = (*eu4country)->getRuler();
      string ckReligion = ruler->getBelief("religion");
      string euReligion = religions->safeGetString(ckReligion);
      set<EU4Province*> eu4Provinces;
      for (EU4Province::Iter eu4prov = (*eu4country)->startProvince(); eu4prov != (*eu4country)->finalProvince(); ++eu4prov) {
	for (CK2Province::Iter ck2prov = (*eu4prov)->startProv(); ck2prov != (*eu4prov)->finalProv(); ++ck2prov) {
	  string provCkReligion = (*ck2prov)->safeGetString("religion");
	  string provEuReligion = religions->safeGetString(provCkReligion);
	  if ((provEuReligion == euReligion) && (provCkReligion != ckReligion)) {
	    Logger::logStream("war") << nameAndNumber(*eu4prov) << " based on " << provCkReligion
				     << " in " << nameAndNumber(*ck2prov) << ".\n";
	    eu4Provinces.insert(*eu4prov);
	  }
	}
      }
      Logger::logStream("war") << LogOption::Undent;
      if (eu4Provinces.empty()) continue;
      Object* faction = new Object("rebel_faction");
      Object* factionId = createTypedId("rebel", "50");
      faction->setValue(factionId);
      faction->setLeaf("fraction", "0.000");
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

  if (!createCK2Objects()) return;
  if (!createEU4Objects()) return;
  if (!createProvinceMap()) return;
  if (!createCountryMap()) return;
  if (!resetHistories()) return;
  if (!calculateProvinceWeights()) return;
  if (!transferProvinces()) return;
  if (!setCores()) return;
  if (!moveCapitals()) return;
  if (!modifyProvinces()) return;
  if (!moveBuildings()) return;
  if (!setupDiplomacy()) return;
  if (!adjustBalkanisation()) return;
  if (!cleanEU4Nations()) return;
  if (!createArmies()) return;
  if (!createNavies()) return;  
  if (!cultureAndReligion()) return;
  if (!createGovernments()) return;
  if (!createCharacters()) return;
  if (!redistributeMana()) return;
  if (!hreAndPapacy()) return;
  if (!warsAndRebels()) return;
  cleanUp();

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

