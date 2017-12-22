#ifndef CK2RULER_HH
#define CK2RULER_HH

#include <map>
#include <unordered_map>
#include <set>
#include <string>

#include "CK2Title.hh"
#include "UtilityFunctions.hh"

class EU4Country;

class CKAttribute : public Enumerable<CKAttribute> {
public:
  CKAttribute (string n, bool f) : Enumerable<CKAttribute>(this, n, f) {}

  static CKAttribute const* const Diplomacy;
  static CKAttribute const* const Martial;
  static CKAttribute const* const Stewardship;  
  static CKAttribute const* const Intrigue;
  static CKAttribute const* const Learning;
};

class CouncilTitle : public Enumerable<CouncilTitle> {
public:
  CouncilTitle (string n, bool f) : Enumerable<CouncilTitle>(this, n, f) {}

  static CouncilTitle const* const Chancellor;
  static CouncilTitle const* const Marshal;
  static CouncilTitle const* const Steward;
  static CouncilTitle const* const Spymaster;
  static CouncilTitle const* const Chaplain;
};

class CK2Character : public ObjectWrapper {
public:
  CK2Character (Object* obj, Object* dynasties);

  typedef vector<CK2Character*>::const_iterator CharacterIter;

  void addSpouse(CK2Character* sp) {spouses.push_back(sp);}
  void createClaims ();
  CK2Character* getAdmiral () const {return admiral;}
  CK2Character* getAdvisor (const string& title);
  double getAge (string date) const;
  CK2Character* getBestSpouse () const;
  int getAttribute (CKAttribute const* const att) const {return attributes[*att];}
  virtual string getBelief (string keyword) const;
  CK2Character* getCouncillor (CouncilTitle const* const con) const {return council[*con];}
  Object* getDynasty () const {return dynasty;}
  virtual EU4Country* getEU4Country () const {return 0;}
  CK2Character* getOldestChild () const {return oldestChild;}
  bool hasModifier (const string& mod);
  bool hasTrait (const string& t) const {return 0 != traits.count(t);}

  CharacterIter startChild () const {return children.begin();}
  CharacterIter finalChild () const {return children.end();}
  CharacterIter startCommander () const {return commanders.begin();}
  CharacterIter finalCommander () const {return commanders.end();}  
  CharacterIter startCouncillor () const {return council.begin();}
  CharacterIter finalCouncillor () const {return council.end();}
  unordered_map<string, vector<CK2Character*>>& getAdvisors() {
    return advisors;
  }

  static objvec ckTraits;
  static objvec euRulerTraits;

protected:
  CK2Character* admiral;
  vector<int> attributes;
  vector<CK2Character*> children;
  vector<CK2Character*> commanders;
  vector<CK2Character*> council;
  vector<CK2Character*> spouses;
  unordered_map<string, vector<CK2Character*> > advisors;
  Object* dynasty;
  CK2Character* oldestChild;  
  set<string> traits;
  map<string, bool> modifiers;
};

class CK2Ruler : public Enumerable<CK2Ruler>, public CK2Character {
public:
  CK2Ruler (Object* obj, Object* dynasties);

  void addEnemy (CK2Ruler* enemy) {enemies.push_back(enemy);}
  void addTitle (CK2Title* title);
  void addTributary (CK2Ruler* trib);
  int  countBaronies ();
  void createLiege ();
  int getEnemies () const {return enemies.size();}
  virtual EU4Country* getEU4Country () const {return eu4Country;}
  CK2Ruler* getLiege () {return liege;}
  CK2Ruler* getSovereignLiege ();
  CK2Title* getPrimaryTitle ();
  CK2Ruler* getSuzerain () {return suzerain;}
  bool hasTitle (CK2Title* title, bool includeVassals = false) const;
  bool isHuman () const {return (safeGetString("player", "no") == "yes");}
  bool isRebel ();
  bool isSovereign () const {return (0 == liege || (humansSovereign && isHuman()));}
  void personOfInterest (CK2Character* person);
  void setEU4Country (EU4Country* eu4) {eu4Country = eu4;}

  Container& getVassals() { return vassals; }
  CK2Title::Container& getTitles() { return titles; }

  CK2Title::Iter startTitle () {return titles.begin();}
  CK2Title::Iter finalTitle () {return titles.end();}
  CK2Title::cIter startTitle () const {return titles.begin();}
  CK2Title::cIter finalTitle () const {return titles.end();}  
  CK2Title::Iter startTitleVassals () {return titlesWithVassals.begin();}
  CK2Title::Iter finalTitleVassals () {return titlesWithVassals.end();}
  
  Iter startEnemy () {return enemies.begin();}
  Iter finalEnemy () {return enemies.end();}
  Iter startVassal () {return vassals.begin();}
  Iter finalVassal () {return vassals.end();}

  static bool humansSovereign;
private:
  EU4Country* eu4Country;
  CK2Title::Container titles;
  CK2Title::Container titlesWithVassals;
  Container enemies; // Opponents in active wars.
  Container tributaries;
  Container vassals;
  CK2Ruler* liege;
  CK2Ruler* suzerain; // Tributary overlord.
  int totalRealmBaronies;
};

string nameAndNumber (CK2Character* ruler);

#endif
