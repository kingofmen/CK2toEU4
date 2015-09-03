#include "EU4Country.hh"

#include "CK2Ruler.hh"
#include "CK2Title.hh"

EU4Country::EU4Country (Object* o)
  : Enumerable<EU4Country>(this, o->getKey(), false)
  , ObjectWrapper(o)
  , ckSovereign(0)
{}

void EU4Country::assignCountry (CK2Title* ck2) {
  if (!ckSovereign) ckSovereign = ck2->getRuler();
  ck2Titles.push_back(ck2);
}

void EU4Country::setRuler (CK2Ruler* ruler) {
  ckSovereign = ruler;
  ckSovereign->setEU4Country(this);
}
