#ifndef CLEANERWINDOW_HH
#define CLEANERWINDOW_HH

#include <QtGui>
#include <QObject>
#include <QThread>
#include "Object.hh"
#include <map>
#include <fstream>

using namespace std;

class WorkerThread;

enum TaskType {LoadFile = 0,
	       Convert,
	       NumTasks};

class CleanerWindow : public QMainWindow {
  Q_OBJECT

public:
  CleanerWindow (QWidget* parent = 0);
  ~CleanerWindow ();

  QPlainTextEdit* textWindow;
  WorkerThread* worker;
  void openDebugLog (string fname);
  void closeDebugLog ();
  void loadFile (string fname, TaskType autoTask = NumTasks);

public slots:

  void loadFile ();
  void convert ();
  void message (QString m);

private:
};

struct ObjectSorter {
  ObjectSorter (string k) {keyword = k;}
  string keyword;
};
struct ObjectAscendingSorter : public ObjectSorter {
public:
  ObjectAscendingSorter (string k) : ObjectSorter(k) {}
  bool operator() (Object* one, Object* two) {return (one->safeGetFloat(keyword) < two->safeGetFloat(keyword));}
private:
};
struct ObjectDescendingSorter : public ObjectSorter {
public:
  ObjectDescendingSorter (string k) : ObjectSorter(k) {}
  bool operator() (Object* one, Object* two) {return (one->safeGetFloat(keyword) > two->safeGetFloat(keyword));}
private:
};

double calcAvg (Object* ofthis);

class WorkerThread : public QThread {
  Q_OBJECT
public:
  WorkerThread (string fname, TaskType aTask = NumTasks);
  ~WorkerThread ();
  void setTask(TaskType t) {task = t;}

protected:
  void run ();

private:
  // Misc globals
  string targetVersion;
  string sourceVersion;
  string fname;
  Object* ck2Game;
  Object* eu4Game;
  TaskType task;
  Object* configObject;
  TaskType autoTask;

  // Conversion processes

  // Infrastructure
  void loadFile (string fname);
  void convert ();
  void configure ();

  // Initialisers
  bool createCountryMap ();
  bool createOutputDir ();
  bool createProvinceMap ();
  void loadFiles ();
  void setupDebugLog ();

  // Helpers:
  void cleanUp ();
  Object* loadTextFile (string fname);
  bool swapKeys (Object* one, Object* two, string key);

  // Maps

  // Lists

  // Input info
  Object* provinceMapObject;
  Object* countryMapObject;
  Object* customObject;
};

#endif

