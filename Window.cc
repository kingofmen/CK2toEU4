#include "Window.hh" 
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

Window* parentWindow;
ofstream* debugFile = 0; 

int main (int argc, char** argv) {
  QApplication industryApp(argc, argv);
  QDesktopWidget* desk = QApplication::desktop();
  QRect scr = desk->availableGeometry();
  parentWindow = new Window();
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
  delete parentWindow;
  return ret; 
}


Window::Window (QWidget* parent) 
  : QMainWindow(parent)
  , worker(0)
{}

Window::~Window () {
  closeDebugLog();
}

void Window::message (QString m) {
  textWindow->appendPlainText(m);
  if (debugFile) (*debugFile) << m.toAscii().data() << std::endl;      
}

void Window::loadFile () {
  openDebugLog("Output\\logfile.txt");
  QString filename = QFileDialog::getOpenFileName(this, tr("Select file"), QString(""), QString("*.v2"));
  string fn(filename.toAscii().data());
  if (fn == "") return;
  loadFile(fn);   
}

void Window::loadFile (string fname, TaskType autoTask) {
  openDebugLog("Output\\logfile.txt");
  if (worker) delete worker;
  worker = new Converter(fname, autoTask);
  worker->start();
}

void Window::convert () {
  if (!worker) {
    Logger::logStream(Logger::Game) << "No file loaded.\n";
    return;
  }
  Logger::logStream(Logger::Game) << "Convert to EU4.\n";
  worker->setTask(Convert); 
  worker->start(); 
}

void Window::closeDebugLog () {
  if (!debugFile) return;
  debugFile->close();
  delete debugFile;
  debugFile = 0; 
}

bool Window::createOutputDir () {
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

void Window::openDebugLog (string fname) {
  if (fname == "") return;
  if (!createOutputDir()) return;
  if (debugFile) closeDebugLog();
  Logger::logStream(Logger::Game) << "Opening debug log " << fname << "\n";
  DWORD attribs = GetFileAttributesA(fname.c_str());
  if (attribs != INVALID_FILE_ATTRIBUTES) {
    int error = remove(fname.c_str());
    if (0 != error) Logger::logStream(Logger::Warning) << "Warning: Could not delete old log file. New one will be appended.\n";
  }

  debugFile = new ofstream(fname.c_str(), ios_base::trunc);
}

