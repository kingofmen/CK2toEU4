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

TitleLevel const* const TitleLevel::getLevel (const string& key) {
  switch (key[0]) {
  case 'b': return TitleLevel::Barony;
  case 'c': return TitleLevel::County;
  case 'd': return TitleLevel::Duchy;
  case 'k': return TitleLevel::Kingdom;
  case 'e': return TitleLevel::Empire;
  default:
    // This should never happen...
    Logger::logStream(LogStream::Error)
        << "Cannot determine level of title with key \"" << key
        << "\". Error? Returning barony.\n";
    return TitleLevel::Barony;
  }
  return TitleLevel::Barony;
}

CK2Title::CK2Title (Object* o)
  : Enumerable<CK2Title>(this, o->getKey(), false)
  , ObjectWrapper(o)
  , eu4country(0)
  , ruler(0)
  , deJureLiege(0)
  , liegeTitle(0)
  , titleLevel(TitleLevel::getLevel(o->getKey()))
  , isRebel(false)
{
  if (TitleLevel::Empire  == getLevel()) empires.push_back(this);
  else if (TitleLevel::Kingdom == getLevel()) kingdoms.push_back(this);
  else if (TitleLevel::Duchy   == getLevel()) duchies.push_back(this);
  else if (TitleLevel::County  == getLevel()) counties.push_back(this);
  else baronies.push_back(this);
}

int CK2Title::distanceToSovereign () {
  CK2Ruler* sovereign = getSovereign();
  if (!sovereign) return 0;
  CK2Ruler* ruler = getRuler();
  if (!ruler) return 0;
  if (ruler == sovereign) return 0;
  int ret = 0;
  CK2Title* current = this;
  while (current) {
    ++ret;
    current = current->getLiege();
    if (!current) break;
    ruler = current->getRuler();
    if (ruler == sovereign) return ret;
  }
  return ret;
}

TitleLevel const* const CK2Title::getLevel () const {
  return titleLevel;
}

void CK2Title::addClaimant (CK2Character* claimant) {
  if (find(claimants.begin(), claimants.end(), claimant) != claimants.end()) return;
  claimants.push_back(claimant);
}

CK2Title* CK2Title::getDeJureLevel (TitleLevel const* const level) {
  CK2Title* curr = this;
  while (curr) {
    if (curr->getLevel() == level) return curr;
    curr = curr->getDeJureLiege();
  }
  return 0;
}

// Must handle both old format, liege = { title = c_whatever }, and new format,
// liege = c_whatever.
CK2Title* CK2Title::getLiege () {
  if (liegeTitle) return liegeTitle;
  string liegeTitleKey = object->safeGetString("liege", "nonesuch");
  if (liegeTitleKey != "nonesuch") {
    liegeTitle = findByName(liegeTitleKey);
    if (liegeTitle) {
      Logger::logStream("titles")
          << getKey() << " has liege " << liegeTitleKey << ".\n";
      return liegeTitle;
    }
  }

  Object* liegeObject = object->getNeededObject("liege");
  if (liegeObject) {
    liegeTitleKey = remQuotes(liegeObject->safeGetString("title", "nonesuch"));
    liegeTitle = findByName(liegeTitleKey);
    if (liegeTitle) {
      Logger::logStream("titles")
          << getKey() << " has liege " << liegeTitleKey << ".\n";
      return liegeTitle;
    }
  }

  // Try for base title.
  string baseTag = object->safeGetString("base_title", "nonesuch");
  liegeTitle = findByName(baseTag);
  if (liegeTitle) {
    Logger::logStream("titles")
        << getKey() << " has base title " << baseTag << ".\n";
    isRebel = true;
  }
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
