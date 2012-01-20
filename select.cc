/* this file is part of log2mail

   file: select.cc

   mk-log2mail@krax.net

   GPL

   $Id: select.cc,v 1.13 2001/06/25 10:45:42 krax Exp $

   thanks to 
   Enrico Zini <zinie@cs.unibo.it>
   for his patch to use strings    September 2002

*/

#include "includes.h"
#include "defines.h"
#include <algorithm>
#include <iterator>

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

using namespace std;

class inDaemon *thisDaemon = NULL;

int inDaemon::open(inLogfile* alf, int *fd) {
  char *filename = alf->getFilename();
  *fd = ::open(filename, O_RDONLY);
  if (*fd == -1) {
    printlog(LOG_ERR, "Couldn't open file \"%s\" (%m). Check your configuration!\n",
	     filename);
    return -1;
  }
  if (-1 == alf->readStat(*fd)) {
    printlog(LOG_ERR, "stat of '%s' failed: %m\n", 
	     filename);
    return -1;
  }	
  if (bVerbose) { /* bVerbose is thisDaemon->bVerbose */
    printlog(LOG_DEBUG, "Listening for %s\n", filename);
  }
  return 0;
}

int inDaemon::openFiles() {
  /* open the files */
  list<inConfigFile*>::iterator config_first = konfig.configfiles.begin();
  list<inConfigFile*>::iterator config_last = konfig.configfiles.end();

  int i = 0;

  while (config_first != config_last) {
    list<inLogfile*>::iterator first = (*config_first)->logfiles.begin();
    list<inLogfile*>::iterator last  = (*config_first)->logfiles.end();

    while (first != last) {
      if (i >= L2M_LF_SIZE) {
	printlog(LOG_CRIT, "Too many logfiles. Change L2M_LF_SIZE and recompile.\n");
	return -1;
      }
      lf[i] = *first++;
      if (-1 == open (lf[i], &files[i])) {
	return -1;
      }
      if (-1 == lseek (files[i], 0, SEEK_END)) {
	printlog(LOG_ERR, "Couldn't go to end of file \"%s\" (%m). Check that file!\n", 
		 lf[i]->getFilename());
	return -1;
      } 
      i++;
    }

    config_first++;
  } 

  return(i);
}

void inDaemon::closeFiles(int max_fd) {
  for (int i=0;i<max_fd;i++) {
    close(files[i]);
  }
}

int inDaemon::run() {
  fd_set rfds;
  struct timeval tv;
  int retval, max_fd = 0, iLoopCounter = 0;

  max_fd = openFiles();

  /* an error occured? */
  if (max_fd == -1) return 1;

  int i;

  while (!bSigTerm) { /* leave this if signal SIGTERM */
    FD_ZERO(&rfds);
    for (i=0;i<max_fd;i++) FD_SET(files[i], &rfds); 

    tv.tv_sec = /* SELECT_TIME; */ 0;
    tv.tv_usec = 0;
    /* select verhält sich falsch?: 
       es gibt immer mit allen dateien als readable zurück 
       trsltd: select gives false response:
       it returns always with all files readable
    */
    retval = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);

    if (retval == -1) {
      // an error occured 
      printlog(LOG_ERR, "Error in select(): %m\n");
      return 1;
    }

    if (retval == 0) {
      // Keine Aktivität -> warteschlange prüfen
      // no activity -> check queue
      /* wegen des select-verhaltens sollte dieser Teil der fkt nie
	 erreicht werden: warteschlange an anderer stelle prüfen */
      /* because of a possible bug (or misconduct?) in select() (see above)
	 this part of the program is never reached; the queue is checked below */
    } else {
      // Aktivität

      for (i=0;i<max_fd;i++) {
	if (FD_ISSET(files[i], &rfds)) {
	  // cout << "Neue Daten in " << lf[i]->getFilename() << endl;
	  string eing;  /* new content in a logfile */
	  const unsigned int bufsize = 512;
	  char buf[bufsize];
	  int rdsize;

	  while ((rdsize = read(files[i], buf, bufsize)) > 0)
	    for (unsigned int t = 0; t < static_cast<size_t>(rdsize); t++)
	      if (buf[t] == '\n')
	      {
		// Have a line
		// hier hat sich wirklich was getan
		// something changed
		if (lf[i])
		{
		  lf[i]->new_data_appeared = 1;
		  lf[i]->checkPatternIn(this->msgQueue, eing);
		}
		eing = string();
	      } else
		eing += buf[t];

	  if (rdsize == -1)
	    printlog(LOG_ERR, "read failed while reading new content of logfile (inDaemon::run): %m\n");
	}
      }
    }

    /* check state of signal flags */
    if (bSigTerm) {
      if (bVerbose) {
	printlog(LOG_DEBUG, "SIGTERM received\n");
      }
      flush();
      break;
    } else if (bSigHUP) {
      /* reread config file */
      if (bVerbose) {
	printlog(LOG_DEBUG, "SIGHUP received. Rereading config.\n");
      }
      flush();
      closeFiles(max_fd);
      if (konfig.reread()) {
	printlog(LOG_ERR, "Error reading config file. Terminating.\n");
	exit(1);
      }
      max_fd = openFiles();
      if (max_fd == -1) return 1;
      bSigHUP = 0;
    } else {
      list<inMsg*>::iterator fi = msgQueue.begin();
      list<inMsg*>::iterator la = msgQueue.end();
      while (fi != la) {
	// wieder eine sekunde um
	// a second later
	(*fi)->update();
	
	if ((*fi)->isReady()) {
	  (*fi)->sendMessage();
	}
	fi++;
      }

      // eine sekunde schlafen
      sleep (1);

      /* do the check rotate stuff */
      iLoopCounter++;
      if (iLoopCounter > 60) {
	iLoopCounter = 0;

	for (int i=0;i<max_fd;i++) {
	  int j;
	  if (1 == (j = lf[i]->checkRotate())) {
	    /* rotated. reopen */
	    close(files[i]);
	    if (-1 == open(lf[i], &files[i])) {
	      return 1;
	    }
	    printlog(LOG_DEBUG, "Logfile %s rotated. Listening to new file.\n",
		     lf[i]->getFilename());
	  } else if (j != 0) {
	    /* error */
	    return 1;
	  }
	}
	
      }
    }
  }

  // exit-code für System
  return 0;
}

void inDaemon::flush() {
  list<inMsg*>::iterator fi = msgQueue.begin();
  list<inMsg*>::iterator la = msgQueue.end();
  while (fi != la) {
    (*fi)->sendMessage();
    fi++;
  }
}

int inMsg::sendMessage() {
  FILE *sendmail;
  char *cSendmail = mailto->getParams()->getParamValue("sendmail");

  string msg = build();

  if (cSendmail) {

#ifdef DEBUG_SENDMAIL
    sendmail = fopen ("/tmp/log2mail-sendmail.log", "a");
#else        
    sendmail = popen (cSendmail, "w");
#endif
    if (!sendmail) {
      printlog(LOG_ERR, "inDaemon::sendMessage: Could not run sendmail (%s): %m\n",
	       cSendmail);
      return 1;
    }
    fprintf(sendmail, "%.*s", msg.size(), msg.data());
    
#ifdef DEBUG
    printlog(LOG_DEBUG, "inDaemon::sendMessage: message sent.\n");
#endif

    if (thisDaemon->bLogEveryMail) {
      printlog(LOG_DEBUG, "[file %s] pattern '%s' matched, sent mail to %s\n",
	       mailto->getLogfile(), mailto->getPattern(), 
	       mailto->getMailto());
    }

#ifdef DEBUG_SENDMAIL
    fclose(sendmail);
#else
    if (-1 == pclose (sendmail)) {
      printlog(LOG_ERR, "inDaemon::sendMessage: sendmail failed: %m\n");
      return 2;
    }
#endif
    done();
  }

  return 0;
}
