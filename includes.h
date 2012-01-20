/* log2mail
   
   file: includes.h

   mk-log2mail@krax.net

   GPL

   $Id: includes.h,v 1.12 2001/07/16 11:14:55 pape Exp $

 */

#ifndef _LOGWATCHD_INCLUDES_H__

#define _LOGWATCHD_INCLUDES_H__

// #include <String.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <list>
#include "defines.h"
#include <errno.h>
#include <stdio.h>
#include <map>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

using std::list;
using std::map;

class inDaemon;
extern class inDaemon *thisDaemon;

/* main.cc */
int printlog(int priority, char *format, ...);

/* needed for map to sort char* */
struct ltstr
{
  bool operator()(const char* s1, const char* s2) const
  {
    return strcmp(s1, s2) < 0;
  }
};

/* to reuse a template file (read once, use often) */
class inTemplate {
 private:
  char *filename;
  char *contents;

 public:
  inTemplate() { filename = NULL; contents = NULL; }
  inTemplate(char *afilename) {
    filename = NULL; contents = NULL; 
    setFilename(afilename);
  }

  ~inTemplate() { if (filename) free(filename); if (contents) free(contents); }

  void setFilename(char *afilename) {
    if (filename) free(filename);
    filename = (char*) malloc(strlen(afilename)+1);
    if (filename) {
      strncpy(filename, afilename, strlen(afilename)+1);
    }
  }

  int readContents();

  char *getContents() {
    if (!contents) readContents();
    return (contents);
  }
};

/* lowest level of configuration params */

/* one param: value */
class inParam {
 public:
  inParam() { ; }
  virtual ~inParam() { ; }
  
  virtual int getValueAsInt() = 0;
  virtual char* getValueAsChar() = 0;

  /* returns 0 on success */
  virtual int setValue(int) = 0;
  virtual int setValue(char*) = 0;
};

class inPChar : public inParam {
 private:
  char *value;

 public:
  inPChar() { value = NULL; }
  inPChar(char *avalue) { value = NULL; setValue(avalue); }
  virtual ~inPChar() { if (value) free(value); }

  virtual char *getValueAsChar() { return value; }
  virtual int getValueAsInt() { 
    printlog(LOG_ERR, "inPChar::getValueAsInt() virtual method called\n");
    exit(1);
  }
  virtual int setValue(char *avalue) {
    if (value) free(value);
    value = strdup(avalue);
    return (value == NULL);
  }
  virtual int setValue(int /* i */) { return 1; }
};

class inPInt : public inParam {
 private:
  int value;

 public:
  inPInt() { value = -1; }
  inPInt(char *avalue) { setValue(avalue); }
  inPInt(int avalue) { setValue(avalue); }

  virtual int getValueAsInt() { return value; }
  virtual char* getValueAsChar() { return NULL; }
  virtual int setValue(int avalue) {
    value = avalue;
    return 0;
  }
  virtual int setValue(char *avalue) {
    if (avalue) {
      char *endptr;
      int i = strtol(avalue, &endptr, 10);
      if (*endptr != '\0') { 
	printlog(LOG_ERR, 
		 "inPInt::setValue: Number conversion error (%s): %m\n",
		 avalue);
	return 1;
      } else {
	value = i;
	return 0;
      }
    } else return 2;
  }
};

/* map of inParam name -> type and value */ 
class inParams {
 private:
  inParams *parent;
  map<const char*, inParam*, ltstr> params;

 public:
  inParams() { ; }
  inParams(inParams *aparent) { parent = aparent; }
  ~inParams() { params.erase(params.begin(), params.end()); }

  /* returns 0 on success */
  int setParam(const char *aparam, inParam *avalue) {
    char *param;
    param = strdup(aparam);
    if ( (param) && (avalue) ) {
      params[param] = avalue;
      return 0;
    } else 
      return 1;
  }

  inParam* findParam(const char *aparam) {
    inParam* res = params[aparam];
    return ( (res)?res : ( (parent)?(parent->findParam(aparam)):NULL ) );
  }
  
  char *getParamValue(const char *aparam) {
    inParam* p = findParam(aparam);
    return ( (p)?p->getValueAsChar():NULL );
  }

  int getParamValueAsInt(const char *aparam) {
    inParam* p = findParam(aparam);
    return ( (p)?p->getValueAsInt(): (-1) );
  }
};

/* common options of all config levels */
class inCfCommon {
 private:
  inParams params;

 public:
  inCfCommon() { ; }
  inCfCommon(inParams* parent) : params(parent) { ; }

  inParams *getParams() { return &params; }
};

class inPattern;

/* aus data.cc */

class inMailTo : public inCfCommon {
 private:
  char *mailto;

 public:
  inPattern *pattern;

  inMailTo() { mailto = NULL; }
  inMailTo(char *amailto, inPattern *apattern, inParams* parent)
    : inCfCommon(parent) { 
    mailto = NULL;
    setMailto(amailto); 
    pattern = apattern;
  }
  ~inMailTo() { if (mailto) free(mailto); }

  int setMailto(char *amailto) {
    if (mailto) free(mailto);
    mailto = (char*)malloc(strlen(amailto)+1);
    if (mailto) {
      strncpy(mailto, amailto, strlen(amailto)+1);
      return 0;
    }
    return errno;
  }

  char *getMailto() {
    return mailto;
  }

  char *getPattern();
  char *getLogfile();
};

class inLogfile;

class inPattern : public inCfCommon {
 private:
  char *pattern;
  regex_t rPattern;
  inParams params;
  
 public:
  list<inMailTo*> mailto;
  inLogfile *logfile;

  inPattern() { pattern = NULL; }
  inPattern(char *aPattern, inLogfile *alogfile, inParams *parent) 
    : inCfCommon(parent) {
    pattern = NULL; 
    logfile = alogfile;
    setPattern(aPattern);
  }

  ~inPattern() { regfree(&rPattern); if (pattern) free(pattern); }

  void addMailto(inMailTo *amailto) {
    mailto.insert(mailto.end(), amailto);
  }

  inMailTo *addMailto(char *mailaddr) {
    inMailTo *mt = new inMailTo(mailaddr, this, getParams());
    addMailto(mt);
    return mt;
  }

  char *getPattern() { return pattern; }
  int setPattern(char *aPattern) { 
    if (pattern) free(pattern);
    pattern = (char*)malloc(strlen(aPattern)+1);
    if (pattern) {
      strncpy(pattern, aPattern, strlen(aPattern)+1);
      return (regcomp (&rPattern, pattern, 0));
    } else {
      return errno;
    }
  }

  /*  int sendMessage() {
    list<inMailTo*>::iterator mt1 = mailto.begin();
    list<inMailTo*>::iterator mt2 = mailto.end();
    while (mt1 != mt2) {
      (*mt1)->sendMessage();
      mt1++;
    }
    } */

  // prüft, ob Pattern paßt: Rückgabe 1, wenn wahr
  // validates pattern: returns 1 if match
  int matches(const std::string& aLine);

  char *getLogfile(); 
};

class inMsg {
 public:
  inMailTo *mailto;
  list<std::string> zeilen;
  int zaehler; // in sekunden
  int lines;
 private:
  int _done;

 public:
  inMsg() { zaehler = 0; _done = 0; }
  inMsg(inMailTo *amailto, const std::string& zeile) {
    mailto = amailto; zaehler = 0; 
    _done = 0; lines = 0;
    addZeile(zeile);
  }

  void update() { zaehler++; }

  void done() { _done=1; zaehler = 0; lines = 0; }
  int isDone() { return _done; }
  void addZeile(const std::string& zeile);

  int isReady() {
    if (!lines) return 0;
    if (!_done) return (zaehler > mailto->getParams()->getParamValueAsInt("sendtime"));
    return (zaehler > mailto->getParams()->getParamValueAsInt("resendtime"));
  }

  /* build a Message */
  std::string build();

  /* sends a message, returns 0 on success */
  int sendMessage();
};

class inLogfile : public inCfCommon {
 private:
  char *filename;
  list<inPattern*> pattern;
  struct stat lfStat;

 public:
  int new_data_appeared;

  inLogfile() { filename = NULL; }
  inLogfile(char *aFilename, inParams *parents) : inCfCommon(parents) {
    filename = NULL;
    setFilename(aFilename); 
    new_data_appeared = 0;
  }
  
  ~inLogfile() { 
    if (filename) free(filename); 
    pattern.erase(pattern.begin(), pattern.end());
  }
  
  // readPatternFromStream
  
  char *getFilename() { return filename; }
  int setFilename(char *aFilename) { 
    if (filename) free(filename);
    filename = (char*)malloc(strlen(aFilename)+1);
    if (filename) {
      strncpy(filename, aFilename, strlen(aFilename)+1);
      return 0;
    }
    return errno;
  }

  /* this is for recognizing if a file is rotated or similar
     executed once at initial open */
  int readStat(int fd) {
    new_data_appeared = 0;
    return fstat(fd, &lfStat);
  }

  /* checks if the logfile is rotated.
     returns 1 if so, 0 if not */
  int checkRotate() {
    struct stat sbuf;
    
    /* most simply check: are inode and device the same? */
    if (-1 == stat(getFilename(), &sbuf)) {
      printlog(LOG_ERR, "stat failed for %s: %m\n", getFilename());
      return -1;
    }

    if (( lfStat.st_dev != sbuf.st_dev ) 
	|| ( lfStat.st_ino != sbuf.st_ino )) {
      /* inode or device changed. file has rotated */
      return 1;
    }

    /* check the size */
    if (sbuf.st_size < lfStat.st_size) {
      /* file is smaller */
      return 1;
    }

    /* no new data but modification time changed */
    /* use cached filesize to check whether there is new data,
     * to avoid a race condition */
    if ( (sbuf.st_size == lfStat.st_size) && (sbuf.st_mtime > lfStat.st_mtime) ) {
      /* file is newer */
      return 1;
    }

    /* save last stat -> file size and mod time recognition */
    lfStat = sbuf;
    new_data_appeared = 0;

    return 0;
  }
  
  void addPattern(inPattern* aPattern) { pattern.insert(pattern.end(), 
							aPattern); }
  inPattern *addPattern(char *pat) {
    inPattern *res = new inPattern(pat, this, getParams());
    addPattern(res);
    return (res);
  }

  void checkPatternIn(list<inMsg*>&, const std::string&); 
};

/* aus config.cc */

void init_regex();
void free_regex();

class inConfigFile : public inCfCommon {
 private:
  char filename[L2M_MAX_STR];
 public:
  list<inLogfile*> logfiles;

 public:
  inConfigFile() { ; /* do nothing */ }
  inConfigFile(inParams *parent) : inCfCommon(parent) { ; }
  inConfigFile(char *aFilename, inParams *parent) : inCfCommon(parent) {
    setFilename(aFilename); 
  }

  ~inConfigFile() { logfiles.erase(logfiles.begin(), logfiles.end()); }

  /* is called for each include 
     returns 0 on success */
  int parseAFile(char *afilename,
		 int& defaults_done,
		 inLogfile **curLf,
		 inParams **curParams,
		 inPattern **curPattern,
		 inMailTo **curMailto);

  /* returns 0 on success */
  int parseConfig();
  
  /* returns 0 on success */
  int setFilename(char *aFilename) { 
    if (strlen(aFilename) >= L2M_MAX_STR) {
      printlog(LOG_ERR, "Config filename %s is too long (max. L2M_MAX_STR = %d).\n", 
	       aFilename, L2M_MAX_STR);
      return -1;
    }
    strncpy(filename, aFilename, L2M_MAX_STR);
    return parseConfig(); 
  }

  void addLogfile(inLogfile* logfile) { logfiles.insert(logfiles.end(), 
						       logfile); }
  inLogfile* addLogfile(char *logfile) {
    inLogfile* t = new inLogfile(logfile, getParams());
    addLogfile(t);
    return t;
  }
};

class inConfig : public inCfCommon {
 private:
  /* msg template */
  map<const char*,inTemplate*,ltstr> templates;

  char filename[L2M_MAX_STR];

 public:
  list<inConfigFile*> configfiles;

  inConfig() { ; }
  inConfig(char *aFilename) : inCfCommon(NULL) { setFilename(aFilename); }

  inTemplate *getTemplate(char *f) {
    if (!templates[f]) {
      templates[f] = new inTemplate(f);
    }
    return templates[f];
  }

 protected:
  void initGl();
  
  int doConfigFile(char *afilename);
  
  /* returns 0 on success */
  int parseConfig(char *);

 public:
  /* returns 0 on success */
  int setFilename(char *aFilename) { 
    if (strlen(aFilename) >= L2M_MAX_STR) {
      printlog(LOG_ERR, "Config filename %s is too long (max. L2M_MAX_STR = %d).\n", 
	       aFilename, L2M_MAX_STR);
      return -1;
    }
    strncpy(filename, aFilename, L2M_MAX_STR);
    return parseConfig(filename); 
  }

  void free() {
    configfiles.erase(configfiles.begin(), configfiles.end());
  };

  int reread() {
    free();
    return parseConfig(filename);
  }

};

/* aus select.cc */
class inDaemon {
 private:
  int files[L2M_LF_SIZE];
  inLogfile* lf[L2M_LF_SIZE];

 public:
  inConfig konfig;
  list<inMsg*> msgQueue;

  int bRecursive;
  int bLogEveryMail;
  int bVerbose;

  /* signal flags */
  int bSigTerm, bSigHUP;

 public:
  inDaemon() { thisDaemon = this; init(); }

  void init() {
    bRecursive = 0;
    bLogEveryMail = 0;
    bVerbose = 0;
    bSigTerm = bSigHUP = 0;
  }

  /* returns 0 on success */
  int setConfigFile(char *file) {
    return konfig.setFilename(file);
  }

  int run();

  /* opens a file, returns 0 on success */
  int open(inLogfile*, int *fd);

  /* sends messages and terminates */
  void signalTERM() {
    bSigTerm++;
  }

  /* rereads configuration files */
  void signalHUP() {
    bSigHUP++;
  }

  /* sends messages */
  void flush();

  int openFiles();
  void closeFiles(int);
};



#endif // _LOGWATCHD_INCLUDES_H__
















