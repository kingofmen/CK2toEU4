#include "CK2Title.hh"

#include "CK2Ruler.hh"
#include "EU4Country.hh"
#include "Logger.hh"

TitleLevel const* const TitleLevel::Barony  = new TitleLevel("barony", false);
TitleLevel const* const TitleLevel::County  = new TitleLevel("county", false);
TitleLevel const* const TitleLevel::Duchy   = new TitleLevel("duchy", false);
TitleLevel const* const TitleLevel::Kingdom = new TitleLevel("kingdom", false);
TitleLevel const* const TitleLevel::Empire  = new TitleLevel("empire", true);

CK2Title::Container CK2Title::empires;
CK2Title::Container CK2Title::kingdoms;
CK2Title::Container CK2Title::duchies;
CK2Title::Container CK2Title::counties;
CK2Title::Container CK2Title::baronies;

CK2Title::CK2Title (Object* o)
  : Enumerable<CK2Title>(this, o->getKey(), false)
  , ObjectWrapper(o)
  , eu4country(0)
  , ruler(0)
  , deJureLiege(0)
  , liegeTitle(0)
  , titleLevel(0)
{
  if (TitleLevel::Empire  == getLevel()) empires.push_back(this);
  if (TitleLevel::Kingdom == getLevel()) kingdoms.push_back(this);
  if (TitleLevel::Duchy   == getLevel()) duchies.push_back(this);
  if (TitleLevel::County  == getLevel()) counties.push_back(this);
  baronies.push_back(this);
}

TitleLevel const* const CK2Title::getLevel () {
  if (titleLevel) return titleLevel;
  switch (getName()[0]) {
  case 'b': titleLevel = TitleLevel::Barony; break;
  case 'c': titleLevel = TitleLevel::County; break;
  case 'd': titleLevel = TitleLevel::Duchy; break;
  case 'k': titleLevel = TitleLevel::Kingdom; break;
  case 'e': titleLevel = TitleLevel::Empire; break;
  default:
    // This should never happen...
    Logger::logStream(LogStream::Error) << "Cannot determine level of title with key \"" << getName() << "\". Error? Returning barony.\n";
    titleLevel = TitleLevel::Barony;
  }
  return titleLevel;
}

CK2Title* CK2Title::getLiege () {
  if (liegeTitle) return liegeTitle;
  Object* liegeObject = object->getNeededObject("liege");
  if (!liegeObject) return 0;
  string liegeTitleKey = remQuotes(liegeObject->safeGetString("title", "nonesuch"));
  liegeTitle = findByName(liegeTitleKey);
  return liegeTitle;
}

CK2Ruler* CK2Title::getSovereign () {
  CK2Title* currTitle = getSovereignTitle();
  if (!currTitle) return 0;
  return currTitle->getRuler();
}

CK2Title* CK2Title::getSovereignTitle () {
  CK2Title* currTitle = this;
  // Straightforward liege chain.
  while (currTitle) {
    if (currTitle->getEU4Country()) return currTitle;
    currTitle = currTitle->getLiege();
  }

  // De jure chain.
  currTitle = this;
  while (currTitle) {
    if (currTitle->getEU4Country()) return currTitle;
    currTitle = currTitle->getDeJureLiege();
  }

  // Liege chain again - characters may have nations where their titles don't.
  currTitle = this;
  while (currTitle) {
    if ((currTitle->getRuler()) && (currTitle->getRuler()->getEU4Country())) return currTitle;
    currTitle = currTitle->getLiege();
  }

  // And de-jure again.
  currTitle = this;
  while (currTitle) {
    if ((currTitle->getRuler()) && (currTitle->getRuler()->getEU4Country())) return currTitle;
    currTitle = currTitle->getDeJureLiege();
  }
  
  return 0;
}

bool CK2Title::isDeJureOverlordOf (CK2Title* dat) const {
  while (dat) {
    if (this == dat) return true;
    dat = dat->getDeJureLiege();
  }
  return false;
}

void CK2Title::setDeJureLiege (CK2Title* djl) {
  if (!djl) return;
  Logger::logStream("titles") << getName() << " has de jure liege " << djl->getName() << "\n";
  deJureLiege = djl;
}

CK2Title::Iter CK2Title::startLevel (TitleLevel const* const level) {
  if (TitleLevel::Empire  == level) return empires.begin();
  if (TitleLevel::Kingdom == level) return kingdoms.begin();
  if (TitleLevel::Duchy   == level) return duchies.begin();
  if (TitleLevel::County  == level) return counties.begin();
  return baronies.begin();
}


CK2Title::Iter CK2Title::finalLevel (TitleLevel const* const level) {
  if (TitleLevel::Empire  == level) return empires.end();
  if (TitleLevel::Kingdom == level) return kingdoms.end();
  if (TitleLevel::Duchy   == level) return duchies.end();
  if (TitleLevel::County  == level) return counties.end();
  return baronies.end();
}
