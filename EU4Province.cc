#include "EU4Province.hh"

EU4Province::EU4Province (Object* o)
  : Enumerable<EU4Province>(this, o->getKey(), false)
  , ObjectWrapper(o)
  , eu4Country(0)
{}

void EU4Province::assignProvince (CK2Province* ck) {
  ckProvinces.push_back(ck);
}

void EU4Province::addCore (string countryTag) {
  string quotedTag = addQuotes(countryTag);
  setLeaf("core", quotedTag);
  resetHistory("add_core", quotedTag);
}

bool EU4Province::hasCore (string countryTag) const {
  objvec cores = getValue("core");
  string quotedTag = addQuotes(countryTag);
  for (objiter core = cores.begin(); core != cores.end(); ++core) {
    if ((*core)->getLeaf() != quotedTag) continue;
    return true;
  }
  return false;
}
