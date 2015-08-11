#ifndef CONVERTER_HH
#define CONVERTER_HH

#include <QThread>
#include <fstream>
#include <map>
#include <string>

using namespace std;

enum TaskType {LoadFile = 0,
	       Convert,
	       NumTasks};

class Object;
class Window;
class Converter : public QThread {
public:
  Converter (Window* ow, string fname, TaskType aTask = NumTasks);
  ~Converter ();
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
  bool createProvinceMap ();
  void loadFiles ();

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

  Window* outputWindow;
};

#endif

