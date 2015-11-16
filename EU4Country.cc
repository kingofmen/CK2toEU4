#include "EU4Country.hh"

#include "CK2Ruler.hh"
#include "CK2Title.hh"

const string EU4Country::kNoProvinceMarker("has_zero_provs");

EU4Country::EU4Country (Object* o)
  : Enumerable<EU4Country>(this, o->getKey(), false)
  , ObjectWrapper(o)
  , ckSovereign(0)
  , ckTitle(0)
{}

bool EU4Country::converts () {
  if (!getRuler()) return false;
  if (safeGetString(kNoProvinceMarker, PlainNone) == "yes") return false;
  return true;
}

void EU4Country::setRuler (CK2Ruler* ruler, CK2Title* title) {
  ckSovereign = ruler;
  if (title == ruler->getPrimaryTitle()) ruler->setEU4Country(this);
  ckTitle = title;
  title->setEU4Country(this);
}
