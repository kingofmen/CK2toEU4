#include "CK2Province.hh"

CK2Province::CK2Province (Object* o)
  : Enumerable<CK2Province>(this, o->getKey(), false)
  , ObjectWrapper(o)
{}
