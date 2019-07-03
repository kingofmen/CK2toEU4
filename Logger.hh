#ifndef LOGGER_HH
#define LOGGER_HH

#include <QObject>
#include <string>
#include <map>
#include <ostream>
#include <fstream>

#include "Parser.hh"
#include "UtilityFunctions.hh"

class LogStream : public Enumerable<const LogStream> {
public:
  LogStream (string n) : Enumerable<const LogStream>(this, n, false) {}
  ~LogStream () {}

  static LogStream const* const Debug;
  static LogStream const* const Info;
  static LogStream const* const Warn;
  static LogStream const* const Error;
};

class LogOption : public Enumerable<const LogOption> {
public:
  LogOption (string n, bool last) : Enumerable<const LogOption>(this, n, last) {}
  static LogOption const* const Indent;
  static LogOption const* const Unindent;
  static LogOption const* const Undent;
};

class Logger : public QObject, public std::ostream {
  Q_OBJECT
public:
  Logger (string n);
  ~Logger ();

  Logger& operator<< (std::string dat);
  Logger& operator<< (QString dat);
  Logger& operator<< (int dat);
  Logger& operator<< (unsigned int dat);
  Logger& operator<< (double dat);
  Logger& operator<< (char dat);
  Logger& operator<< (char* dat);
  Logger& operator<< (const char* dat);
  Logger& operator<< (Object* dat);
  Logger& operator<< (LogOption const* const opt);
  void setActive (bool a) {active = a;}
  void setPrecision (int p = -1) {precision = p;}
  bool isActive () const {return active;}

  static Logger* createStream (LogStream const* const str);
  static Logger& logStream (int idx);
  static Logger& logStream (LogStream const* const str);
  static Logger& logStream (LogStream const& str);
  static Logger& logStream (const string& ls);
  static void setLogFile(std::ofstream* file);
  static ofstream* getLogFile() { return logFile; }

signals:
  void message (QString m);

private:
  bool active;
  QString buffer;
  int precision;
  string name;

  static int indent;
  static std::map<int, Logger*> logs;
  static std::ofstream* logFile;

  Logger& sendMessage ();
};


#endif
