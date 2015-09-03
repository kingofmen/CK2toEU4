#ifndef CK2_PROVINCE_HH
#define CK2_PROVINCE_HH

#include "UtilityFunctions.hh"

class CK2Title;
class EU4Province;

class CK2Province : public Enumerable<CK2Province>, public ObjectWrapper {
public:
  CK2Province (Object* o);

  void assignProvince (EU4Province* t);
  CK2Title* getCountyTitle () const {return countyTitle;}
  double getWeight () {return 1;}
  void setCountyTitle (CK2Title* t) {countyTitle = t;}
private:
  CK2Title* countyTitle;
  vector<EU4Province*> targets;
};

#endif
