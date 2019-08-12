#include "CK2Ruler.hh"

#include <algorithm>

#include "constants.hh"
#include "CK2Province.hh"
#include "CK2Title.hh"
#include "Logger.hh"
#include "UtilityFunctions.hh"

objvec CK2Character::ckTraits;
objvec CK2Character::euRulerTraits;

CKAttribute const* const CKAttribute::Diplomacy    = new CKAttribute("diplomacy",   false);
CKAttribute const* const CKAttribute::Martial      = new CKAttribute("martial",     false);
CKAttribute const* const CKAttribute::Stewardship  = new CKAttribute("stewardship", false);  
CKAttribute const* const CKAttribute::Intrigue     = new CKAttribute("intrigue",    false);
CKAttribute const* const CKAttribute::Learning     = new CKAttribute("learning",    true);

CouncilTitle const* const CouncilTitle::Chancellor = new CouncilTitle("job_chancellor", false);
CouncilTitle const* const CouncilTitle::Marshal    = new CouncilTitle("job_marshal",    false);
CouncilTitle const* const CouncilTitle::Steward    = new CouncilTitle("job_treasurer",  false);
CouncilTitle const* const CouncilTitle::Spymaster  = new CouncilTitle("job_spymaster",  false);
CouncilTitle const* const CouncilTitle::Chaplain   = new CouncilTitle("job_spiritual",  true);

bool CK2Ruler::humansSovereign = false;

CK2Character::CK2Character (Object* obj, Object* dynasties)
  : ObjectWrapper(obj)
  , admiral(0)
  , attributes(CKAttribute::totalAmount(), 0)
  , council(CouncilTitle::totalAmount())
  , dynasty(0)
  , heir(0)
{
  string dynastyNum = safeGetString(dynastyString, PlainNone);
  if (dynastyNum != PlainNone) {
    dynasty = dynasties->safeGetObject(dynastyNum);
  }
  
  Object* attribs = safeGetObject(attributesString);
  if (attribs) {
    for (int i = 0; i < (int) attributes.size(); ++i) {
      if (i >= attribs->numTokens()) break;
      attributes[i] = attribs->tokenAsInt(i);
    }
  }
  bool debug = false;
  if (safeGetString("player", "no") == "yes") {
    Logger::logStream("characters")
        << "Starting traits for " << nameAndNumber(this) << ", played by "
        << safeGetString("player_name") << "\n"
        << LogOption::Indent;
    debug = true;
  }
  static set<int> unknownTraits;
  Object* traitList = safeGetObject(traitString);
  if (traitList) {
    for (int i = 0; i < traitList->numTokens(); ++i) {
      int index = traitList->tokenAsInt(i) - 1;
      if (index >= (int) ckTraits.size()) {
        if (!unknownTraits.count(index)) {
          Logger::logStream(LogStream::Warn)
              << "Trait " << index << " is unknown and will be ignored.\n";
          unknownTraits.insert(index);
        }
	continue;
      }
      Object* traitObject = ckTraits[index];
      if (debug) Logger::logStream("characters") << traitObject->getKey() << " ";
      traits.insert(traitObject->getKey());
      for (CKAttribute::Iter att = CKAttribute::start(); att != CKAttribute::final(); ++att) {
	attributes[**att] += traitObject->safeGetInt((*att)->getName());
      }
      if (traitObject->safeGetString("leader", "no") == "yes") {
        attributes[*CKAttribute::Martial]++;
      }
    }
  } else if (debug) {
    Logger::logStream("characters")
        << "Could not find traits for " << nameAndNumber(this) << "\n";
  }
  if (debug) {
    Logger::logStream("characters") << "\n";
    for (CKAttribute::Iter att = CKAttribute::start();
         att != CKAttribute::final(); ++att) {
      Logger::logStream("characters")
          << (*att)->getName() << ": " << attributes[**att] << " ";
    }
    Logger::logStream("characters") << "\n" << LogOption::Undent;
  }
}

CK2Character* CK2Character::getAdvisor (const string& title) {
  if (advisors.find(title) == advisors.end()) {
    return nullptr;
  }
  return advisors[title][0];
}

double CK2Character::getAge (string date) const {
  int gameYear, gameMonth, gameDay;
  if (!yearMonthDay(date, gameYear, gameMonth, gameDay)) {
    Logger::logStream(LogStream::Warn) << "Could not get year, month, day from '" << date << "'\n";
    return 16;
  }
  int charYear, charMonth, charDay;
  string birthday = remQuotes(safeGetString(birthDateString, QuotedNone));
  if (!yearMonthDay(birthday, charYear, charMonth, charDay)) {
    Logger::logStream(LogStream::Warn) << "Could not get year, month, day from ("
				       << getKey() << " " << safeGetString(birthNameString) << ") '"
				       << birthday << "'\n";
    return 16;
  }
  double years = gameYear;
  years -= charYear;
  years += (charMonth - gameMonth) / 12.0;
  years += (charDay - gameDay) / 365.0;
  return years;
}

CK2Character* CK2Character::getBestSpouse() const {
  if (spouses.empty()) {
    return nullptr;
  }
  CK2Character* best = nullptr;
  int score = 0;
  for (auto* spouse : spouses) {
    int curr_score = 0;
    for (auto att : attributes) {
      curr_score += att;
    }
    curr_score -= (int)floor(spouse->getAge("1444.11.11") + 0.5);
    if (curr_score > score || !best) {
      best = spouse;
      score = curr_score;
    }
  }
  return best;
}

bool CK2Character::hasModifier (const string& mod) {
  if (modifiers.count(mod)) return modifiers[mod];
  modifiers[mod] = false;
  objvec mods = getValue("modifier");
  for (objiter m = mods.begin(); m != mods.end(); ++m) {
    string modname = remQuotes((*m)->safeGetString("modifier", QuotedNone));
    modifiers[modname] = true;
  }
  return modifiers[mod];
}

void CK2Ruler::personOfInterest (CK2Character* person) {
  string myId = getKey();
  if ((person->safeGetString(fatherString) == myId) ||
      (person->safeGetString(motherString) == myId)) {
    children.push_back(person);
    if ((!heir) ||
        (heir->getAge(
             remQuotes(person->safeGetString(birthDateString))) < 0)) {
      heir = person;
    }
  }

  CK2Ruler* other = findByName(person->getKey());
  bool vassal = ((other) && (other->getLiege() == this));
  if ((person->safeGetString("host") == myId) || (vassal)) {
    objvec titles = person->getValue("title");
    for (objiter title = titles.begin(); title != titles.end(); ++title) {
      if (!(*title)->isLeaf()) continue;
      string job = remQuotes((*title)->getLeaf());
      if (job == "title_commander") commanders.push_back(person);
      else if (job == "title_high_admiral") admiral = person;
      else advisors[job].push_back(person);
    }
    string job = remQuotes(person->safeGetString(jobTitleString, QuotedNone));
    for (CouncilTitle::Iter con = CouncilTitle::start(); con != CouncilTitle::final(); ++con) {
      if (job != (*con)->getName()) continue;
      council[**con] = person;
      if ((*con) == CouncilTitle::Marshal) commanders.push_back(person);
      break;
    }
  }
}

CK2Ruler::CK2Ruler (Object* obj, Object* dynasties)
  : Enumerable<CK2Ruler>(this, obj->getKey(), false)
  , CK2Character(obj, dynasties)
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

void CK2Character::createClaims () {
  objvec clams = getValue("claim");
  for (objiter clam = clams.begin(); clam != clams.end(); ++clam) {
    Object* obj = (*clam)->safeGetObject("title");
    if (!obj) continue;
    CK2Title* title = CK2Title::findByName(remQuotes(obj->safeGetString("title", QuotedNone)));
    if (!title) continue;
    title->addClaimant(this);
  }
}

void CK2Ruler::createLiege () {
  CK2Ruler* liegeCand = 0;
  CK2Title* liegeTitle = 0;
  CK2Title* vassalTitle = 0;
  for (auto* title : titles) {
    CK2Title* bossTitle = title->getLiege();
    if (!bossTitle) continue;
    string liegeId = bossTitle->safeGetString("holder", PlainNone);
    if (liegeId == PlainNone) continue;
    if (liegeId == getName()) continue; // Eg a count who holds a barony is his own liege.
    CK2Ruler* boss = findByName(liegeId);
    if (!liegeCand) {
      liegeCand = boss;
      liegeTitle = bossTitle;
      vassalTitle = title;
    }
    else if (boss != liegeCand) {
      Logger::logStream(LogStream::Warn) << getName() << " appears to have multiple lieges, "
					 << liegeCand->getName() << " of " << liegeTitle->getName()
					 << " through " << vassalTitle->getName() << " and "
					 << boss->getName() << " of " << bossTitle->getName()
					 << " through " << title->getName()
					 << ".\n";
      continue;
    }
  }
  if (!liegeCand) return;

  // Iterating manually because we are still constructing the liege chain.
  CK2Title* currTitle = liegeTitle;
  while (currTitle) {
    string holderId = currTitle->safeGetString("holder", PlainNone);
    if (holderId == getName()) {
      Logger::logStream("characters")
          << "Not making " << getName() << " vassal of " << liegeCand->getName()
          << " because of circularity with " << currTitle->getName() << "\n";
      return;
    }
    currTitle = currTitle->getLiege();
  }

  Logger::logStream("characters") << getName() << " is vassal of "
				  << liegeCand->getName()
				  << " because " << vassalTitle->getName()
				  << " is vassal of " << liegeTitle->getName()
				  << ".\n";
  liege = liegeCand;
  liege->personOfInterest(this);
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

bool CK2Ruler::hasTitle(CK2Title* title, bool includeVassals) const {
  if (includeVassals)
    return (find(titlesWithVassals.begin(), titlesWithVassals.end(), title) !=
            titlesWithVassals.end());
  return (find(titles.begin(), titles.end(), title) != titles.end());
}

string CK2Character::getBelief (string keyword) const {
  string ret = remQuotes(safeGetString(keyword, QuotedNone));
  if (ret != PlainNone) return ret;
  // Try the new keywords - don't use new/old constant here because it is
  // 'culture' and 'religion' everywhere *except* the character object.
  if (keyword == "culture") {
    ret = remQuotes(safeGetString("cul", QuotedNone));
  } else if (keyword == "religion") {
    ret = remQuotes(safeGetString("rel", QuotedNone));
  }
  if (ret != PlainNone) return ret;
  Object* myDynasty = getDynasty();
  if (myDynasty) {
    ret = remQuotes(myDynasty->safeGetString(keyword, QuotedNone));
    if (ret != PlainNone) return ret;
    Object* coa = myDynasty->safeGetObject("coat_of_arms");
    if (coa) {
      ret = remQuotes(coa->safeGetString(keyword, QuotedNone));
      if (ret != PlainNone) return ret;
    }
  }

  CK2Ruler* ruler = CK2Ruler::getByName(getKey());
  if (ruler != nullptr) {
    for (CK2Title::cIter title = ruler->startTitle();
         title != ruler->finalTitle(); ++title) {
      if (TitleLevel::Barony != (*title)->getLevel()) {
        continue;
      }
      CK2Province* province = CK2Province::getFromBarony((*title)->getKey());
      if (province != nullptr) {
        ret = province->safeGetString(keyword, PlainNone);
        if (PlainNone != ret) return ret;
      }
    }
  }

  return PlainNone;
}

CK2Title* CK2Ruler::getPrimaryTitle () {
  if (0 == titles.size()) return 0;
  Object* demesne = safeGetObject(demesneString);
  if (demesne) {
    Object* primary = demesne->safeGetObject("primary");
    if (primary) {
      string tag;
      if (primary->isLeaf()) {
        tag = demesne->safeGetString("primary");
      } else {
        tag = remQuotes(primary->safeGetString("title"));
      }
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

CK2Ruler* CK2Ruler::getSovereignLiege () {
  CK2Ruler* cand = this;
  while ((cand) && (!cand->isSovereign())) cand = cand->getLiege();
  return cand;
}

bool CK2Ruler::isRebel () {
  const auto* primary = getPrimaryTitle();
  if (!primary) {
    return false;
  }
  return primary->isRebelTitle();
}

string nameAndNumber(CK2Character* ruler) {
  return ruler->safeGetString(birthNameString) + " (" + ruler->getKey() + ")";
}
