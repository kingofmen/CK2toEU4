#ifndef EU4COUNTRY_HH
#define EU4COUNTRY_HH

#include "CK2Title.hh"
#include "UtilityFunctions.hh"

class EU4Country : public Enumerable<EU4Country>, public ObjectWrapper {
public:
  EU4Country (Object* o);

  void assignCountry (CK2Title* ck2);
  CK2Ruler* getRuler () const {return ckSovereign;}
  void setRuler (CK2Ruler* ruler);
private:
  CK2Ruler* ckSovereign;
  CK2Title::Container ck2Titles;
};

#endif
