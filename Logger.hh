#ifndef LOGGER_HH
#define LOGGER_HH

#include <QObject> 
#include <string> 
#include <map> 
#include "Parser.hh"
#include <ostream> 

enum Debugs {DebugLeaders = 30,
	     DebugProvinces,
	     DebugMinisters,
	     DebugTechTeams,
	     DebugStratResources,
	     DebugTechs,
	     DebugBuildings,
	     DebugUnits,
	     DebugStockpiles,
	     DebugLaws,
	     DebugMisc,
	     DebugRevolters,
	     DebugCores,
	     DebugCountries,
	     DebugBasetax,
	     DebugManpower,
	     DebugCulture,
	     DebugReligion,
	     DebugGovernments,
	     DebugIndustry,
	     DebugCities,
	     DebugDiplomacy,
	     DebugResources, 
	     NumDebugs};

class Logger : public QObject, public std::ostream {
  Q_OBJECT 
public: 
  Logger (); 
  ~Logger (); 
  enum DefaultLogs {Debug = 0, Trace, Game, Warning, Error}; 

  Logger& append (unsigned int prec, double val); 
  
  Logger& operator<< (std::string dat);
  Logger& operator<< (QString dat);
  Logger& operator<< (int dat);
  Logger& operator<< (unsigned int dat);
  Logger& operator<< (double dat);
  Logger& operator<< (char dat);
  Logger& operator<< (char* dat);
  Logger& operator<< (const char* dat);
  Logger& operator<< (Object* dat); 
  void setActive (bool a) {active = a;}
  void setPrecision (int p = -1) {precision = p;}
  bool isActive () const {return active;} 
  
  static void createStream (int idx); 
  static Logger& logStream (int idx); 

  // Inherited from ostream. 
  std::ostream& flush (); 
  
signals:
  void message (QString m);
  
private:
  
  bool active;
  QString buffer; 
  int precision;
  
  static std::map<int, Logger*> logs; 
};


#endif
