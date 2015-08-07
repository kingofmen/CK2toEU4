#include <cstdio> 
#include <iostream> 
#include <string>
#include <set>
#include <algorithm>
#include <direct.h>

#include "Parser.hh"
#include "StructUtils.hh" 
#include "StringManips.hh"
#include "UtilityFunctions.hh"


#include "Converter.hh"
#include "Logger.hh"

using namespace std;

Converter::Converter (string fn, TaskType atask)
  : targetVersion("")
  , sourceVersion("")
  , fname(fn)
  , ck2Game(0)
  , eu4Game(0)
  , task(LoadFile)
  , configObject(0)
  , autoTask(atask)
  , provinceMapObject(0)
  , countryMapObject(0)
  , customObject(0)
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
  switch (task) {
  case LoadFile: loadFile(fname); break;
  case Convert: convert(); break;
  case NumTasks: 
  default: break; 
  }
}

void Converter::loadFile (string fname) {
  ck2Game = loadTextFile(fname);
  if (NumTasks != autoTask) {
    task = autoTask; 
    switch (autoTask) {
    case Convert:
      convert(); 
      break;
    default:
      break;
    }
  }
  Logger::logStream(Logger::Game) << "Ready to convert.\n";
}

void Converter::cleanUp () {
}

void Converter::configure () {
  configObject = processFile("config.txt");
  targetVersion = configObject->safeGetString("hoidir", targetVersion);
  sourceVersion = configObject->safeGetString("vicdir", sourceVersion);
  Logger::logStream(Logger::Debug).setActive(false);

  Object* debug = configObject->safeGetObject("debug");
  if (debug) {
    if (debug->safeGetString("generic", "no") == "yes") Logger::logStream(Logger::Debug).setActive(true);
    bool activateAll = (debug->safeGetString("all", "no") == "yes");
    for (int i = DebugLeaders; i < NumDebugs; ++i) {
      sprintf(strbuffer, "%i", i);
      if ((activateAll) || (debug->safeGetString(strbuffer, "no") == "yes")) Logger::logStream(i).setActive(true);
    }
  }
}

Object* Converter::loadTextFile (string fname) {
  Logger::logStream(Logger::Game) << "Parsing file " << fname << "\n";
  ifstream reader;
  reader.open(fname.c_str());
  if ((reader.eof()) || (reader.fail())) {
    Logger::logStream(Logger::Error) << "Could not open file, returning null object.\n";
    return 0; 
  }
  
  Object* ret = processFile(fname);
  Logger::logStream(Logger::Game) << " ... done.\n";
  return ret; 
}

string nameAndNumber (Object* eu3prov) {
  return eu3prov->getKey() + " (" + remQuotes(eu3prov->safeGetString("name", "\"could not find name\"")) + ")";
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

bool Converter::createCountryMap () {
  if (!countryMapObject) {
    Logger::logStream(Logger::Error) << "Error: Could not find country-mapping object.\n";
    return false; 
  }

  return true; 
}

bool Converter::createProvinceMap () {
  if (!provinceMapObject) {
    Logger::logStream(Logger::Error) << "Error: Could not find province-mapping object.\n";
    return false; 
  }

  return true; 
}

void Converter::loadFiles () {
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
    Logger::logStream(Logger::Game) << "No file loaded.\n";
    return; 
  }

  Logger::logStream(Logger::Game) << "Loading HoI source file.\n";
  eu4Game = loadTextFile(targetVersion + "input.hoi3");

  loadFiles();
  if (!createProvinceMap()) return; 
  if (!createCountryMap()) return;
  cleanUp();
  
  Logger::logStream(Logger::Game) << "Done with conversion, writing to Output/converted.hoi3.\n";
 
  ofstream writer;
  writer.open(".\\Output\\converted.hoi3");
  Parser::topLevel = eu4Game;
  writer << (*eu4Game);
  writer.close();
  Logger::logStream(Logger::Game) << "Done writing.\n";
}

