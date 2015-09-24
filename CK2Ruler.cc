#include "CK2Ruler.hh"

#include <algorithm>
#include "CK2Title.hh"
#include "Logger.hh"

CK2Ruler::CK2Ruler (Object* obj)
  : Enumerable<CK2Ruler>(this, obj->getKey(), false)
  , ObjectWrapper(obj)
  , eu4Country(0)
  , liege(0)
  , suzerain(0)
  , totalRealmBaronies(-1)
{}

void CK2Ruler::addTitle (CK2Title* title) {
  titles.push_back(title);
  titlesWithVassals.push_back(title);
  title->setRuler(this);
}

void CK2Ruler::addTributary (CK2Ruler* trib) {
  trib->suzerain = this;
  tributaries.push_back(trib);
}

void CK2Ruler::createLiege () {
  CK2Ruler* liegeCand = 0;
  CK2Title* liegeTitle = 0;
  CK2Title* vassalTitle = 0;
  for (CK2Title::Iter title = titles.begin(); title != titles.end(); ++title) {
    CK2Title* bossTitle = (*title)->getLiege();
    if (!bossTitle) continue;
    string liegeId = bossTitle->safeGetString("holder", "none");
    if (liegeId == "none") continue;
    if (liegeId == getName()) continue; // Eg a count who holds a barony is his own liege.
    CK2Ruler* boss = findByName(liegeId);
    if (!liegeCand) {
      liegeCand = boss;
      liegeTitle = bossTitle;
      vassalTitle = (*title);
    }
    else if (boss != liegeCand) {
      Logger::logStream(LogStream::Warn) << getName() << " appears to have multiple lieges, "
					 << liegeCand->getName() << " of " << liegeTitle->getName()
					 << " through " << vassalTitle->getName() << " and "
					 << boss->getName() << " of " << bossTitle->getName()
					 << " through " << (*title)->getName()
					 << ".\n";
      continue;
    }
  }
  if (!liegeCand) return;
  Logger::logStream("characters") << getName() << " is vassal of "
				  << liegeCand->getName()
				  << " because " << vassalTitle->getName()
				  << " is vassal of " << liegeTitle->getName()
				  << ".\n";
  liege = liegeCand;
  liege->vassals.push_back(this);
  for (CK2Title::Iter title = startTitle(); title != finalTitle(); ++title) {
    liege->titlesWithVassals.push_back(*title);
  }
}

// Ha ha, pun!
int CK2Ruler::countBaronies () {
  if (-1 != totalRealmBaronies) return totalRealmBaronies;
  totalRealmBaronies = 0;
  for (CK2Title::Iter title = titles.begin(); title != titles.end(); ++title) {
    if (TitleLevel::Barony != (*title)->getLevel()) continue;
    ++totalRealmBaronies;
  }
  for (Iter vassal = vassals.begin(); vassal != vassals.end(); ++vassal) {
    totalRealmBaronies += (*vassal)->countBaronies();
  }
  object->setLeaf("totalRealmBaronies", totalRealmBaronies);
  return totalRealmBaronies;
}

bool CK2Ruler::hasTitle (CK2Title* title, bool includeVassals) const {
  if (includeVassals) return (find(titlesWithVassals.begin(), titlesWithVassals.end(), title) != titlesWithVassals.end());
  return (find(titles.begin(), titles.end(), title) != titles.end());
}

CK2Title* CK2Ruler::getPrimaryTitle () {
  if (0 == titles.size()) return 0;
  Object* demesne = safeGetObject("demesne");
  if (demesne) {
    Object* primary = demesne->safeGetObject("primary");
    if (primary) {
      string tag = remQuotes(primary->safeGetString("title"));
      return CK2Title::findByName(tag);
    }
  }

  // No primary title in save; return the first highest-level title.
  CK2Title* best = *startTitle();
  for (CK2Title::Iter title = startTitle(); title != finalTitle(); ++title) {
    if (*(*title)->getLevel() <= *(best->getLevel())) continue;
    best = (*title);
  }
  return best;
}
