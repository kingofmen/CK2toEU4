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
class CK2Ruler;
class EU4Country;
class Logger;

using namespace std;

class ConverterJob : public Enumerable<const ConverterJob> {
public:
  ConverterJob (string n, bool final) : Enumerable<const ConverterJob>(this, n, final) {}
  static ConverterJob const* const Convert;
  static ConverterJob const* const DebugParser;
  static ConverterJob const* const DejureLieges;
  static ConverterJob const* const CheckProvinces;
  static ConverterJob const* const LoadFile;
  static ConverterJob const* const PlayerWars;
  static ConverterJob const* const Statistics;
  static ConverterJob const* const DynastyScores;
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
  bool adjustBalkanisation ();
  void calculateDynasticScores ();
  bool calculateProvinceWeights ();
  bool cleanEU4Nations ();
  bool createArmies ();
  bool createCharacters ();
  bool createGovernments ();
  bool createNavies ();  
  bool cultureAndReligion ();
  bool displayStats ();
  bool hreAndPapacy ();
  bool modifyProvinces ();
  bool moveBuildings ();
  bool moveCapitals ();
  bool redistributeMana ();
  bool resetHistories ();
  bool setCores ();
  bool setupDiplomacy ();
  bool transferProvinces ();
  bool warsAndRebels ();
  bool greatWorks ();

  // Infrastructure
  void loadFile ();
  void checkProvinces ();
  void convert ();
  void debugParser ();
  void dynastyScores ();
  void playerWars ();
  void configure ();
  void dejures ();
  void statistics ();

  // Initialisers
  bool createCK2Objects ();
  bool createEU4Objects ();
  bool createCountryMap ();
  bool createProvinceMap ();
  void loadFiles ();
  void setDynastyNames (Object* dynastyNames);

  // Helpers:
  string getConversionDate(int add_years);
  string getConversionDate(const string& ck2Date);
  double calculateTroopWeight (Object* levy, Logger* logstream = 0, int idx = 1);
  void cleanUp ();
  Object* createMonarchId ();
  Object* createTypedId (string keyword, string idType);
  Object* createUnitId (string unitType);
  Object* loadTextFile (string fname);
  bool makeAdvisor(CK2Character* councillor, Object* country_advisors,
                   objvec& advisorTypes, string& birthDate, Object* history,
                   string capitalTag, map<string, objvec>& allAdvisors);
  void makeLeader(Object* eu4country, const string& keyword, CK2Character* base,
                  const objvec& generalSkills, const string& birthDate);
  Object* makeMonarch(const string& capitalTag, CK2Character* ruler,
                      CK2Ruler* king, const string& keyword,
                      Object* bonusTraits);
  Object* makeMonarchObject(const string& capitalTag, CK2Character* ruler,
                            const string& keyword, Object* bonusTraits);
  bool rankProvinceDevelopment();
  bool redistributeDevelopment();
  bool swapKeys(Object* one, Object* two, string key);

  // Input info
  Object* advisorTypesObject;
  Object* ckBuildingObject;
  Object* ckBuildingWeights;
  Object* configObject;
  Object* countryMapObject;
  Object* customObject;
  Object* deJureObject;
  Object* euBuildingObject;
  Object* provinceMapObject;

  Window* outputWindow;
};

#endif

