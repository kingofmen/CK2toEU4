#include "EU4Province.hh"

EU4Province::EU4Province (Object* o)
  : Enumerable<EU4Province>(this, o->getKey(), false)
  , ObjectWrapper(o)
  , eu4Country(0)
{}

void EU4Province::assignProvince (CK2Province* ck) {
  ckProvinces.push_back(ck);
}
