#ifndef WINDOW_HH
#define WINDOW_HH

#include <QtGui>
#include <QObject>
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
  void convert ();
  void message (QString m);

private:
  void closeDebugLog ();
  bool createOutputDir ();
  void openDebugLog (string fname);
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

#endif

