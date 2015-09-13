#include <algorithm>
#include <cstdio>
#include <direct.h>
#include <deque>
#include <iostream> 
#include <string>
#include <set>

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

/*
 * Wars
 * Armies, fleets
 * Trade?
 * Religion
 * Known provinces
 * Advisors
 * Rulers
 * Bonus events
 * Governments
 * Unions?
 * Cultures
 * Base tax
 * Manpower
 * Autonomy
 * Cores
 * Liberty desire
 * Claims
 * Buildings
 * Starting money, manpower, ADM
 * Techs
 * Generals
 * Rebels
 */

ConverterJob const* const ConverterJob::Convert = new ConverterJob("convert", false);
ConverterJob const* const ConverterJob::DebugParser = new ConverterJob("debug", false);
ConverterJob const* const ConverterJob::LoadFile = new ConverterJob("loadfile", true);

const string QuotedNone("\"none\"");
const string PlainNone("none");

Converter::Converter (Window* ow, string fn)
  : ck2FileName(fn)
  , ck2Game(0)
  , eu4Game(0)
  , configObject(0)
  , deJureObject(0)
  , provinceMapObject(0)
  , countryMapObject(0)
  , customObject(0)
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
    if (ConverterJob::Convert     == job) convert();
    if (ConverterJob::DebugParser == job) debugParser();
    if (ConverterJob::LoadFile    == job) loadFile();
  }
}

void Converter::loadFile () {
  if (ck2FileName == "") return;
  Parser::ignoreString = "CK2text";
  ck2Game = loadTextFile(ck2FileName);
  Parser::ignoreString = "";
  Logger::logStream(LogStream::Info) << "Ready to convert.\n";
}

void Converter::cleanUp () {
  // Restore the initial '-' in province tags.
  string minus("-");
  for (EU4Province::Iter prov = EU4Province::start(); prov != EU4Province::final(); ++prov) {
    (*prov)->object->setKey(minus + (*prov)->getKey());
  }
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
      for (objiter str = streams.begin(); str != streams.end(); ++str) {
	string str_name = (*str)->getKey();
	Logger::logStream(LogStream::findByName(str_name)).setActive(true);
      }
    }
  }
}

void Converter::debugParser () {
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


/********************************  End helpers  **********************/

/******************************** Begin initialisers **********************/

bool Converter::createCK2Objects () {
  Logger::logStream(LogStream::Info) << "Creating CK2 objects\n" << LogOption::Indent;
  Object* wrapperObject = ck2Game->safeGetObject("provinces");
  if (!wrapperObject) {
    Logger::logStream(LogStream::Error) << "Could not find provinces object, cannot continue.\n" << LogOption::Undent;
    return false;
  }

  objvec provinces = wrapperObject->getLeaves();
  for (objiter province = provinces.begin(); province != provinces.end(); ++province) {
    new CK2Province(*province);
  }
  Logger::logStream(LogStream::Info) << "Created " << CK2Province::totalAmount() << " CK2 provinces.\n";

  wrapperObject = ck2Game->safeGetObject("title");
  if (!wrapperObject) {
    Logger::logStream(LogStream::Error) << "Could not find title object, cannot continue.\n" << LogOption::Undent;
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
    Logger::logStream(LogStream::Error) << "Could not find character object, cannot continue.\n" << LogOption::Undent;
    return false;
  }
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
      ruler = new CK2Ruler(character);
    }

    ruler->addTitle(*ckCountry);
  }

  Logger::logStream(LogStream::Info) << "Created " << CK2Ruler::totalAmount() << " CK2 rulers.\n";
  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    (*ruler)->createLiege();
  }

  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    (*ruler)->countBaronies();
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
      Logger::logStream(LogStream::Warn) << "Could not find ruler "
					 << mainCharTag
					 << " from diplo object "
					 << (*relation)
					 << "\n";
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
    new EU4Country(*country);
  }
  Logger::logStream(LogStream::Info) << "Created " << EU4Country::totalAmount() << " countries.\n";
 
  Logger::logStream(LogStream::Info) << "Done with EU4 objects.\n" << LogOption::Undent;
  return true;
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
    if (!ckCountry) {
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
    euCountry->setRuler(ckCountry->getRuler());
    forbiddenCountries.insert(euCountry);
    Logger::logStream(LogStream::Info) << "Converting "
				       << euCountry->getName() << " from "
				       << ckCountry->getRuler()->getName() << " " << ckCountry->getRuler()->safeGetString("birth_name")
				       << " due to custom override.\n";
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
      EU4Country* eu4Country = EU4Country::findByName(eutag);
      if (!eu4Country) {
	Logger::logStream(LogStream::Warn) << "Could not find EU4 country "
					   << eutag
					   << ", wanted for having discovered "
					   << nameAndNumber(*known)
					   << ".\n";
	continue;
      }
      if (forbiddenCountries.count(eu4Country)) continue;
      candidateCountries.insert(eu4Country);
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

  CK2Ruler::Container rulers = CK2Ruler::makeCopy();  
  sort(rulers.begin(), rulers.end(), ObjectDescendingSorter("totalRealmBaronies"));
  deque<CK2Ruler*> rulerQueue;
  for (CK2Ruler::Iter ruler = rulers.begin(); ruler != rulers.end(); ++ruler) {
    if (0 == (*ruler)->countBaronies()) break; // Rebels, adventurers, and suchlike riffraff.
    if ((*ruler)->getEU4Country()) continue;   // Already converted.
    rulerQueue.push_back(*ruler);
  }
  set<CK2Ruler*> attempted;
  while (!rulerQueue.empty()) {
    CK2Ruler* ruler = rulerQueue.front();
    rulerQueue.pop_front();
    EU4Country* bestCandidate = 0;
    vector<pair<string, string> > bestOverlapList;
    for (set<EU4Country*>::iterator cand = candidateCountries.begin(); cand != candidateCountries.end(); ++cand) {
      vector<EU4Province*> eu4Provinces = initialProvincesMap[*cand];
      vector<pair<string, string> > currOverlapList;
      for (vector<EU4Province*>::iterator eu4Province = eu4Provinces.begin(); eu4Province != eu4Provinces.end(); ++eu4Province) {
	for (CK2Province::Iter ck2Prov = (*eu4Province)->startProv(); ck2Prov != (*eu4Province)->finalProv(); ++ck2Prov) {
	  CK2Title* countyTitle = (*ck2Prov)->getCountyTitle();
	  if (ruler->hasTitle(countyTitle, true /* includeVassals */)) {
	    currOverlapList.push_back(pair<string, string>(countyTitle->getName(), nameAndNumber(*eu4Province)));
	  }
	}
      }
      if ((0 < bestOverlapList.size()) && (currOverlapList.size() <= bestOverlapList.size())) continue;
      bestCandidate = (*cand);
      bestOverlapList = currOverlapList;
    }
    if ((0 == bestOverlapList.size()) && (!attempted.count(ruler))) {
      rulerQueue.push_back(ruler);
      attempted.insert(ruler);
      continue;
    }
    
    Logger::logStream(LogStream::Info) << "Converting "
				       << bestCandidate->getName() << " from "
				       << ruler->getName() << " " << ruler->safeGetString("birth_name")
				       << " based on overlap " << bestOverlapList.size()
				       << " with these counties: ";
    for (vector<pair<string, string> >::iterator overlap = bestOverlapList.begin(); overlap != bestOverlapList.end(); ++overlap) {
      Logger::logStream(LogStream::Info) << (*overlap).first << " -> " << (*overlap).second << " ";
    }
    Logger::logStream(LogStream::Info) << "\n";
    bestCandidate->setRuler(ruler);

    candidateCountries.erase(bestCandidate);
    if (0 == candidateCountries.size()) break;
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
    if (QuotedNone == baronytag) {
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

  string overrideFileName = remQuotes(configObject->safeGetString("custom", QuotedNone));
  if (PlainNone != overrideFileName) customObject = loadTextFile(dirToUse + overrideFileName);
  else customObject = new Object("custom");

  countryMapObject = customObject->getNeededObject("country_overrides");
  
  Logger::logStream(LogStream::Info) << "Done loading input files\n" << LogOption::Undent;
}

/******************************* End initialisers *******************************/ 


/******************************* Begin conversions ********************************/

bool Converter::calculateProvinceWeights () {
  Logger::logStream(LogStream::Info) << "Beginning province weight calculations.\n" << LogOption::Indent;

  // Add merchant-family houses to provinces.
  for (CK2Title::Iter title = CK2Title::start(); title != CK2Title::final(); ++title) {
    if (TitleLevel::Barony != (*title)->getLevel()) continue;
    Object* settlement = (*title)->safeGetObject("settlement");
    if (!settlement) continue;
    if (settlement->safeGetString("type") != "family_palace") continue;
    CK2Title* liegeTitle = (*title)->getLiege();
    if (!liegeTitle) continue;
    CK2Ruler* liege = liegeTitle->getRuler();
    Object* demesne = liege->safeGetObject("demesne");
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
  }

  map<string, int> castleBuildings;
  map<string, int> templeBuildings;
  map<string, int> cityBuildings;
  map<string, double> troopTypes;
  for (CK2Province::Iter ck2prov = CK2Province::start(); ck2prov != CK2Province::final(); ++ck2prov) {
    for (objiter barony = (*ck2prov)->startBarony(); barony != (*ck2prov)->finalBarony(); ++barony) {
      string baronyType = (*barony)->safeGetString("type", PlainNone);
      if (PlainNone == baronyType) continue;
      map<string, int>* buildings = 0;
      string prefix;
      if (baronyType == "city") {
	prefix = "ct_";
	buildings = &cityBuildings;
      }
      else if (baronyType == "temple") {
	prefix = "tp_";
	buildings = &templeBuildings;
      }
      else if (baronyType == "castle") {
	prefix = "ca_";
	buildings = &castleBuildings;
      }
      else if (baronyType == "tradepost") {
	continue;
      }
      else {
	Logger::logStream(LogStream::Warn) << "Could not determine type of barony "
					   << (*barony)->getKey() << " in province "
					   << nameAndNumber(*ck2prov) << "\n";
	continue;
      }
      objvec leaves = (*barony)->getLeaves();
      for (objiter leaf = leaves.begin(); leaf != leaves.end(); ++leaf) {
	if (!hasPrefix(prefix, (*leaf)->getKey())) continue;
	(*buildings)[(*leaf)->getKey()]++;
      }
      Object* levyObject = (*barony)->safeGetObject("levy");
      if (!levyObject) continue;
      objvec levies = levyObject->getLeaves();
      for (objiter levy = levies.begin(); levy != levies.end(); ++levy) {
	troopTypes[(*levy)->getKey()] += (*levy)->tokenAsFloat(1);
      }
    }
  }

  Object* weightObject = configObject->getNeededObject("buildings");
  for (CK2Province::Iter ck2prov = CK2Province::start(); ck2prov != CK2Province::final(); ++ck2prov) {
    for (objiter barony = (*ck2prov)->startBarony(); barony != (*ck2prov)->finalBarony(); ++barony) {
      string baronyType = (*barony)->safeGetString("type", PlainNone);
      if (PlainNone == baronyType) continue;
      map<string, int>* buildings = 0;
      if (baronyType == "city") buildings = &cityBuildings;
      else if (baronyType == "temple") buildings = &templeBuildings;
      else if (baronyType == "castle") buildings = &castleBuildings;
      else continue;
      Object* weights = weightObject->getNeededObject(baronyType);
      double prodMultiplier = weights->safeGetFloat("prod", 0.5);
      double taxMultiplier  = weights->safeGetFloat("tax", 0.5);
      double buildingWeight = 0;
      for (map<string, int>::iterator b = buildings->begin(); b != buildings->end(); ++b) {
	if ((*barony)->safeGetString((*b).first, "no") == "no") continue;
	buildingWeight += 1.0 / (*b).second;
      }
      double prodWeight = buildingWeight * prodMultiplier;
      double taxWeight  = buildingWeight * taxMultiplier;
      Object* levyObject = (*barony)->safeGetObject("levy");
      double mpWeight = 0;
      if (levyObject) {
	objvec levies = levyObject->getLeaves();
	for (objiter levy = levies.begin(); levy != levies.end(); ++levy) {
	  string key = (*levy)->getKey();
	  double amount = (*levy)->tokenAsFloat(1);
	  mpWeight += amount / troopTypes[key];
	}
      }
      (*barony)->setLeaf(ProvinceWeight::Production->getName(), prodWeight);
      (*barony)->setLeaf(ProvinceWeight::Taxation->getName(), taxWeight);
      (*barony)->setLeaf(ProvinceWeight::Manpower->getName(), mpWeight);
    }
    Logger::logStream("provinces") << nameAndNumber(*ck2prov)
				   << " has weights production "
				   << (*ck2prov)->getWeight(ProvinceWeight::Production)
				   << ", taxation "
				   << (*ck2prov)->getWeight(ProvinceWeight::Taxation)
				   << ", manpower "
				   << (*ck2prov)->getWeight(ProvinceWeight::Manpower)
				   << ".\n";
  }

  Logger::logStream(LogStream::Info) << "Done with province weight calculations.\n" << LogOption::Undent;
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
    double amount = max(1.0, floor(provTaxWeight + 0.5));
    (*eu4prov)->resetLeaf("base_tax", amount); afterTax += amount;
    amount = max(1.0, floor(provProWeight + 0.5));
    (*eu4prov)->resetLeaf("base_production", amount); afterPro += amount;
    amount = max(1.0, floor(provManWeight + 0.5));
    (*eu4prov)->resetLeaf("base_manpower", amount); afterMen += amount;
  }

  Logger::logStream("provinces") << "After distribution: "
				 << afterTax << " base tax, "
				 << afterPro << " base production, "
				 << afterMen << " base manpower.\n";

  Logger::logStream(LogStream::Info) << "Done modifying provinces.\n" << LogOption::Undent;
  return true;
}

bool Converter::setupDiplomacy () {
  Logger::logStream(LogStream::Info) << "Beginning diplomacy.\n" << LogOption::Indent;
  Object* euDipObject = eu4Game->getNeededObject("diplomacy");
  euDipObject->clear();
  //Object* ckDipObject = ck2Game->getNeededObject("relation");

  for (CK2Ruler::Iter ruler = CK2Ruler::start(); ruler != CK2Ruler::final(); ++ruler) {
    if (!(*ruler)->getEU4Country()) continue;
    CK2Ruler* liege = (*ruler)->getLiege();
    while (liege) {
      if (liege->getEU4Country()) break;
      liege = liege->getLiege();
    }
    if (!liege) continue;
    Object* vassal = new Object("vassal");
    euDipObject->setValue(vassal);
    vassal->setLeaf("first", addQuotes(liege->getEU4Country()->getName()));
    vassal->setLeaf("second", addQuotes((*ruler)->getEU4Country()->getName()));
    vassal->setLeaf("end_date", "1836.1.1");
    vassal->setLeaf("cancel", "no");
    vassal->setLeaf("start_date", remQuotes(ck2Game->safeGetString("date")));
    Logger::logStream("diplomacy") << (*ruler)->getEU4Country()->getName()
				   << " is vassal of "
				   << liege->getEU4Country()->getName()
				   << " based on characters "
				   << (*ruler)->getKey() << " " << (*ruler)->safeGetString("birth_name")
				   << " and "
				   << liege->getKey() << " " << liege->safeGetString("birth_name")
				   << ".\n";
  }
  
  Logger::logStream(LogStream::Info) << "Done with diplomacy.\n" << LogOption::Undent;
  return true;
}
  
bool Converter::transferProvinces () {
  Logger::logStream(LogStream::Info) << "Beginning province transfer.\n" << LogOption::Indent;

  for (EU4Province::Iter eu4Prov = EU4Province::start(); eu4Prov != EU4Province::final(); ++eu4Prov) {
    if (0 == (*eu4Prov)->numCKProvinces()) continue; // ROTW or water.
    map<EU4Country*, double> weights;
    double rebelWeight = 0;
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
      bool printed = false;
      while (!ruler->getEU4Country()) {
	ruler = ruler->getLiege();
	if (!ruler) break;
      }
      if (ruler) {
	Logger::logStream("provinces") << countyTitle->getName()
				       << " assigned "
				       << ruler->getEU4Country()->getName()
				       << " from regular liege chain.\n";
	printed = true;
      }
      else {
	// We didn't find an EU4 nation in the
	// chain of lieges. Possibly this is a
	// rebel-controlled county.
	ruler = countyTitle->getRuler();
	CK2Ruler* cand = 0;
	for (CK2Ruler::Iter enemy = ruler->startEnemy(); enemy != ruler->finalEnemy(); ++enemy) {
	  if (!(*enemy)->getEU4Country()) continue;
	  cand = (*enemy); // Intuitively would assign to ruler, but that breaks the iterator, despite the exit below.
	  rebelWeight += (*ck2Prov)->getWeight(ProvinceWeight::Manpower);
	  break;
	}
	ruler = cand;
      }
      if (ruler) {
	if (!printed) Logger::logStream("provinces") << countyTitle->getName()
						     << " assigned "
						     << ruler->getEU4Country()->getName()
						     << " from war - assumed rebel.\n";
	printed = true;
      }
      else {
	// Not a rebel. Might be an independent count. Try for de-jure liege.
	CK2Title* currTitle = countyTitle;
	while (currTitle) {
	  currTitle = currTitle->getDeJureLiege();
	  if (!currTitle) break;
	  ruler = currTitle->getRuler();
	  if (!ruler) continue;
	  if (ruler->getEU4Country()) break;
	  ruler = 0;
	}
      }
      if (ruler) {
	if (!printed) Logger::logStream("provinces") << countyTitle->getName()
						     << " assigned "
						     << ruler->getEU4Country()->getName()
						     << " from de-jure liege chain.\n";
	printed = true;
      }
      else {
	// Try for tribute overlord.
	CK2Ruler* suzerain = countyTitle->getRuler()->getSuzerain();
	while (suzerain) {
	  if (suzerain->getEU4Country()) {
	    ruler = suzerain;
	    break;
	  }
	  suzerain = suzerain->getSuzerain();
	}
      }
      if (ruler) {
	if (!printed) Logger::logStream("provinces") << countyTitle->getName()
						     << " assigned "
						     << ruler->getEU4Country()->getName()
						     << " from tributary chain.\n";
	printed = true;
      }
      else {
	// Same dynasty.
 	CK2Ruler* original = countyTitle->getRuler();
	string dynasty = original->safeGetString("dynasty", "dsa");
	for (CK2Ruler::Iter cand = CK2Ruler::start(); cand != CK2Ruler::final(); ++cand) {
	  if (!(*cand)->getEU4Country()) continue;
	  if (dynasty != (*cand)->safeGetString("dynasty", "fds")) continue;
	  if ((*cand) == original) continue;
	  if ((ruler) && (ruler->countBaronies() > (*cand)->countBaronies())) continue;
	  ruler = (*cand);
	}
      }
      if (ruler) {
	if (!printed) Logger::logStream("provinces") << countyTitle->getName()
						     << " assigned "
						     << ruler->getEU4Country()->getName()
						     << " from same dynasty.\n";
	printed = true;
      }
      else {
	Logger::logStream(LogStream::Warn) << "County " << countyTitle->getName()
					   << " doesn't have an assigned EU4 nation; skipping.\n";
	continue;
      }
      weights[ruler->getEU4Country()] += (*ck2Prov)->getWeight(ProvinceWeight::Manpower);
    }
    if (0 == weights.size()) {
      Logger::logStream(LogStream::Warn) << "Could not find any candidates for "
					 << nameAndNumber(*eu4Prov)
					 << ", it will convert belonging to "
					 << (*eu4Prov)->safeGetString("owner")
					 << "\n";
      continue;
    }
    EU4Country* best = 0;
    double highest = -1;
    for (map<EU4Country*, double>::iterator cand = weights.begin(); cand != weights.end(); ++cand) {
      if ((*cand).second < highest) continue;
      best = (*cand).first;
      highest = (*cand).second;
    }
    Logger::logStream("provinces") << nameAndNumber(*eu4Prov) << " assigned to " << best->getName() << "\n";
    (*eu4Prov)->resetLeaf("owner", addQuotes(best->getName()));
    (*eu4Prov)->resetLeaf("controller", addQuotes(best->getName()));
  }


  Logger::logStream(LogStream::Info) << "Done with province transfer.\n" << LogOption::Undent;
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
  if (!createCK2Objects()) return;
  if (!createEU4Objects()) return;
  if (!createProvinceMap()) return; 
  if (!createCountryMap()) return;
  if (!calculateProvinceWeights()) return;
  if (!transferProvinces()) return;
  if (!modifyProvinces()) return;
  if (!setupDiplomacy()) return;
  cleanUp();
  
  Logger::logStream(LogStream::Info) << "Done with conversion, writing to Output/converted.eu4.\n";

  // Last leaf needs special treatment.
  objvec leaves = eu4Game->getLeaves();
  Object* final = leaves.back();
  eu4Game->removeObject(final);
  
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

