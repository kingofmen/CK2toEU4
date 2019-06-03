#include "Window.hh"

#include <algorithm>
#include <cstdio>
#include <direct.h>
#include <iostream>
#include <set>
#include <string>
#include <windows.h>

#include <QApplication>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QPainter>
#include <QRect>
#include <QtGui>

#include "Logger.hh"
#include "Parser.hh"
#include "StructUtils.hh"
#include "StringManips.hh"
#include "UtilityFunctions.hh"

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
  parentWindow->setWindowTitle(QApplication::translate("toplevel", "CK2 to EU4 converter"));

  QMenuBar* menuBar = parentWindow->menuBar();
  QMenu* fileMenu = menuBar->addMenu("File");
  QAction* newGame = fileMenu->addAction("Load file");
  QAction* quit    = fileMenu->addAction("Quit");
  QObject::connect(quit, SIGNAL(triggered()), parentWindow, SLOT(close()));
  QObject::connect(newGame, SIGNAL(triggered()), parentWindow, SLOT(loadFile()));

  QMenu* actionMenu = menuBar->addMenu("Actions");
  QAction* convert = actionMenu->addAction("Convert to EU4");
  QAction* debugParser = actionMenu->addAction("Debug parser");
  QAction* dejures = actionMenu->addAction("Dejure lieges");
  QAction* dynastyScore = actionMenu->addAction("Dynastic scores");
  QAction* checkProvinces = actionMenu->addAction("Check provinces");
  QAction* playerWars = actionMenu->addAction("Player wars");
  QObject::connect(convert, SIGNAL(triggered()), parentWindow, SLOT(convert()));
  QObject::connect(debugParser, SIGNAL(triggered()), parentWindow, SLOT(debugParser()));
  QObject::connect(dynastyScore, SIGNAL(triggered()), parentWindow, SLOT(dynasticScore()));
  QObject::connect(checkProvinces, SIGNAL(triggered()), parentWindow, SLOT(checkProvinces()));
  QObject::connect(playerWars, SIGNAL(triggered()), parentWindow, SLOT(playerWars()));
  QObject::connect(dejures, SIGNAL(triggered()), parentWindow, SLOT(dejures()));

  parentWindow->textWindow = new QPlainTextEdit(parentWindow);
  parentWindow->textWindow->setFixedSize(3*scr.width()/5 - 10, scr.height()/2-40);
  parentWindow->textWindow->move(5, 30);
  parentWindow->textWindow->show();

  Logger::createStream(LogStream::Debug);
  Logger::createStream(LogStream::Info);
  Logger::createStream(LogStream::Warn);
  Logger::createStream(LogStream::Error);

  QObject::connect(&(Logger::logStream(LogStream::Debug)), SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(LogStream::Info)),  SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(LogStream::Warn)),  SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(LogStream::Error)), SIGNAL(message(QString)), parentWindow, SLOT(message(QString)));

  parentWindow->show();
  if (argc > 1) {
    parentWindow->loadFile(argv[1]);
  }
  if ((argc > 2) && ((unsigned int) atoi(argv[2]) == ConverterJob::Convert->getIdx())) {
    while (!parentWindow->worker) {
      Sleep(1000);
      continue;
    }
    parentWindow->worker->scheduleJob(ConverterJob::Convert);
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
  if (debugFile) (*debugFile) << m.toStdString() << std::endl;
}

void Window::loadFile () {
  QString filename = QFileDialog::getOpenFileName(this, tr("Select file"), QString(""), QString("*.ck2"));
  if (filename.isEmpty()) return;
  loadFile(filename.toStdString());
}

void Window::loadFile (string fname) {
  openDebugLog("Output\\logfile.txt");
  if (worker) delete worker;
  worker = new Converter(this, fname);
  worker->scheduleJob(ConverterJob::LoadFile);
  worker->start();
}

void Window::convert () {
  if (!worker) {
    Logger::logStream(LogStream::Info) << "No file loaded.\n";
    return;
  }
  Logger::logStream(LogStream::Info) << "Queued up conversion to EU4.\n";
  worker->scheduleJob(ConverterJob::Convert);
}

void Window::debugParser () {
  if (!worker) {
    Logger::logStream(LogStream::Info) << "No file loaded.\n";
    return;
  }
  Logger::logStream(LogStream::Info) << "Queued up debug.\n";
  worker->scheduleJob(ConverterJob::DebugParser);
}

void Window::dejures () {
  if (!worker) {
    Logger::logStream(LogStream::Info) << "No file loaded.\n";
    return;
  }
  Logger::logStream(LogStream::Info) << "Queued up dejures.\n";
  worker->scheduleJob(ConverterJob::DejureLieges);
}


void Window::dynasticScore () {
  if (!worker) {
    Logger::logStream(LogStream::Info) << "No file loaded.\n";
    return;
  }
  Logger::logStream(LogStream::Info) << "Queued up dynasty scores.\n";
  worker->scheduleJob(ConverterJob::DynastyScores);
}

void Window::checkProvinces () {
  if (!worker) {
    Logger::logStream(LogStream::Info) << "No file loaded.\n";
    return;
  }
  Logger::logStream(LogStream::Info) << "Queued up province check.\n";
  worker->scheduleJob(ConverterJob::CheckProvinces);
}

void Window::playerWars () {
  if (!worker) {
    Logger::logStream(LogStream::Info) << "No file loaded.\n";
    return;
  }
  Logger::logStream(LogStream::Info) << "Queued up player wars.\n";
  worker->scheduleJob(ConverterJob::PlayerWars);
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
    Logger::logStream(LogStream::Warn) << "Warning, no Output directory, attempting to create one.\n";
    int error = _mkdir("Output");
    if (-1 == error) {
      Logger::logStream(LogStream::Error) << "Error: Could not create Output directory. Aborting.\n";
      return false;
    }
  }
  return true;
}

void Window::openDebugLog (string fname) {
  if (fname == "") return;
  if (!createOutputDir()) return;
  if (debugFile) closeDebugLog();
  Logger::logStream(LogStream::Info) << "Opening debug log " << fname << "\n";
  DWORD attribs = GetFileAttributesA(fname.c_str());
  if (attribs != INVALID_FILE_ATTRIBUTES) {
    int error = remove(fname.c_str());
    if (0 != error) Logger::logStream(LogStream::Warn) << "Warning: Could not delete old log file. New one will be appended.\n";
  }

  debugFile = new ofstream(fname.c_str(), ios_base::trunc);
}

