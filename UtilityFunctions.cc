#include "UtilityFunctions.hh"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <stdarg.h>

char strbuffer[1000]; 
const doublet doublet::zero(0, 0);
const triplet triplet::zero(0, 0, 0); 

const string QuotedNone("\"none\"");
const string PlainNone("none");

double degToRad (double degrees) {
  return degrees * 3.14159265 / 180; 
}
double radToDeg (double radians) {
  return radians * 180 / 3.14159265; 
}

triplet triplet::cross (const triplet& other) const {
  triplet ret;
  ret.x() = this->y() * other.z() - this->z() * other.y();
  ret.y() = this->z() * other.x() - this->x() * other.z();
  ret.z() = this->x() * other.y() - this->y() * other.x(); 
  return ret;
}

double triplet::dot (const triplet& other) const {
  double ret = this->x() * other.x();
  ret       += this->y() * other.y();
  ret       += this->z() * other.z();
  return ret; 
}

double triplet::angle (const triplet& other) const {
  double ret = this->dot(other);
  ret /= this->norm();
  ret /= other.norm();
  return acos(ret); 
}

void triplet::normalise () {
  (*this) /= this->norm();
}

void triplet::rotatexy (double degrees, const triplet& around) {
  (*this) -= around;
  double newx = this->x() * cos(degToRad(degrees)) - this->y() * sin(degToRad(degrees));
  double newy = this->x() * sin(degToRad(degrees)) + this->y() * cos(degToRad(degrees));
  this->x() = newx;
  this->y() = newy; 
  (*this) += around;
}

void triplet::operator-= (const triplet& other) {
  this->x() -= other.x();
  this->y() -= other.y();
  this->z() -= other.z();  
}
void triplet::operator+= (const triplet& other) {
  this->x() += other.x();
  this->y() += other.y();
  this->z() += other.z();  
}
void triplet::operator/= (double scale) {
  this->x() /= scale;
  this->y() /= scale;
  this->z() /= scale;
}
void triplet::operator*= (double scale) {
  this->x() *= scale;
  this->y() *= scale;
  this->z() *= scale;
}

triplet operator- (triplet one, triplet two) {
  triplet ret = one;
  ret -= two;
  return ret;
}

triplet operator+ (triplet one, triplet two) {
  triplet ret = one;
  ret += two;
  return ret;
}

triplet operator* (triplet one, double scale) {
  triplet ret = one;
  ret *= scale;
  return ret;
}

triplet operator/ (triplet one, double scale) {
  triplet ret = one;
  ret /= scale;
  return ret; 
}


double doublet::dot (const doublet& other) const {
  double ret = this->x() * other.x();
  ret       += this->y() * other.y();
  return ret; 
}

double doublet::angle (const doublet& other) const {
  double ret = this->dot(other);
  ret /= this->norm();
  ret /= other.norm();
  return acos(ret); 
}

void doublet::normalise () {
  (*this) /= this->norm();
}

void doublet::operator-= (const doublet& other) {
  this->x() -= other.x();
  this->y() -= other.y();
}
void doublet::operator+= (const doublet& other) {
  this->x() += other.x();
  this->y() += other.y();
}

//void doublet::operator+= (const triplet& other) {
//this->x() += other.x();
//this->y() += other.y();
//}

void doublet::operator/= (double scale) {
  this->x() /= scale;
  this->y() /= scale;
}
void doublet::operator*= (double scale) {
  this->x() *= scale;
  this->y() *= scale;
}

doublet& doublet::operator= (const boost::tuple<double, double, double>& rhs) {
  this->x() = boost::get<0>(rhs);
  this->y() = boost::get<1>(rhs);
  return *this;
}

void doublet::rotate (double degrees, const doublet& around) {
  (*this) -= around;
  double newx = this->x() * cos(degToRad(degrees)) - this->y() * sin(degToRad(degrees));
  double newy = this->x() * sin(degToRad(degrees)) + this->y() * cos(degToRad(degrees));
  this->x() = newx;
  this->y() = newy; 
  (*this) += around;
}

int convertFractionToInt (double fraction) {
  // Returns the integer part of fraction,
  // plus 1 with probability equal to the
  // fractional part. 
  
  int ret = (int) floor(fraction);
  fraction -= ret;
  double roll = rand();
  roll /= RAND_MAX;
  if (roll < fraction) ret++;
  return ret; 
}

bool intersect (double line1x1, double line1y1, double line1x2, double line1y2,
		double line2x1, double line2y1, double line2x2, double line2y2) {

  double line1A = line1y2 - line1y1;
  double line1B = line1x1 - line1x2; 
  double line1C = line1A*line1x1 + line1B*line1y1;

  double line2A = line2y2 - line2y1;
  double line2B = line2x1 - line2x2; 
  double line2C = line2A*line2x1 + line2B*line2y1;

  double det = line1A*line2B - line1B*line2A;
  if (fabs(det) < 0.00001) return false; // Parallel lines

  double xinter = (line2B*line1C - line1B*line2C)/det;
  static const double tol = 0.0001;
  if (xinter < std::min(line1x1, line1x2) - tol) return false;  // Small tolerance accounts for roundoff error
  if (xinter > std::max(line1x1, line1x2) + tol) return false;
  if (xinter < std::min(line2x1, line2x2) - tol) return false;
  if (xinter > std::max(line2x1, line2x2) + tol) return false;
  double yinter = (line1A*line2C - line2A*line1C)/det;
  if (yinter < std::min(line1y1, line1y2) - tol) return false;
  if (yinter > std::max(line1y1, line1y2) + tol) return false;
  if (yinter < std::min(line2y1, line2y2) - tol) return false;
  if (yinter > std::max(line2y1, line2y2) + tol) return false;

  return true;
}

string remQuotes (string tag) {
  
  string ret;
  bool found = false; 
  for (unsigned int i = 0; i < tag.size(); ++i) {
    char curr = tag[i];
    if (!found) {
      if (curr != '"') continue;
      found = true;
      continue;
    }

    if (curr == '"') break;
    ret += curr; 
  }
  if (!found) return tag;
  return ret; 
}

string addQuotes (string tag) {
  // Add surrounding quotes to tag if it doesn't already have them.
  if (tag.empty()) return "\"\"";
  string ret("\"");
  if (tag[0] == '"') ret = tag;
  else ret += tag;
  if (ret[ret.size() - 1] != '"') ret += "\"";
  return ret;
}

string outcomeToString (Outcome out) {
  switch (out) {
  case Disaster:   return "Disaster";
  case Bad:        return "Bad";
  case Neutral:    return "Neutral";
  case Good:       return "Good";
  case VictoGlory: return "VictoGlory";
  case NumOutcomes:
  default:
    return "Unknown outcome type"; 
  }

  return "This cannot happen"; 
}

doublet calcMeanAndSigma (vector<double>& data) {
  if (0 == data.size()) return doublet(0, 0);
  double mean = 0;
  double meanSq = 0;
  for (vector<double>::iterator i = data.begin(); i != data.end(); ++i) {
    mean += (*i);
    meanSq += (*i) * (*i);
  }
  mean /= data.size();
  meanSq /= data.size();
  return doublet(mean, sqrt(meanSq  - mean * mean));
}

DieRoll::DieRoll (int d, int f) 
 : dice(d)
 , faces(f)
 , next(0)
{
  if (dice > 1) next = new DieRoll(dice-1, f); 
}

double DieRoll::probability (int target, int mods, RollType t) const {
  if (1 == dice) return baseProb(target, mods, t); 
  target -= mods;
  double ret = 0;
  for (int i = 1; i <= faces; ++i) {
    ret += next->probability(target - i, 0, t);
  }
  ret /= faces; 
  return ret; 
}

int DieRoll::roll () const {
  int ret = dice; 
  for (int i = 0; i < dice; ++i) {
    ret += (rand() % faces);
  }
  return ret; 
}


double DieRoll::baseProb (int target, int mods, RollType t) const {
  // Returns probability of roll <operator> target on one die. 
  
  target -= mods; 
  double numTargets = 1.0; 
  switch (t) {
  case Equal:
    if (target < 1) return 0;
    if (target > faces) return 0; 
    return (numTargets / faces);
    
  case GtEqual:
    if (target <= 1) return 1;
    if (target > faces) return 0;
    numTargets = 1 + faces - target; 
    return numTargets/faces; 
    
  case LtEqual:
    if (target >= faces) return 1;
    if (target <= 0) return 0;
    numTargets = target; 
    return numTargets/faces; 
    
  case Greater:
    if (target < 1) return 1;
    if (target >= faces) return 0;
    numTargets = faces - target;
    return numTargets/faces; 
    
  case Less:
    if (target > faces) return 1;
    if (target < 1) return 0;
    numTargets = target - 1; 
    return numTargets/faces; 
    
  default: break; 
  }
  return 0;
}

bool contains (vector<triplet> const& polygon, triplet const& point) {
  triplet outerpoint = point;
  outerpoint.x() += 2000;
  outerpoint.y() += 2000;

  int inters = 0;
  for (unsigned int i = 1; i <= polygon.size(); ++i) {
    if (intersect(polygon[i-1].x(), polygon[i-1].y(), polygon[i].x(), polygon[i].y(),
		  point.x(), point.y(), outerpoint.x(), outerpoint.y())) inters++;    
  }

  if (intersect(polygon.back().x(), polygon.back().y(), polygon[0].x(), polygon[0].y(),
		point.x(), point.y(), outerpoint.x(), outerpoint.y())) inters++;

  if (0 == inters % 2) return false;
  return true; 
}

string createString (const char* format, ...) {
  static char message[1000];
  va_list arglist;
  va_start(arglist, format);
  vsprintf(message, format, arglist);
  va_end(arglist);
  return string(message);
}

void throwFormatted (const char* format, ...) {
  va_list arglist;
  va_start(arglist, format);
  string error = createString(format, arglist);
  va_end(arglist);
  throw error;
}

bool hasPrefix (string prefix, string candidate) {
  if (candidate.size() < prefix.size()) return false;
  for (unsigned int i = 0; i < prefix.size(); ++i) {
    if (candidate[i] != prefix[i]) return false;
  }
  return true;
}

bool yearMonthDay (const string& date, int& year, int& month, int& day) {
  int firstDotIdx = -1;
  int secondDotIdx = -1;
  for (unsigned int i = 0; i < date.size(); ++i) {
    if (isdigit(date[i])) continue;
    if (date[i] != '.') return false;
    if (firstDotIdx < 0) firstDotIdx = i;
    else if (secondDotIdx < 0) secondDotIdx = i;
    else return false;
  }
  if (firstDotIdx < 0) return false;
  if (secondDotIdx < 0) return false;
  if (firstDotIdx + 1 == secondDotIdx) return false;
  if (secondDotIdx + 1 == (int) date.size()) return false;
  year = atoi(date.substr(0, firstDotIdx).c_str());
  month = atoi(date.substr(firstDotIdx+1, secondDotIdx).c_str());
  day = atoi(date.substr(secondDotIdx).c_str());
  return true;
}

double getLevyStrength(const string& key, Object* levyObject) {
  auto* levy = levyObject->safeGetObject(key);
  if (!levy) {
    return 0;
  }
  if (levy->numTokens() == 0) {
    return levyObject->safeGetFloat(key);
  }
  return levy->tokenAsFloat(1);
}

string nameAndNumber(ObjectWrapper* prov, string key) {
  return prov->getKey() + " (" +
         remQuotes(prov->safeGetString(key, "\"could not find name\"")) + ")";
}

string nameAndNumber(Object* prov, string key) {
  return prov->getKey() + " (" +
         remQuotes(prov->safeGetString(key, "\"could not find name\"")) + ")";
}
