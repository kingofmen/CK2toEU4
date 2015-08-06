#include "CleanerWindow.hh" 
#include <QPainter> 
#include "Parser.hh"
#include <cstdio> 
#include <QtGui>
#include <QDesktopWidget>
#include <QRect>
#include <iostream> 
#include <string>
#include "Logger.hh" 
#include <set>
#include <algorithm>
#include "StructUtils.hh" 
#include "StringManips.hh"
#include "UtilityFunctions.hh"
#include <direct.h>

using namespace std; 

CleanerWindow* parentWindow;
ofstream* debugFile = 0; 

Object* hoiCountry = 0;
Object* vicCountry = 0;
string hoiTag;
string vicTag; 

int main (int argc, char** argv) {
  QApplication industryApp(argc, argv);
  QDesktopWidget* desk = QApplication::desktop();
  QRect scr = desk->availableGeometry();
  parentWindow = new CleanerWindow();
  parentWindow->show();
  srand(42); 
  
  parentWindow->resize(3*scr.width()/5, scr.height()/2);
  parentWindow->move(scr.width()/5, scr.height()/4);
  parentWindow->setWindowTitle(QApplication::translate("toplevel", "Vic2 to HoI3 converter"));
 
  QMenuBar* menuBar = parentWindow->menuBar();
  QMenu* fileMenu = menuBar->addMenu("File");
  QAction* newGame = fileMenu->addAction("Load file");
  //QAction* newGame = fileMenu->addAction("Load-and-convert file");
  QAction* quit    = fileMenu->addAction("Quit");
  QObject::connect(quit, SIGNAL(triggered()), parentWindow, SLOT(close())); 
  QObject::connect(newGame, SIGNAL(triggered()), parentWindow, SLOT(loadFile())); 

  QMenu* actionMenu = menuBar->addMenu("Actions");
  QAction* convert = actionMenu->addAction("Convert to EU4");
  QObject::connect(convert, SIGNAL(triggered()), parentWindow, SLOT(convert()));
  
  parentWindow->textWindow = new QPlainTextEdit(parentWindow);
  parentWindow->textWindow->setFixedSize(3*scr.width()/5 - 10, scr.height()/2-40);
  parentWindow->textWindow->move(5, 30);
  parentWindow->textWindow->show(); 

  Logger::createStream(Logger::Debug);
  Logger::createStream(Logger::Trace);
  Logger::createStream(Logger::Game);
  Logger::createStream(Logger::Warning);
  Logger::createStream(Logger::Error);

  QObject::connect(&(Logger::logStream(Logger::Debug)),   SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Trace)),   SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Game)),    SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Warning)), SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Error)),   SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));

  for (int i = DebugLeaders; i < NumDebugs; ++i) {
    Logger::createStream(i);
    QObject::connect(&(Logger::logStream(i)), SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
    Logger::logStream(i).setActive(false); 
  } 

  parentWindow->show();
  if (argc > 1) {
    parentWindow->loadFile(argv[2], (TaskType) atoi(argv[1])); 
  }
  int ret = industryApp.exec();
  parentWindow->closeDebugLog();   
  return ret; 
}


CleanerWindow::CleanerWindow (QWidget* parent) 
  : QMainWindow(parent)
  , worker(0)
{}

CleanerWindow::~CleanerWindow () {}

void CleanerWindow::message (QString m) {
  textWindow->appendPlainText(m);
  if (debugFile) (*debugFile) << m.toAscii().data() << std::endl;      
}

void CleanerWindow::loadFile () {
  QString filename = QFileDialog::getOpenFileName(this, tr("Select file"), QString(""), QString("*.v2"));
  string fn(filename.toAscii().data());
  if (fn == "") return;
  loadFile(fn);   
}

void CleanerWindow::loadFile (string fname, TaskType autoTask) {
  if (worker) delete worker;
  worker = new WorkerThread(fname, autoTask);
  worker->start();
}

void CleanerWindow::convert () {
  if (!worker) {
    Logger::logStream(Logger::Game) << "No file loaded.\n";
    return;
  }
  Logger::logStream(Logger::Game) << "Convert to EU4.\n";
  worker->setTask(Convert); 
  worker->start(); 
}

void CleanerWindow::closeDebugLog () {
  if (!debugFile) return;
  debugFile->close();
  delete debugFile;
  debugFile = 0; 
}

void CleanerWindow::openDebugLog (string fname) {
  if (fname == "") return;
  if (debugFile) closeDebugLog();
  debugFile = new ofstream(fname.c_str(), ios_base::trunc);
}

WorkerThread::WorkerThread (string fn, TaskType atask)
  : targetVersion("")
  , sourceVersion("")
  , fname(fn)
  , ck2Game(0)
  , eu4Game(0)
  , task(LoadFile)
  , configObject(0)
  , autoTask(atask)
  , provinceMapObject(0)
  , countryMapObject(0)
  , customObject(0)
{
  configure(); 
  if (!createOutputDir()) {
    Logger::logStream(Logger::Error) << "Error: No output directory, could not create one. Fix this before proceeding.\n";
    autoTask = NumTasks; 
  }  
}  

WorkerThread::~WorkerThread () {
  if (eu4Game) delete eu4Game;
  if (ck2Game) delete ck2Game; 
  eu4Game = 0;
  ck2Game = 0; 
}

void WorkerThread::run () {
  switch (task) {
  case LoadFile: loadFile(fname); break;
  case Convert: convert(); break;
  case NumTasks: 
  default: break; 
  }
}

void WorkerThread::loadFile (string fname) {
  ck2Game = loadTextFile(fname);
  if (NumTasks != autoTask) {
    task = autoTask; 
    switch (autoTask) {
    case Convert:
      convert(); 
      break;
    default:
      break;
    }
  }
  Logger::logStream(Logger::Game) << "Ready to convert.\n";
}

void WorkerThread::cleanUp () {
}

void WorkerThread::configure () {
  setupDebugLog();  
  configObject = processFile("config.txt");
  targetVersion = configObject->safeGetString("hoidir", targetVersion);
  sourceVersion = configObject->safeGetString("vicdir", sourceVersion);
  Logger::logStream(Logger::Debug).setActive(false);

  Object* debug = configObject->safeGetObject("debug");
  if (debug) {
    if (debug->safeGetString("generic", "no") == "yes") Logger::logStream(Logger::Debug).setActive(true);
    bool activateAll = (debug->safeGetString("all", "no") == "yes");
    for (int i = DebugLeaders; i < NumDebugs; ++i) {
      sprintf(strbuffer, "%i", i);
      if ((activateAll) || (debug->safeGetString(strbuffer, "no") == "yes")) Logger::logStream(i).setActive(true);
    }
  }
}

Object* WorkerThread::loadTextFile (string fname) {
  Logger::logStream(Logger::Game) << "Parsing file " << fname << "\n";
  ifstream reader;
  reader.open(fname.c_str());
  if ((reader.eof()) || (reader.fail())) {
    Logger::logStream(Logger::Error) << "Could not open file, returning null object.\n";
    return 0; 
  }
  
  Object* ret = processFile(fname);
  Logger::logStream(Logger::Game) << " ... done.\n";
  return ret; 
}

string nameAndNumber (Object* eu3prov) {
  return eu3prov->getKey() + " (" + remQuotes(eu3prov->safeGetString("name", "\"could not find name\"")) + ")";
}

bool WorkerThread::swapKeys (Object* one, Object* two, string key) {
  if (one == two) return true; 
  Object* valOne = one->safeGetObject(key);
  if (!valOne) return false;
  Object* valTwo = two->safeGetObject(key);
  if (!valTwo) return false;

  one->unsetValue(key);
  two->unsetValue(key);
  one->setValue(valTwo);
  two->setValue(valOne);
  return true; 
}


/********************************  End helpers  **********************/

/******************************** Begin initialisers **********************/

bool WorkerThread::createCountryMap () {
  if (!countryMapObject) {
    Logger::logStream(Logger::Error) << "Error: Could not find country-mapping object.\n";
    return false; 
  }

  return true; 
}

bool WorkerThread::createOutputDir () {
  DWORD attribs = GetFileAttributesA("Output");
  if (attribs == INVALID_FILE_ATTRIBUTES) {
    Logger::logStream(Logger::Warning) << "Warning, no Output directory, attempting to create one.\n";
    int error = _mkdir("Output");
    if (-1 == error) {
      Logger::logStream(Logger::Error) << "Error: Could not create Output directory. Aborting.\n";
      return false; 
    }
  }
  return true; 
}

bool WorkerThread::createProvinceMap () {
  if (!provinceMapObject) {
    Logger::logStream(Logger::Error) << "Error: Could not find province-mapping object.\n";
    return false; 
  }

  return true; 
}

void WorkerThread::loadFiles () {
}

void WorkerThread::setupDebugLog () {
  string debuglog = "logfile.txt";
  string outputdir = "Output\\";
  debuglog = outputdir + debuglog;
  Logger::logStream(Logger::Game) << "Opening debug log " << debuglog << "\n"; 
    
  DWORD attribs = GetFileAttributesA(debuglog.c_str());
  if (attribs != INVALID_FILE_ATTRIBUTES) {
    int error = remove(debuglog.c_str());
    if (0 != error) Logger::logStream(Logger::Warning) << "Warning: Could not delete old log file. New one will be appended.\n";
  }
  parentWindow->openDebugLog(debuglog);
}

/******************************* End initialisers *******************************/ 


/******************************* Begin conversions ********************************/

/******************************* End conversions ********************************/

/*******************************  Begin calculators ********************************/

double calcAvg (Object* ofthis) {
  if (!ofthis) return 0; 
  int num = ofthis->numTokens();
  if (0 == num) return 0;
  double ret = 0;
  for (int i = 0; i < num; ++i) {
    ret += ofthis->tokenAsFloat(i);
  }
  ret /= num;
  return ret; 
}

/******************************* End calculators ********************************/

void WorkerThread::convert () {
  if (!ck2Game) {
    Logger::logStream(Logger::Game) << "No file loaded.\n";
    return; 
  }

  Logger::logStream(Logger::Game) << "Loading HoI source file.\n";
  eu4Game = loadTextFile(targetVersion + "input.hoi3");

  loadFiles();
  if (!createProvinceMap()) return; 
  if (!createCountryMap()) return;
  cleanUp();
  
  Logger::logStream(Logger::Game) << "Done with conversion, writing to Output/converted.hoi3.\n";
 
  ofstream writer;
  writer.open(".\\Output\\converted.hoi3");
  Parser::topLevel = eu4Game;
  writer << (*eu4Game);
  writer.close();
  Logger::logStream(Logger::Game) << "Done writing.\n";
}

