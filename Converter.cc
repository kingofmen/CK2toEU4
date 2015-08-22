#include <cstdio> 
#include <iostream> 
#include <string>
#include <set>
#include <algorithm>
#include <direct.h>

#include "Converter.hh"
#include "CK2Province.hh"
#include "CK2Title.hh"
#include "EU4Province.hh"
#include "Logger.hh"
#include "Parser.hh"
#include "StructUtils.hh" 
#include "StringManips.hh"
#include "UtilityFunctions.hh"
#include "Window.hh"

using namespace std;

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
  ck2Game = loadTextFile(ck2FileName);
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
  
  Logger::logStream(LogStream::Info) << "Done with EU4 objects.\n" << LogOption::Undent;
  return true;
}

bool Converter::createCountryMap () {
  if (!countryMapObject) {
    Logger::logStream(LogStream::Error) << "Error: Could not find country-mapping object.\n";
    return false; 
  }

  
  
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
    string eu4id = provinceMapObject->safeGetString(countytag, QuotedNone);
    if (QuotedNone == eu4id) {
      Logger::logStream(LogStream::Warn) << "Could not find EU4 equivalent for province "
					 << nameAndNumber(*ckprov)
					 << ", ignoring.\n";
      continue;
    }

    EU4Province* target = EU4Province::findByName(eu4id);
    if (!target) {
      Logger::logStream(LogStream::Warn) << "Could not find EU4 province " << eu4id
					 << ", (missing DLC in input save?), skipping "
					 << nameAndNumber(*ckprov) << ".\n";
      continue;
    }
    (*ckprov)->setEu4Province(target);

    Logger::logStream("provinces") << nameAndNumber(*ckprov)
				   << " mapped to EU4 province "
				   << nameAndNumber(target)
				   << ".\n";
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

  string overrideFileName = remQuotes(configObject->safeGetString("custom", QuotedNone));
  if (PlainNone != overrideFileName) customObject = loadTextFile(dirToUse + overrideFileName);
  else customObject = new Object("custom");

  countryMapObject = customObject->getNeededObject("country_overrides");
  
  Logger::logStream(LogStream::Info) << "Done loading input files\n" << LogOption::Undent;
}

/******************************* End initialisers *******************************/ 


/******************************* Begin conversions ********************************/

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

