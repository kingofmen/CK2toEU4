#ifndef WINDOW_HH
#define WINDOW_HH

#include <QtGui>
#include <QMainWindow>
#include <QObject>
#include <QPlainTextEdit>
#include "Object.hh"
#include <map>
#include <fstream>

#include "Converter.hh"

class Window : public QMainWindow {
  Q_OBJECT

public:
  Window (QWidget* parent = 0);
  ~Window ();

  QPlainTextEdit* textWindow;
  Converter* worker;
  void loadFile (string fname);

public slots:
  void loadFile ();
  void checkProvinces ();
  void convert ();
  void debugParser ();
  void dejures ();
  void dynasticScore ();
  void playerWars ();
  void message (QString m);

private:
  void closeDebugLog ();
  bool createOutputDir ();
  void openDebugLog (string fname);
};

#endif

