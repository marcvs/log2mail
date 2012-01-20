/* data.cc für logwatchd 

   Michael Krax

   $Id: data.cc,v 1.6 2001/02/07 13:23:07 krax Exp $

   GPL

   mk-log2mail@krax.net

   thanks to 
   Enrico Zini <zinie@cs.unibo.it>
   for his patch to use strings   September 2002

 */

#include "includes.h"
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <strings.h>

using namespace std;

int inPattern::matches(const string& aLine) {
  return (regexec(&rPattern, aLine.c_str(), 0, 0, 0) == 0);
}

void inLogfile::checkPatternIn(list<inMsg*>& msgQ, const string& eing) {
  list<inPattern*>::iterator first = pattern.begin();
  list<inPattern*>::iterator last  = pattern.end();

  while (first != last) {
    if ((*first)->matches(eing)) {
      int done = 0;
      list<inMsg*>::iterator fi = msgQ.begin();
      list<inMsg*>::iterator la = msgQ.end();
      while (fi != la) {
	/* for every mailto one addZeile */
	if ( ((*fi)->mailto->pattern->logfile == this) && 
	     ((*fi)->mailto->pattern == *first) ) {
	  (*fi)->addZeile(eing);
#ifdef DEBUG
	  printlog(LOG_DEBUG, "inLogfile::checkPatternIn(): called addZeile with \"%.*s\"\n", eing.size(), eing.data());
#endif
	  done++;
	}
	fi++;
      }
      if (!done) {
	/* do add msgQ entry for every mailto in pattern */
	list<inMailTo*>::iterator mt1 = (*first)->mailto.begin();
	list<inMailTo*>::iterator mt2 = (*first)->mailto.end();
	while (mt1 != mt2) {
	  msgQ.insert(msgQ.end(), new inMsg((*mt1), eing));
	  mt1++;
#ifdef DEBUG
	  printlog(LOG_DEBUG, 
		   "inLogfile::checkPatternIn(): new inMsg added for \"%.*s\"\n", 
		   eing.size(), eing.data());
#endif
	}
      }
    }
    first++;
  }
}

/* substitute :					
   %f thisDaemon->glFromAdr     Sender address
   %n                           Frequency of messages
   %l                           Last lines
   %t pattern->getMailadr()     Recipient address
   %m                           pattern that was matched
   %F                           name of the logfile
   (%d                          time)
   %h                           hostname
*/

string lastTry(const string& tmpl, const string& f, const string& n,
	      const string& t, const string& m,
	      const string& F, const string& l,
	      const string& h) {
  string res;
  for (string::const_iterator i = tmpl.begin(); i != tmpl.end(); i++)
  {
    if (*i == '%' && i+1 != tmpl.end())
      switch (*++i)
      {
	 case 'f': res += f; break;
	 case 'n': res += n; break;
	 case 't': res += t; break;
	 case 'm': res += m; break;
	 case 'F': res += F; break;
	 case 'l': res += l; break;
	 case 'h': res += h; break;
	 case '%': res += "%"; break;
	 default: res += '%'; res += *i; break;
      }
    else
      res += *i;
  }

  return res;
}

string inMsg::build() {
  inMailTo *mt = mailto;
  char s1[100];
  bzero(s1, 100);
  snprintf(s1, 100, "%i", lines);

  string s;
  for (list<string>::const_iterator i = zeilen.begin(); i != zeilen.end(); i++) {
    if (i != zeilen.begin())
      s += '\n';
    s += *i;
  }

  /* flush the line buffer after sending if running in "all lines since last
   * mail" mode */
  if (mailto->getParams()->getParamValueAsInt("maxlines") == 0)
    zeilen.clear();

  /* get hostname */
  char hostname[L2M_MAX_STR];
  if (0 != gethostname(hostname, L2M_MAX_STR)) {
    printlog(LOG_ERR, "nMsg::build: Couldn't determine host name: %m\n");
    return NULL;
  }

#ifdef DEBUG
  printlog(LOG_DEBUG, "inMsg::build: Preparing message to %s\n", mt->getMailto());
#endif 

  return lastTry(thisDaemon->konfig.getTemplate(mt->getParams()->getParamValue("template"))->getContents(),
		       mt->getParams()->getParamValue("fromaddr"),
		       s1,
		       mt->getMailto(),
		       mt->getPattern(),
		       mt->getLogfile(),
		       s.c_str(),
		       hostname);
}

void inMsg::addZeile(const string& zeile) {
  zeilen.insert(zeilen.end(), zeile);
  lines++;
  int maxlines = mailto->getParams()->getParamValueAsInt("maxlines");
  if (maxlines > 0 && zeilen.size() > static_cast<size_t>(maxlines)) {
    zeilen.erase(zeilen.begin());
  }
  if (lines == 1) zaehler = 0;
}


char *inMailTo::getPattern() { return pattern->getPattern(); }
char *inMailTo::getLogfile() { return pattern->getLogfile(); }

char *inPattern::getLogfile() { return logfile->getFilename(); }
