#ifndef UTILITIES_HH
#define UTILITIES_HH

#include <climits>
#include <cstdlib>
#include <map>
#include <cassert>
#include <cmath> 
#include <string>
#include <unordered_map>
#include <vector>
#include "boost/foreach.hpp"
#include "boost/tuple/tuple.hpp"

#include "Parser.hh"

using namespace std;
enum Outcome {Disaster = 0, Bad, Neutral, Good, VictoGlory, NumOutcomes}; 
extern char strbuffer[10000];

extern const string QuotedNone;
extern const string PlainNone;

double degToRad (double degrees);
double radToDeg (double radians); 

bool yearMonthDay (const string& date, int& year, int& month, int& day);

enum RollType {Equal = 0, GtEqual, LtEqual, Greater, Less};

struct DieRoll {
  DieRoll (int d, int f);   
  double probability (int target, int mods, RollType t) const; 
  int roll () const;
  
private:
  double baseProb (int target, int mods, RollType t) const; 

  int dice;
  int faces;
  DieRoll* next; 
};

string outcomeToString (Outcome out);

struct doublet : public boost::tuple<double, double> {
  // Adding a touch of syntactic sugar to the tuple. 
  
  doublet (double x, double y) : boost::tuple<double, double>(x, y) {}
  doublet () : boost::tuple<double, double>(0, 0) {}

  double& x () {return boost::get<0>(*this);}
  double& y () {return boost::get<1>(*this);}

  double x () const {return boost::get<0>(*this);}
  double y () const {return boost::get<1>(*this);}

  double  angle (const doublet& other) const;  
  double  dot   (const doublet& other) const;
  double  norm  () const {return sqrt(pow(this->x(), 2) + pow(this->y(), 2));}
  void    normalise ();
  void    rotate (double degrees, const doublet& around = zero);
  double  distance (const doublet& other) const {return sqrt(pow(x() - other.x(), 2) + pow(y() - other.y(), 2));}

  doublet& operator= (const boost::tuple<double, double, double>& rhs);
  void operator/= (double scale);
  void operator*= (double scale);
  void operator-= (const doublet& other);
  void operator+= (const doublet& other);
  //void operator+= (const triplet& other);

private:
  static const doublet zero; 
}; 

struct triplet : public boost::tuple<double, double, double> {
  // Adding a touch of syntactic sugar to the tuple. 
  
  triplet (double x, double y, double z) : boost::tuple<double, double, double>(x, y, z) {}
  triplet (double x, double y)           : boost::tuple<double, double, double>(x, y, 0) {}
  triplet (double x)                     : boost::tuple<double, double, double>(x, 0, 0) {}    
  triplet ()                             : boost::tuple<double, double, double>(0, 0, 0) {}

  double& x () {return boost::get<0>(*this);}
  double& y () {return boost::get<1>(*this);}
  double& z () {return boost::get<2>(*this);}

  double x () const {return boost::get<0>(*this);}
  double y () const {return boost::get<1>(*this);}
  double z () const {return boost::get<2>(*this);}

  double  angle (const triplet& other) const;  
  triplet cross (const triplet& other) const;
  double  dot   (const triplet& other) const;
  double  norm  () const {return sqrt(pow(this->x(), 2) + pow(this->y(), 2) + pow(this->z(), 2));}
  void    normalise (); 

  void operator/= (double scale);
  void operator*= (double scale);  
  void operator-= (const triplet& other);
  void operator+= (const triplet& other);

  void rotatexy (double degrees, const triplet& around = zero); // Special for rotating the xy projection.  

private:
  static const triplet zero; 
};

triplet operator- (triplet one, triplet two);
triplet operator+ (triplet one, triplet two);
triplet operator* (triplet one, double scale);
triplet operator/ (triplet one, double scale);

int convertFractionToInt (double fraction);
bool intersect (double line1x1, double line1y1, double line1x2, double line1y2,
		double line2x1, double line2y1, double line2x2, double line2y2); 
string remQuotes (string tag);
string addQuotes (string tag);
bool contains (vector<triplet> const& polygon, triplet const& point); 
doublet calcMeanAndSigma (vector<double>& data);

template <class T>
T getKeyByWeight (const map<T, int>& mymap) {
  // What if some of the weights are negative? 
  
  if (mymap.empty()) return (*(mymap.begin())).first; 
  
  int totalWeight = 0;
  for (typename map<T, int>::const_iterator i = mymap.begin(); i != mymap.end(); ++i) {
    totalWeight += (*i).second;
  }
  if (0 == totalWeight) {
    int roll = rand() % mymap.size();
    typename map<T, int>::const_iterator i = mymap.begin();
    for (int j = 0; j < roll; ++j) ++i;
    return (*i).first;
  }

  int roll = rand() % totalWeight;
  int counter = 0; 
  for (typename map<T, int>::const_iterator i = mymap.begin(); i != mymap.end(); ++i) {
    if (0 == (*i).second) continue;
    if (counter >= roll) return (*i).first;
    counter += (*i).second;
  }
  return (*(mymap.begin())).first;
}

template <class T> class Iterable {
 public:

  Iterable<T> (int i) {} // Constructor for mirrors, which we don't want to iterate over. Don't make it empty, to avoid accidents. 
  Iterable<T> (T* dat) {allThings.push_back(dat);} 
  ~Iterable<T> () {
    for (unsigned int i = 0; i < allThings.size(); ++i) {
      if (allThings[i] != this) continue;
      allThings[i] = allThings.back();
      allThings.pop_back(); 
      break;
    }
  }

  typedef vector<T*> Container;
  typedef typename vector<T*>::iterator Iter;
  typedef typename vector<T*>::iterator Iterator;
  typedef typename vector<T*>::const_iterator cIter;
  typedef typename vector<T*>::reverse_iterator rIter;
  typedef typename vector<T*>::reverse_iterator rIterator;

  static Container makeCopy () {
    Container ret;
    for (Iter r = start(); r != final(); ++r) ret.push_back(*r);
    return ret;
  }

  static Container& getAll () {return allThings;}
  static Iter start () {return allThings.begin();}
  static Iter final () {return allThings.end();}
  static rIter rstart () {return allThings.rbegin();}
  static rIter rfinal () {return allThings.rend();}

  static unsigned int totalAmount () {return allThings.size();}
  static void clear () {
    while (0 < totalAmount()) {
      delete allThings.back();
    }
  }

 private:
  static vector<T*> allThings;
};

template <class T> vector<T*> Iterable<T>::allThings; 

template <class T> class Finalizable {
public:
  Finalizable<T> (bool final = false) {
    assert(!s_Final);
    if (final) freeze();
  }

protected:
  static void thaw () {s_Final = false;}
  static void freeze () {s_Final = true;}
  
private:
  static bool s_Final;
};

template<class T> bool Finalizable<T>::s_Final = false; 

template<class T, bool unique=true> class Named {
public:
  Named(const string& n, T* dat) : name(n) {
    if (unique) {
      assert(!nameToObjectMap[name]);
    }
    nameToObjectMap[name] = dat;
  }
  Named () : name("ToBeNamed") {}
  string getName () const {return name;}
  string getName (int space) const {return name + string("").insert(0, space - name.size(), ' ');}
  void resetName (string n) {T* dat = nameToObjectMap[name]; assert(dat); nameToObjectMap[n] = dat; name = n;}
  // Use setName for objects that don't have a name yet.
  void setName (string n) {assert(name == "ToBeNamed"); if (unique) assert(!nameToObjectMap[n]); nameToObjectMap[n] = (T*) this; name = n;}
  static T* getByName (const string& n) {assert(unique); return nameToObjectMap[n];}
  static T* findByName (const string& n) {return getByName(n);}
  static void clear () {nameToObjectMap.clear();}
private:
  string name;
  static unordered_map<string, T*> nameToObjectMap;
};

template <class T, bool unique>
unordered_map<string, T*> Named<T, unique>::nameToObjectMap;

template<class T> class Numbered {
public:
  Numbered<T> (T* dat, unsigned int i) : idx(UINT_MAX) {
    setIdx(i);
  }

  Numbered<T> (T* dat) : idx(theNumbers.size()) {
    theNumbers.push_back(dat);
  }

  Numbered<T> () : idx(UINT_MAX) {} // Empty constructor awaits later setting of id.

  void setIdx (unsigned int id) {
    assert(UINT_MAX == idx);
    if (id >= theNumbers.size()) theNumbers.resize(id+1);
    assert(!theNumbers[id]);
    theNumbers[id] = (T*) this;
    idx = id;
  }
  unsigned int getIdx () const {return idx;}
  static T* getByIndex (unsigned int i) {if (i >= theNumbers.size()) return 0; return theNumbers[i];}
  operator unsigned int() const {return idx;}
  bool operator< (unsigned int i) const {return idx < i;}
  bool operator<  (const Numbered<T>& other) {return idx <  other.idx;}
  bool operator<= (const Numbered<T>& other) {return idx <= other.idx;}
  bool operator>  (const Numbered<T>& other) {return idx >  other.idx;}
  bool operator>= (const Numbered<T>& other) {return idx >= other.idx;}

protected:
  static void clear () {
    theNumbers.clear();
  }
  
private:
  unsigned int idx; 
  static vector<T*> theNumbers;
};

template<class T> vector<T*> Numbered<T>::theNumbers; 

template<class T> class Enumerable : public Finalizable<T>, public Iterable<T>, public Named<T>, public Numbered<T> {
public:
  Enumerable<T> (T* dat, string n, int i, bool final = false) 
    : Finalizable<T>(final)
    , Iterable<T>(dat)
    , Named<T>(n, dat)
    , Numbered<T>(dat, i)
  {}

  Enumerable<T> (T* dat, string n, bool final = false) 
    : Finalizable<T>(final)
    , Iterable<T>(dat)
    , Named<T>(n, dat)
    , Numbered<T>(dat)
  {}
 
  static unsigned int numTypes () {return Iterable<T>::totalAmount();}

protected:
  static void clear () {
    Named<T>::clear();
    Iterable<T>::clear();
    Numbered<T>::clear();
    Finalizable<T>::thaw();
  }
  
private:
};

struct ObjectWrapper {
  ObjectWrapper (Object* o) : object(o) {}
  virtual ~ObjectWrapper () {}

  string  getKey () const {return object->getKey();}
  objvec  getLeaves () {return object->getLeaves();}
  Object* getNeededObject (string key) {return object->getNeededObject(key);}
  objvec  getValue (string key) const {return object->getValue(key);}
  void    resetLeaf (string k, string value) {object->resetLeaf(k, value);}
  void    resetLeaf (string k, int value) {object->resetLeaf(k, value);}
  void    resetLeaf (string k, unsigned int value) {object->resetLeaf(k, value);}
  void    resetLeaf (string k, double value) {object->resetLeaf(k, value);}
  double  safeGetFloat (string key, double def = 0) const {return object->safeGetFloat(key, def);}
  int     safeGetInt (string key, int def = 0) const {return object->safeGetInt(key, def);}
  string  safeGetString (string key, string def = "") const {return object->safeGetString(key, def);}
  Object* safeGetObject (string key, Object* def = 0) const {return object->safeGetObject(key, def);}
  void    setLeaf (string key, string value) {object->setLeaf(key, value);}
  void    setValue (Object* val, Object* beforeThis = 0) {object->setValue(val, beforeThis);}
  void    unsetValue (string val) {object->unsetValue(val);}
  void    unsetKeyValue (string key, string val) {object->unsetKeyValue(key, val);}
  void    removeObject (Object* target) {object->removeObject(target);}
  void    resetHistory (string k, string value) {object->getNeededObject("history")->resetLeaf(k, value);}
  
  Object* object;
};

template<class T, void (T::*fPtr)()> class CallbackRegistry {
public:
  CallbackRegistry () {}
  ~CallbackRegistry () {}

  void registerCallback (T* dat) {toCall.push_back(dat);}
  void unregister (T* dat) {typename vector<T*>::iterator pos = find(toCall.begin(), toCall.end(), dat); if (pos != toCall.end()) toCall.erase(pos);}
  void call () {call(fPtr);}
  void call (void (T::*otherPtr)()) {for (typename vector<T*>::iterator i = toCall.begin(); i != toCall.end(); ++i) {((*i)->*(otherPtr))();}}
  
private:
  vector<T*> toCall;
};

#define REMOVE(from, dis) from.erase(find(from.begin(), from.end(), dis))
string createString (const char* format, ...);
void throwFormatted (const char* format, ...);

struct ObjectSorter {
  ObjectSorter (string k) {keyword = k;}
  string keyword;
};
struct ObjectAscendingSorter : public ObjectSorter {
public:
  ObjectAscendingSorter (string k) : ObjectSorter(k) {}
  bool operator() (Object* one, Object* two) {return (one->safeGetFloat(keyword) < two->safeGetFloat(keyword));}
  bool operator() (ObjectWrapper* one, ObjectWrapper* two) {return (*this)(one->object, two->object);}
private:
};
struct ObjectDescendingSorter : public ObjectSorter {
public:
  ObjectDescendingSorter (string k) : ObjectSorter(k) {}
  bool operator() (Object* one, Object* two) {return (one->safeGetFloat(keyword) > two->safeGetFloat(keyword));}
  bool operator() (ObjectWrapper* one, ObjectWrapper* two) {return (*this)(one->object, two->object);}
private:
};

bool hasPrefix (string prefix, string candidate);
double calcAvg (Object* ofthis);

double getLevyStrength(const string& key, Object* levyObject, int idx = 1);

string nameAndNumber(const ObjectWrapper* prov, string key = "name");
string nameAndNumber(Object* prov, string key = "name");

#endif
