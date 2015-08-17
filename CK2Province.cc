#include "CK2Province.hh"

CK2Province::CK2Province (Object* o)
  : Iterable<CK2Province>(this)
  , ObjectWrapper(o)
{}
