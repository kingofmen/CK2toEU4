#include <cstdio> 
#include <iostream> 
#include <string>
#include <set>
#include <algorithm>
#include <direct.h>

#include "Converter.hh"
#include "Logger.hh"
#include "Parser.hh"
#include "StructUtils.hh" 
#include "StringManips.hh"
#include "UtilityFunctions.hh"
#include "Window.hh"

using namespace std;

ConverterJob const* const ConverterJob::Convert = new ConverterJob("convert", false);
ConverterJob const* const ConverterJob::LoadFile = new ConverterJob("loadfile", true);

Converter::Converter (Window* ow, string fn)
  : targetVersion("")
  , sourceVersion("")
  , ck2FileName(fn)
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
    if (ConverterJob::LoadFile == job) loadFile();
    if (ConverterJob::Convert  == job) convert();
  }
}

void Converter::loadFile () {
  if (ck2FileName == "") return;
  ck2Game = loadTextFile(ck2FileName);
  Logger::logStream(LogStream::Info) << "Ready to convert.\n";
}

void Converter::cleanUp () {
}

void Converter::configure () {
  configObject = processFile("config.txt");
  targetVersion = configObject->safeGetString("hoidir", targetVersion);
  sourceVersion = configObject->safeGetString("vicdir", sourceVersion);
  Logger::logStream(LogStream::Debug).setActive(false);

  Object* debug = configObject->safeGetObject("streams");
  if (debug) {
    objvec streams = debug->getLeaves();
    for (objiter str = streams.begin(); str != streams.end(); ++str) {
      string str_name = (*str)->getKey();
      if (!LogStream::findByName(str_name)) {
	LogStream const* new_stream = new LogStream(str_name);
	Logger* newlog = Logger::createStream(*new_stream);
	QObject::connect(newlog, SIGNAL(message(QString)), outputWindow, SLOT(message(QString)));
      }
      Logger::logStream(str_name).setActive((*str)->getLeaf() == "yes");
    }

    if (debug->safeGetString("generic", "no") == "yes") Logger::logStream(LogStream::Debug).setActive(true);
    bool activateAll = (debug->safeGetString("all", "no") == "yes");
    if (activateAll) {
      for (objiter str = streams.begin(); str != streams.end(); ++str) {
	string str_name = (*str)->getKey();
	Logger::logStream(LogStream::findByName(str_name)).setActive(true);
      }
    }
  }
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

  return true; 
}

void Converter::loadFiles () {
  string dirToUse = remQuotes(configObject->safeGetString("maps_dir", ".\\maps\\"));
  Logger::logStream(LogStream::Info) << "Directory: \"" << dirToUse << "\"\n";
  provinceMapObject = loadTextFile(dirToUse + "provinces.txt");
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

  Logger::logStream(LogStream::Info) << "Loading HoI source file.\n";
  eu4Game = loadTextFile(targetVersion + "input.hoi3");

  loadFiles();
  if (!createProvinceMap()) return; 
  if (!createCountryMap()) return;
  cleanUp();
  
  Logger::logStream(LogStream::Info) << "Done with conversion, writing to Output/converted.hoi3.\n";
 
  ofstream writer;
  writer.open(".\\Output\\converted.hoi3");
  Parser::topLevel = eu4Game;
  writer << (*eu4Game);
  writer.close();
  Logger::logStream(LogStream::Info) << "Done writing.\n";
}

