#include "EU4Province.hh"
#include "EU4Country.hh"

EU4Province::EU4Province (Object* o)
  : Enumerable<EU4Province>(this, o->getKey(), false)
  , ObjectWrapper(o)
  , eu4Country(0)
{}

void EU4Province::addCore (string countryTag) {
  string quotedTag = addQuotes(countryTag);
  setLeaf("core", quotedTag);
  getNeededObject("history")->setLeaf("add_core", quotedTag);
}

void EU4Province::assignCountry (EU4Country* eu4) {
  eu4Country = eu4;
  resetLeaf("owner", addQuotes(eu4->getName()));
  resetLeaf("controller", addQuotes(eu4->getName()));
  resetHistory("owner", addQuotes(eu4->getName()));
  getNeededObject("history")->getNeededObject("controller")->resetLeaf("tag", addQuotes(eu4->getName()));
}

void EU4Province::assignProvince (CK2Province* ck) {
  ckProvinces.push_back(ck);
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
