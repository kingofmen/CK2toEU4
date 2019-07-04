#include "Logger.hh"
#include <cassert>
#include "Parser.hh"

int Logger::indent = 0;
std::map<int, Logger*> Logger::logs;
char convertBuffer[10000];
std::ofstream* Logger::logFile;

LogStream const* const LogStream::Debug = new LogStream("debug");
LogStream const* const LogStream::Info  = new LogStream("info");
LogStream const* const LogStream::Warn  = new LogStream("warn");
LogStream const* const LogStream::Error = new LogStream("error");

LogOption const* const LogOption::Indent   = new LogOption("indent", false);
LogOption const* const LogOption::Unindent = new LogOption("unindent", true);
LogOption const* const LogOption::Undent   = LogOption::Unindent;

Logger::Logger (string n)
  : active(true)
  , buffer()
  , precision(-1)
  , name(n)
{}

Logger::~Logger () {}

void Logger::setLogFile(std::ofstream* file) {
  logFile = file;
}

Logger& Logger::operator<< (std::string dat) {
  if (!active) return *this;
  if (logFile) {
    (*logFile) << dat;
    logFile->flush();
  }
  std::size_t linebreak = dat.find_first_of('\n');
  if (std::string::npos == linebreak) buffer.append(dat.c_str());
  else {
    buffer.append(dat.substr(0, linebreak).c_str());
    sendMessage();
    (*this) << dat.substr(linebreak+1);
  }
  return *this;
}

Logger& Logger::operator<< (Object* dat) {
  if (!active) return *this;
  static int objindent = 0;
  for (int i = 0; i < objindent; i++) {
    *this << "  ";
  }
  if (dat->isLeaf()) {
    *this << dat->getKey() << " = " << dat->getLeaf() << "\n";
    return *this;
  }
  if (0 != dat->numTokens()) {
    *this << dat->getKey() << " = { " << dat->getLeaf() << " }\n";
    return *this;
  }

  if (dat != Parser::topLevel) {
    *this << dat->getKey() << " = { \n";
    objindent++;
  }
  objvec leaves = dat->getLeaves();
  for (objiter i = leaves.begin(); i != leaves.end(); ++i) {
    *this << (*i);
  }
  if (dat != Parser::topLevel) {
    objindent--;
    for (int i = 0; i < objindent; i++) {
      *this << "  ";
    }
    *this << "} \n";
  }
  return *this;
}

Logger& Logger::operator<< (char* dat) {
  if (!active) return *this;
  return ((*this) << std::string(dat));
}

Logger& Logger::operator<< (const char* dat) {
  if (!active) return *this;
  return ((*this) << std::string(dat));
}

Logger& Logger::operator<< (QString dat) {
  if (!active) return *this;
  return (*this) << dat.toStdString();
}

Logger& Logger::operator<< (int dat) {
  if (!active) return *this;
  sprintf(convertBuffer, "%i", dat);
  return (*this) << convertBuffer;
}

Logger& Logger::operator<< (unsigned int dat) {
  if (!active) return *this;
  sprintf(convertBuffer, "%i", dat);
  return (*this) << convertBuffer;
}

Logger& Logger::operator<< (double dat) {
  if (!active) return *this;
  if (precision > 0) {
    sprintf(convertBuffer, "%.*f", precision, dat);
  }
  else {
    sprintf(convertBuffer, "%f", dat);
  }
  return (*this) << convertBuffer;
}

Logger& Logger::operator<< (char dat) {
  if (!active) return *this;
  return (*this) << std::string(1, dat);
}

Logger& Logger::operator<< (LogOption const* const opt) {
  if (opt == LogOption::Indent) indent += 2;
  else if (opt == LogOption::Unindent) {
    indent -= 2;
    if (0 > indent) indent = 0;
  }
  return *this;
}

Logger* Logger::createStream (LogStream const* const str) {
  logs[*str] = new Logger(str->getName());
  return logs[*str];
}

Logger& Logger::logStream (int idx) {
  assert(logs[idx]);
  return *(logs[idx]);
}

Logger& Logger::logStream (LogStream const* const str) {
  assert(logs[*str]);
  return *(logs[*str]);
}

Logger& Logger::logStream (LogStream const& str) {
  assert(logs[str]);
  return *(logs[str]);
}

Logger& Logger::logStream (const string& ls) {
  LogStream const* const str = LogStream::findByName(ls);
  return *(logs[*str]);
}

Logger& Logger::sendMessage () {
  if (0 < indent) emit message(QString(indent, ' ') + buffer);
  else emit message(buffer);
  buffer.clear();
  return *this;
}
