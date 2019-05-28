#ifndef CK2_WAR_HH
#define CK2_WAR_HH

#include <functional>
#include <vector>

#include "CK2Ruler.hh"
#include "UtilityFunctions.hh"

class CK2War : public Enumerable<CK2War>, public ObjectWrapper {
public:
  CK2War (Object* obj);

  enum WarMask {
    Attackers = 1,
    Defenders = 2,
  };

  std::vector<const CK2Ruler*>
  getParticipants(int mask, std::function<bool(const CK2Ruler*)> filter) const;

private:
  CK2Ruler::Container attackers;
  CK2Ruler::Container defenders;
};

#endif
