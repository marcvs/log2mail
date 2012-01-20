/* config.cc für log2mail

   GPL

   $Id: config.cc,v 1.7 2001/02/02 17:02:22 krax Exp $

   Michael Krax (michael.krax@innominate.com)

*/



/* we USE GNU because of the getline function */
//#define _GNU_SOURCE

#include <stdio.h>
#include "includes.h"
#include <stdlib.h>
#include <regex.h>
#include <syslog.h>
#include <strings.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

/* valid options in config files */
char *validCharOptions[] = { "fromaddr",
			     "sendmail",
			     "template",
			     NULL
};
char *validIntOptions[] = { "sendtime",
			    "resendtime",
			    "maxlines",
			    NULL
};


/* Reguläre Ausdrucke vorab berechnen */
// FIXME: translate

regex_t rC, rF, rP;

void init_regex() {
  /* matches a comment or the empty line */
  if (0 != regcomp(&rC, "^(#.*|[[:space:]]*)$", REG_EXTENDED)) { 
    printlog(LOG_ERR, "init_regex: rC could not be initialisied\n"); 
  }
  /* matches "defaults" */
  if (0 != regcomp(&rF, "^[[:space:]]*defaults[[:space:]]*$", REG_EXTENDED)) {
    printlog(LOG_ERR, "init_regex: rF could not be initialisied\n"); 
  }
  /* matches a normal line */
  if (0 != regcomp(&rP, "^.*=.*$", 0)) { 
    printlog(LOG_ERR, "init_regex: rP could not be initialisied\n"); 
  }
}

void free_regex() {
  regfree(&rC);
  regfree(&rF);
  regfree(&rP);
}

/* set default values */
void inConfig::initGl() {
  getParams()->setParam("fromaddr", new inPChar(L2M_FROMADDR));
  getParams()->setParam("sendmail", new inPChar(L2M_SENDMAIL));
  getParams()->setParam("template", new inPChar(L2M_TEMPLATE));
  getParams()->setParam("sendtime", new inPInt(L2M_SENDTIME));
  getParams()->setParam("resendtime", new inPInt(L2M_RESENDTIME));
  getParams()->setParam("maxlines", new inPInt(L2M_MAXLINES));
} 

/* int inConfig::setGlParam(int& i, char *val) {
  char *endptr;
  i = strtol(val, &endptr, 10);
  return (*endptr != '\0');
}

int inConfig::setGlParam(char *s, char *val) {
  strncpy(s, val, L2M_MAX_STR);
  return 0;
  } */

int regexec_error(const   regex_t  *preg,  const  char  *string) {
  int ret_val = regexec(preg, string, 0, 0, 0);

#ifdef DEBUG_REGEX
  if (ret_val != 0) {
    char errbuf[250];
    regerror(ret_val, preg, errbuf, sizeof(errbuf));
    printlog(LOG_DEBUG, "regerror: %s while matching \"%s\"\n", errbuf, string);
  }
#endif

  return ret_val;
}

int inConfig::doConfigFile(char *afilename) {
  inConfigFile *icf = new inConfigFile(getParams());
  if (icf->setFilename(afilename)) return 1;
  configfiles.insert(configfiles.end(), icf);
  return 0;
}

int inConfig::parseConfig(char *afilename) {
  /* keep in mind if we are in recursion */
  static int second_level = 0;

  /* load defaults */
  initGl();

  /* check if config file is a directory */
  struct stat stbuf;
  if (stat(afilename, &stbuf) != 0) {
    printlog(LOG_ERR, "inConfig::parseConfig: Couldn't stat '%s': %m\n", afilename);
    return 1;
  }

  if (S_ISDIR(stbuf.st_mode)) {
    /* if !-R and 2. level dir -> ignore the subdir */
    if ( (second_level) && (!thisDaemon->bRecursive) ) {
      /* this is not an error */
      return 0;
    }

    /* read all files in directory */
    DIR *dir = opendir(afilename);
    if (!dir) {
      printlog(LOG_ERR, "inConfig::parseConfig: Couldn't open directory '%s': %m\n",
	       afilename);
      return 1;
    }

    struct dirent *de;

    while (NULL != (de = readdir(dir))) {
      char fullpath[L2M_MAX_STR];

      if ( (strpbrk(de->d_name, "#~") != NULL) || 
	   (de->d_name[0] == '.') ) {
#ifdef DEBUG
	printlog(LOG_WARNING, "inConfig::parseConfig: ignoring file '%s'\n",
		 de->d_name);
#endif
      } else {
	snprintf(fullpath, L2M_MAX_STR, "%s/%s", afilename, de->d_name);
#ifdef DEBUG
	printlog(LOG_DEBUG, "inConfig::parseConfig: parsing file '%s'\n",
		 fullpath);
#endif
	/* we call ourself */
	second_level++; 
	if (parseConfig(fullpath)) return 1;
	second_level--;
      }
    }

    closedir(dir);
  } else if (S_ISREG(stbuf.st_mode)) {
    /* read file */
    if (doConfigFile(afilename)) {
      printlog(LOG_ERR, "inConfig::parseConfig: parse error in file '%s'\n",
	       afilename);
      return 1;
    }
  }

  return 0;
}


/* keeps a pointer to the last param name found */
char *param_name = NULL;

/* checks if lhs is in optionset, then set value for newp
   return 0 on success */
int setParam(char *lhs, char *optionset[], inParam* newp, char *value) {
  char **vo = optionset;
  while (*vo) {
    if (strcasecmp(lhs, *vo) == 0) {
      param_name = *vo;
      return newp->setValue(value); 
    }
    vo++;
  }
  return 1;
}

int inConfigFile::parseAFile(char *afilename,
			     int& defaults_done,
			     inLogfile **curLf,
			     inParams **curParams,
			     inPattern **curPattern,
			     inMailTo **curMailto) 
{
  int iLine = 0;

  char eing[8192];

  FILE *cnf;

  cnf = fopen(afilename, "r");

  if (!cnf) {
    printlog(LOG_ERR, "inConfig::parseConfig: Couldn't open config file %s: %m\n",
	     afilename);
    return 1;
  }

  while (fgets(eing, sizeof(eing), cnf) != NULL) {
    iLine++;

      /* is our line a comment (or empty) */
      if (regexec_error(&rC, eing) == 0) ;
      /* default section */
      else if (regexec_error(&rF, eing) == 0) {
	if (defaults_done) {
	  printlog(LOG_WARNING, "%s: defaults already loaded (line %i)\n",
		   afilename, iLine);
	} else {
	  /* begin with defaults */
	  if (*curLf) {
	    printlog(LOG_ERR, "%s: defaults not at beginning of file\n",
		     afilename);
	    return 1;
	  }
	  defaults_done++;
#ifdef DEBUG
	  printlog(LOG_DEBUG, "starting default section of %s\n",
		   afilename);
#endif
	}
      } 
      /* normal line something = some other thing */
      else if (regexec_error(&rP, eing) == 0) {
 	char *delim = index(eing, '=');
	if (!delim) { printlog(LOG_CRIT, "inConfig::parseConfig(): = not found\n"); return -3; }
	char *lhs = eing, *rhs = (delim+1);
	*delim = '\0';

	/* chomp rhs */
	while (rhs[strlen(rhs)-1] == '\n') { rhs[strlen(rhs)-1] = '\0'; }
	/* remove trailing space */
	while ( (rhs[strlen(rhs)-1] == ' ') || (rhs[strlen(rhs)-1] == '\t') )
	  { rhs[strlen(rhs)-1] = '\0'; }
	while ( (lhs[strlen(lhs)-1] == ' ') || (lhs[strlen(lhs)-1] == '\t') )
	  { lhs[strlen(lhs)-1] = '\0'; }
	/* remove leading space */
	while ( (rhs[0] == ' ') || (rhs[0] == '\t') ) { rhs++; }
	while ( (lhs[0] == ' ') || (lhs[0] == '\t') ) { lhs++; }
	
	/* remove "" */
	if ( (lhs[0] == '"') && (lhs[strlen(lhs)-1] == '"') ) {
	  lhs++; lhs[strlen(lhs)-1] = '\0';
	}
	if ( (rhs[0] == '"') && (rhs[strlen(rhs)-1] == '"') ) {
	  rhs++; rhs[strlen(rhs)-1] = '\0';
	}
	
	if (strcasecmp(lhs, "include") == 0) {
	  /* include a file */
	  /* parse it exactly as if the options were written in the
	     file from which we include */
	  if (parseAFile(rhs, defaults_done, curLf, 
			 curParams, curPattern, curMailto)) {
	    printlog(LOG_ERR, "Error parsing file '%s' included from '%s'\n",
		     rhs, afilename);
	    return 1;
	  }
	} else if (strcasecmp(lhs, "file") == 0) {
	  /* define a new file */
	  *curLf = addLogfile(rhs);
	  *curParams = (*curLf)->getParams();
	  *curPattern = NULL;
	  *curMailto = NULL;
	} else if (strcasecmp(lhs, "pattern") == 0) {
	  /* define a new pattern */
	  if (*curLf) {
	    *curPattern = (*curLf)->addPattern(rhs);
	    *curParams = (*curPattern)->getParams();
	    *curMailto = NULL;
	  } else {
	    printlog(LOG_ERR, "%s: pattern without a logfile (line %i)\n",
		     afilename, iLine);
	    return 1;
	  }
	} else if (strcasecmp(lhs, "mailto") == 0) {
	  /* define a new mailto address */
	  if (*curPattern) {
	    *curMailto = (*curPattern)->addMailto(rhs);
	    *curParams = (*curMailto)->getParams();
	  } else {
	    printlog(LOG_ERR, "%s: mailto without a pattern (line %i)\n",
		     afilename, iLine);
	    return 1;
	  }
	} else {
	  if (! (*curParams)) {
	    printlog(LOG_ERR, "%s: no valid params record found (line %i)\n",
		     afilename, iLine);
	    return 1;
	  } else {
	    inParam *param = new inPChar();
	    if (setParam(lhs, validCharOptions, param, rhs)) {
	      delete param;
	      param = new inPInt(); 
	      if (setParam(lhs, validIntOptions, param, rhs)) {
		printlog(LOG_ERR, "parse error: no valid lhs expression found (%s, line %i)\n",
			 lhs, iLine);
		return 1;
	      }
	    }
	    if (param_name) {
	      /* special check if sendmail param points
		 to an executable file */
	      if (strcmp(param_name, "sendmail") == 0) {
		/* check if rhs is executable */
		char *sendmail = strdup(param->getValueAsChar());
		if (index(sendmail, ' ')) {
		  * (index(sendmail, ' ')) = 0;
		}
		int a = access(sendmail, X_OK|F_OK);
		if (a == -1) {
		  /* access denied */
		  printlog(LOG_ERR, "invalid sendmail param %s, line %i: non-executable or non-existant\n",
			   sendmail, iLine);
		  return 1;
		} else if (a) {
		  printlog(LOG_ERR, "invalid sendmail param %s, line %i: %m\n",
			   sendmail, iLine);
		  return 1;
		}
		free(sendmail);
		/* okay, we can execute sendmail */
	      }

	      (*curParams)->setParam(param_name, param);
	    } else {
	      printlog(LOG_ERR, "param_name not set (%s, line %i)\n",
		       lhs, iLine);
	      return 1;
	    }
	  }
	}
	 
#ifdef DEBUG
	printlog(LOG_DEBUG, "%s -> %s\n", lhs, rhs);
#endif
      } else {
	/* line has invalid form */
	printlog(LOG_ERR, "malformed line #%i in config file\n", iLine);
	return 1;
      }
  }

  fclose(cnf);

  return 0;
}

int inConfigFile::parseConfig() {
  int defaults_done = 0;
  inLogfile *curLf = NULL;
  inParams *curParams = getParams();
  inPattern *curPattern = NULL;
  inMailTo *curMailto = NULL;

  return parseAFile(filename, defaults_done, &curLf, 
		    &curParams, &curPattern, &curMailto);
}


int inTemplate::readContents() {
  FILE *inf;

#ifdef DEBUG
  printlog(LOG_DEBUG, "inTemplate::readContents(): Reading %s\n", filename);
#endif

  if (NULL == (inf = fopen(filename, "r"))) {
    printlog(LOG_ERR, 
	     "inTemplate::readContents(): Could not open message file %s: %m\n",
	     filename);
    return 0;
  }

  int i = 0;
  char content[L2M_MSG_SIZE+1];
  content[0] = '\0';

  while (!feof(inf)) {
    int c = fgetc(inf);
    if (c != EOF) {
      content[i] = c;
      i++; content[i] = '\0';
      if (i >= L2M_MSG_SIZE) { 
	printlog(LOG_ERR, "inConfig::readMsg(): Message file %s too big.", filename); 
	break; 
      }
    }
  }

  fclose(inf);

  contents = (char*)malloc(strlen(content)+1);

  if (contents) strncpy(contents, content, strlen(content)+1);
  return 0;
}
