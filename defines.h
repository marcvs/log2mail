/* defines.h für logwatchd 

   $Id: defines.h,v 1.8 2001/02/09 15:13:53 pape Exp $

*/

/* Zeit, die select wartet [sekunden] */
/* #define SELECT_TIME 1  */


/* Zeit, nach der eine Nachricht verschickt wird */
#define L2M_SENDTIME 30

/* message sender address */
#define L2M_FROMADDR "log2mail"

/* path and options to sendmail */
#define L2M_SENDMAIL "/usr/lib/sendmail -oi -t"

/* default message template */
#define L2M_TEMPLATE "/etc/log2mail/mail"

/* default config file (or dir) */
#define L2M_CONFIGFILE "/etc/log2mail/config"

/* maximale Anzahl an Zeilen, die mit einer
   Nachricht verschickt werden */
#define L2M_MAXLINES 3

#define L2M_RESENDTIME 100

/* internal: maximal string length for some config parameters */
#define L2M_MAX_STR 120

/* internal: maximum size of message file */
#define L2M_MSG_SIZE 10000

/* maximum number of logfiles open */
#define L2M_LF_SIZE 100

/* there are different options for debugging output */

// #define DEBUG          /* for normal debug output */
/* do not use the DEBUG option above anymore: call "make DEBUG=yes" instead */
// #define DEBUG_SENDMAIL /* for debugging the sendmail call */
// #define DEBUG_REGEX    /* for debugging the regular expression evaluation */
