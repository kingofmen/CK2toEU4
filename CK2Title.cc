#include "CK2Title.hh"

#include "Logger.hh"

TitleLevel const* const TitleLevel::Barony  = new TitleLevel("barony", false);
TitleLevel const* const TitleLevel::County  = new TitleLevel("county", false);
TitleLevel const* const TitleLevel::Duchy   = new TitleLevel("duchy", false);
TitleLevel const* const TitleLevel::Kingdom = new TitleLevel("kingdom", false);
TitleLevel const* const TitleLevel::Empire  = new TitleLevel("empire", true);

CK2Title::CK2Title (Object* o)
  : Enumerable<CK2Title>(this, o->getKey(), false)
  , ObjectWrapper(o)
  , liegeTitle(0)
  , titleLevel(0)
{}

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
  string liegeTitleKey = remQuotes(liegeObject->safeGetString("title", "nonesuch"));
  liegeTitle = findByName(liegeTitleKey);
  return liegeTitle;
}
