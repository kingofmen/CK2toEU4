#include "EU4Province.hh"
#include "EU4Country.hh"
#include "Logger.hh"
EU4Province::EU4Province (Object* o)
  : Enumerable<EU4Province>(this, o->getKey(), false)
  , ObjectWrapper(o)
  , eu4Country(0)
{}

void EU4Province::addCore (string countryTag) {
  if (!hasCore(countryTag)) {
    string quotedTag = addQuotes(countryTag);
    getNeededObject("cores")->addToList(quotedTag);
    getNeededObject("history")->setLeaf("add_core", quotedTag);
  }
}

void EU4Province::assignCountry(EU4Country* eu4) {
  if (eu4Country) {
    eu4Country->remProvince(this);
  }
  eu4Country = eu4;
  resetLeaf("owner", addQuotes(eu4->getName()));
  resetLeaf("controller", addQuotes(eu4->getName()));
  resetHistory("owner", addQuotes(eu4->getName()));
  getNeededObject("history")
      ->getNeededObject("controller")
      ->resetLeaf("tag", addQuotes(eu4->getName()));
  eu4Country->addProvince(this);
}

void EU4Province::assignProvince (CK2Province* ck) {
  ckProvinces.push_back(ck);
}

bool EU4Province::converts () const {
  if (0 == numCKProvinces()) return false;
  return true;
}

bool EU4Province::hasCore (string countryTag) {
  auto* cores = getNeededObject("cores");
  string quotedTag = addQuotes(countryTag);
  for (int i = 0; i < cores->numTokens(); ++i) {
    if (cores->getToken(i) == quotedTag) {
      return true;
    }
  }
  return false;
}

void EU4Province::remCore (string countryTag) {
  string quotedTag = addQuotes(countryTag);
  getNeededObject("cores")->remToken(quotedTag);
  getNeededObject("history")->unsetKeyValue("add_core", quotedTag);
}
