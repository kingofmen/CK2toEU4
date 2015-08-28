#include "CK2Title.hh"

#include "EU4Country.hh"
#include "Logger.hh"

TitleLevel const* const TitleLevel::Barony  = new TitleLevel("barony", false);
TitleLevel const* const TitleLevel::County  = new TitleLevel("county", false);
TitleLevel const* const TitleLevel::Duchy   = new TitleLevel("duchy", false);
TitleLevel const* const TitleLevel::Kingdom = new TitleLevel("kingdom", false);
TitleLevel const* const TitleLevel::Empire  = new TitleLevel("empire", true);

CK2Title::Container CK2Title::empires;

CK2Title::CK2Title (Object* o)
  : Enumerable<CK2Title>(this, o->getKey(), false)
  , ObjectWrapper(o)
  , ruler(0)
  , eu4Country(0)
  , liegeTitle(0)
  , titleLevel(0)
{
  if (TitleLevel::Empire == getLevel()) empires.push_back(this);
}

void CK2Title::assignCountry (EU4Country* eu4) {
  if (eu4Country) {
    Logger::logStream(LogStream::Warn) << "Attempted to assign EU4 "
				       << eu4->getName()
				       << " to CK2 "
				       << getName()
				       << ", but already assigned "
				       << eu4Country->getName()
				       << ". Ignoring.\n";
    return;
  }
  eu4Country = eu4;
  eu4Country->assignCountry(this);
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
