#ifndef CK2_TITLE_HH
#define CK2_TITLE_HH

#include "UtilityFunctions.hh"

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

  TitleLevel const* const getLevel ();
  CK2Title* getLiege ();
private:
  CK2Title* liegeTitle;
  TitleLevel const* titleLevel;
};

#endif
