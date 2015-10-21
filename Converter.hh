#ifndef CONVERTER_HH
#define CONVERTER_HH

#include <QThread>
#include <fstream>
#include <map>
#include <string>
#include <queue>

#include "UtilityFunctions.hh"

class CK2Province;
class CK2Character;
class EU4Country;

using namespace std;

class ConverterJob : public Enumerable<const ConverterJob> {
public:
  ConverterJob (string n, bool final) : Enumerable<const ConverterJob>(this, n, final) {}
  static ConverterJob const* const Convert;
  static ConverterJob const* const DebugParser;
  static ConverterJob const* const LoadFile;
};

class Object;
class Window;
class Converter : public QThread {
public:
  Converter (Window* ow, string fname);
  ~Converter ();
  void scheduleJob (ConverterJob const* const cj) {jobsToDo.push(cj);}

protected:
  void run ();

private:
  // Misc globals
  string ck2FileName;
  Object* ck2Game;
  Object* eu4Game;
  queue<ConverterJob const*> jobsToDo;

  // Conversion processes
  bool calculateProvinceWeights ();
  bool createArmies ();
  bool createCharacters ();
  bool createGovernments ();
  bool createNavies ();  
  bool cultureAndReligion ();
  bool modifyProvinces ();
  bool moveBuildings ();
  bool moveCapitals ();
  bool redistributeMana ();
  bool resetHistories ();
  bool setCores ();
  bool setupDiplomacy ();
  bool transferProvinces ();
  
  // Infrastructure
  void loadFile ();
  void convert ();
  void debugParser ();
  void configure ();

  // Initialisers
  bool createCK2Objects ();
  bool createEU4Objects ();
  bool createCountryMap ();
  bool createProvinceMap ();
  void loadFiles ();

  // Helpers:
  void cleanUp ();
  Object* createMonarchId ();
  Object* createTypedId (string keyword, string idType);
  Object* createUnitId (string unitType);
  Object* loadTextFile (string fname);
  void makeLeader (EU4Country* eu4country, const string& keyword, CK2Character* base, const objvec& generalSkills, const string& birthDate);
  void makeMonarch (CK2Character* ruler, const string& keyword, EU4Country* country, Object* bonusTraits);
  bool swapKeys (Object* one, Object* two, string key);

  // Input info
  Object* advisorTypesObject;
  Object* ckBuildingObject;  
  Object* configObject;
  Object* countryMapObject;
  Object* customObject;
  Object* deJureObject;
  Object* euBuildingObject;
  Object* provinceMapObject;

  Window* outputWindow;
};

#endif

