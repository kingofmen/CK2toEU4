#ifndef CK2_TITLE_HH
#define CK2_TITLE_HH

#include "UtilityFunctions.hh"

class CK2Character;
class CK2Ruler;
class EU4Country;

class TitleLevel : public Enumerable<TitleLevel> {
public:
  TitleLevel (string n, bool f) : Enumerable<TitleLevel>(this, n, f) {};
  static TitleLevel const* const getLevel (const string& key);
  
  static TitleLevel const* const Barony;
  static TitleLevel const* const County;
  static TitleLevel const* const Duchy;
  static TitleLevel const* const Kingdom;
  static TitleLevel const* const Empire;
};

class CK2Title : public Enumerable<CK2Title>, public ObjectWrapper {
public:
  CK2Title (Object* o);

  void addClaimant (CK2Character* claimant);
  int distanceToSovereign ();
  TitleLevel const* const getLevel () const;
  EU4Country* getEU4Country () const {return eu4country;}
  CK2Title* getDeJureLiege () const {return deJureLiege;}
  CK2Title* getDeJureLevel (TitleLevel const* const level);
  CK2Title* getLiege ();
  CK2Ruler* getRuler () {return ruler;}
  CK2Ruler* getSovereign (); // Returns the first liege that converts to an EU4 nation.
  CK2Title* getSovereignTitle (); // The title that converts.
  string getTag() { return getKey(); }
  bool isDeJureOverlordOf (CK2Title* dat) const;
  bool isRebelTitle () const {return isRebel;}
  void setRuler (CK2Ruler* r) {ruler = r;}
  void setDeJureLiege (CK2Title* djl);
  void setEU4Country (EU4Country* eu4) {eu4country = eu4;}
  vector<CK2Character*>::iterator startClaimant () {return claimants.begin();}
  vector<CK2Character*>::iterator finalClaimant () {return claimants.end();}

  static Iter startEmpire () {return empires.begin();}
  static Iter finalEmpire () {return empires.end();}
  static Iter startLevel (TitleLevel const* const level);
  static Iter finalLevel (TitleLevel const* const level);
private:
  vector<CK2Character*> claimants;
  EU4Country* eu4country;
  CK2Ruler* ruler;
  CK2Title* deJureLiege;
  CK2Title* liegeTitle;
  TitleLevel const* const titleLevel;
  bool isRebel;

  static Container empires;
  static Container kingdoms;
  static Container duchies;
  static Container counties;
  static Container baronies;
};

#endif
