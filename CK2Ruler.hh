#ifndef CK2RULER_HH
#define CK2RULER_HH

#include "CK2Title.hh"
#include "UtilityFunctions.hh"

class EU4Country;

class CK2Ruler : public Enumerable<CK2Ruler>, public ObjectWrapper {
public:
  CK2Ruler (Object* o);

  void addEnemy (CK2Ruler* enemy) {enemies.push_back(enemy);}
  void addTitle (CK2Title* title);
  void addTributary (CK2Ruler* trib);
  int  countBaronies ();
  void createClaims ();
  void createLiege ();
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
  CK2Title::Iter startTitleVassals () {return titlesWithVassals.begin();}
  CK2Title::Iter finalTitleVassals () {return titlesWithVassals.end();}
  
  Iter startEnemy () {return enemies.begin();}
  Iter finalEnemy () {return enemies.end();}
private:
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
