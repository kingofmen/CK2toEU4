#ifndef STRING_MANIPS_HH
#define STRING_MANIPS_HH

#include <string>
#include "Object.hh"
using namespace std; 

double days (string datestring);
string getField (string str, int field, char separator = ' ');
int year (string str); 
string convertMonth (string month);
string convertMonth (int month);
void setIdAndType (Object* obj, int id, int type); 
string ordinal (int i);
Object* pop (objvec& dat); 

#endif
