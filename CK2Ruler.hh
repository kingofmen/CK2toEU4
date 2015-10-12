#ifndef CK2RULER_HH
#define CK2RULER_HH

#include <set>
#include <string>

#include "CK2Title.hh"
#include "UtilityFunctions.hh"

class EU4Country;

class CKAttribute : public Enumerable<CKAttribute> {
public:
  CKAttribute (string n, bool f) : Enumerable<CKAttribute>(this, n, f) {};

  static CKAttribute const* const Diplomacy;
  static CKAttribute const* const Martial;
  static CKAttribute const* const Stewardship;  
  static CKAttribute const* const Intrigue;
  static CKAttribute const* const Learning;
};

class CK2Character : public ObjectWrapper {
public:
  CK2Character (Object* obj);

  int getAttribute (CKAttribute const* const att) const {return attributes[*att];}
  bool hasTrait (const string& t) const {return 0 != traits.count(t);}
  
  static objvec ckTraits;
private:
  vector<int> attributes;
  set<string> traits;
};

class CK2Ruler : public Enumerable<CK2Ruler>, public CK2Character {
public:
  CK2Ruler (Object* obj, Object* dynasties);

  void addEnemy (CK2Ruler* enemy) {enemies.push_back(enemy);}
  void addTitle (CK2Title* title);
  void addTributary (CK2Ruler* trib);
  int  countBaronies ();
  void createClaims ();
  void createLiege ();
  string getBelief (string keyword) const;
  Object* getDynasty () const {return dynasty;}
  EU4Country* getEU4Country () const {return eu4Country;}
  CK2Ruler* getLiege () {return liege;}
  CK2Title* getPrimaryTitle ();
  CK2Ruler* getSuzerain () {return suzerain;}
  bool hasTitle (CK2Title* title, bool includeVassals = false) const;
  bool isSovereign () const {return (0 == liege);}
  void setEU4Country (EU4Country* eu4) {eu4Country = eu4;}

  int getEnemies () const {return enemies.size();}

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
private:
  Object* dynasty;
  EU4Country* eu4Country;
  CK2Title::Container claims;
  CK2Title::Container titles;
  CK2Title::Container titlesWithVassals;
  Container enemies; // Opponents in active wars.
  Container tributaries;
  Container vassals;
  CK2Ruler* liege;
  CK2Ruler* suzerain; // Tributary overlord.
  int totalRealmBaronies;
};

#endif
