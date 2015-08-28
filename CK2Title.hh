#ifndef CK2_TITLE_HH
#define CK2_TITLE_HH

#include "UtilityFunctions.hh"

class CK2Ruler;
class EU4Country;

class TitleLevel : public Enumerable<TitleLevel> {
public:
  TitleLevel (string n, bool f) : Enumerable<TitleLevel>(this, n, f) {};

  static TitleLevel const* const Barony;
  static TitleLevel const* const County;
  static TitleLevel const* const Duchy;
  static TitleLevel const* const Kingdom;
  static TitleLevel const* const Empire;
};

class CK2Title : public Enumerable<CK2Title>, public ObjectWrapper {
public:
  CK2Title (Object* o);

  void assignCountry (EU4Country* eu4);
  TitleLevel const* const getLevel ();
  EU4Country* getEUcountry () const {return eu4Country;}
  CK2Title* getLiege ();
  CK2Ruler* getRuler () {return ruler;}
  void setRuler (CK2Ruler* r) {ruler = r;}

  static Iter startEmpire () {return empires.begin();}
  static Iter finalEmpire () {return empires.end();}
private:
  CK2Ruler* ruler;
  EU4Country* eu4Country;
  CK2Title* liegeTitle;
  TitleLevel const* titleLevel;

  static Container empires;
};

#endif
