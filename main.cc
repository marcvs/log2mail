/* log2mail

   This program is released under the terms of the GNU Public License.
   A copy of the license is distributed with the source code.

   mk-log2mail@krax.net
   
   May 2000: GPL

   First relase November 1999

   $Id: main.cc,v 1.13 2001/07/16 11:14:55 pape Exp $

*/


#include "includes.h"
#include <getopt.h>
#include <unistd.h>
#include <stdarg.h>
#include <syslog.h>
#include <signal.h>

void usage (char *name) {
  printf("usage: %s [-f config file] [-l] [-N] [-V] [-v] [-h|?]\n", name);
  printf(" -f specifies a config file\n");
  printf(" -N do not fork into background, log to stderr\n");
  printf(" -V show version information and exit\n");
  printf(" -l write logfile entry for every message sent\n");
  printf(" -v be verbose\n");
  printf(" -h|? display this help text and exit\n");
}

int bUseSyslog = 0;

int printlog(int priority, char *format, ...) {
  va_list ap;
  if (bUseSyslog) {
    int n, size = 100;
    char *p;
    if ((p = (char*)malloc (size)) == NULL) {
      syslog(LOG_CRIT, "printlog: out of memory: %m");
      return -1;
    }
    while (1) {
      /* Try to print in the allocated space. */
      va_start(ap, format);
      n = vsnprintf (p, size, format, ap);
      va_end(ap);
      /* If that worked, return the string. */
      if (n > -1 && n < size)
	break;
      /* Else try again with more space. */
      if (n > -1)    /* glibc 2.1 */
	size = n+1; /* precisely what is needed */
      else           /* glibc 2.0 */
	size *= 2;  /* twice the old size */
      if ((p = (char*)realloc (p, size)) == NULL) {
	syslog(LOG_CRIT, "printlog: out of memory: %m");
	return -1;
      }
    }                                                              
    syslog(priority, "%s", p);
    free(p);
  } else {
    va_start (ap, format);
    vfprintf(stderr, format, ap);
    va_end (ap);
  }
  return 0;
}	

void sigHandler (int signum) {
  switch (signum) {
  case SIGTERM:
    thisDaemon->signalTERM();
    break;
  case SIGHUP:
    thisDaemon->signalHUP();
  }
}

int main (int argc, char *argv[]) {
  inDaemon d;

  int option_char;
  char config_f[L2M_MAX_STR]; 
  int bNoFork = 0;
          
  strncpy (config_f, L2M_CONFIGFILE, L2M_MAX_STR);

  // Invokes member function `int operator ()(void);'
  while ((option_char = getopt (argc, argv, "h?NRlvVf:")) != EOF)
    switch (option_char)
      {  
      case 'v': 
	/* be verbose */
	d.bVerbose++;
	break;

      case 'V': 
	fprintf(stdout, "%s. Version 0.2.8 November 2002.\n", argv[0]);
	exit(0);

      case 'h':
      case '?': 
	usage(argv[0]);
        exit(0);

      case 'f':
	strncpy(config_f, optarg, L2M_MAX_STR);
	break;

      case 'N':
	/* Do not fork into background, log to STDERR */
	bNoFork = 1;
	break;

      case 'R':
	/* read config directory recursive */
	d.bRecursive = 1;
	break;

      case 'l':
	/* write logfile entry for every message sent */
	d.bLogEveryMail = 1;
	break;
      }
  
  if (!bNoFork) {
    bUseSyslog = 1;
    openlog("log2mail", LOG_PID|LOG_PERROR, LOG_DAEMON);
  }
  
  init_regex();

  if (d.setConfigFile(config_f)) {
    printlog(LOG_ERR, "Error reading config file. Terminating.\n");
    if (bUseSyslog) 
      closelog();
    return 1;
  }

  /* fork into background */
  if (!bNoFork) {
#ifdef DEBUG
    printlog(LOG_DEBUG, "Forking into background\n");
#endif
    int pid = fork();
    if (pid == -1) {
      printlog(LOG_ERR, "Couldn't fork into background: %m");
      exit (1);
    } else if (pid != 0) {
      /* we are the parent */
#ifdef DEBUG
      printlog(LOG_DEBUG, "Forked. Exiting.\n");
#endif
      if (bUseSyslog)
	closelog();
      exit(0);
    }
  }
  /* if forking: only the child gets here */

  if (bUseSyslog) {
    closelog();
    openlog("log2mail", LOG_PID, LOG_DAEMON);
  }

  /* install signal handler */
  if (SIG_ERR == signal(SIGTERM, sigHandler)) {
    printlog(LOG_ERR, "signal handler for SIGTERM not installed: %m");
  }
  if (SIG_ERR == signal(SIGHUP, sigHandler)) {
    printlog(LOG_ERR, "signal handler for SIGHUP not installed: %m");
  }

  /* start the real main */
  int ret_val = d.run();

  fflush(stderr);
  free_regex();
  
  if (bUseSyslog)
    closelog();

  return ret_val;
}
