/*
 * Copyright (c) 1988, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * From: @(#)commands.c	5.5 (Berkeley) 3/22/91
 */
char cmd_rcsid[] = 
  "$Id: commands.cpp,v 1.1.1.1 1997/08/13 05:39:36 chaffee Exp $";

#ifdef __WIN32__
#include <windows.h>
#include <winsock.h>
#endif

#include <string.h>

#ifndef __WIN32__
#include <sys/param.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#ifdef	CRAY
#include <fcntl.h>
#endif	/* CRAY */

#include <sys/wait.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pwd.h>

#endif

#include <signal.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/telnet.h>

#include "ring.h"

#include "externs.h"
#include "defines.h"
#include "types.h"
#include "genget.h"
#include "environ.h"
#include "proto.h"
#include "ptrarray.h"
#include "netlink.h"

#ifdef __linux__
#define HAS_IPPROTO_IP
#endif

#ifdef IPPROTO_IP
#define HAS_IPPROTO_IP
#endif

#ifndef CRAY
#if (defined(vax) || defined(tahoe) || defined(hp300)) && !defined(ultrix)
#include <machine/endian.h>
#endif /* vax */
#endif /* CRAY */

#define HELPINDENT (sizeof ("connect"))

#ifndef       MAXHOSTNAMELEN
#define       MAXHOSTNAMELEN 64
#endif        MAXHOSTNAMELEN

#if	defined(HAS_IPPROTO_IP) && defined(IP_TOS)
int tos = -1;
#endif	/* defined(HAS_IPPROTO_IP) && defined(IP_TOS) */

static unsigned long sourceroute(char *arg, char **cpp, int *lenp);


char	*hostname;
static char _hostname[MAXHOSTNAMELEN];

//typedef int (*intrtn_t)(int argc, const char *argv[]);

class command_entry;
typedef ptrarray<command_entry> command_table;

static int process_command(command_table *tab, int argc, const char **argv);


class command_entry {
 protected:
    const char *name;    /* command name */
    const char *help;    /* help string (NULL for no help) */

    int nargs;
    union {             /* routine which executes command */
	command_table *subhandler;
	int (*handlern)(int, const char **);
	int (*handler0)(void);
	int (*handler1)(const char *);
	int (*handler2)(const char *, const char *);
    };
 public:
    command_entry(const char *n, const char *e, 
		  int (*h)(int, const char **)) 
    { 
	name = n; 
	help = e;
	nargs = -1; handlern = h; 
    }
    command_entry(const char *n, const char *e,
		  int (*h)(void)) 
    { 
	name = n; 
	help = e;
	nargs = 0; handler0 = h; 
    }
    command_entry(const char *n, const char *e,
		  int (*h)(const char *)) 
    { 
	name = n; 
	help = e;
	nargs = 1; handler1 = h; 
    }
    command_entry(const char *n, const char *e,
		  int (*h)(const char *, const char *)) 
    { 
	name = n; 
	help = e;
	nargs = 2; handler2 = h; 
    }
    command_entry(const char *n, const char *e, command_table *sub) {
	name = n;
	help = e;
	nargs = -2;
	subhandler = sub;
    }
    
    int call(int argc, const char *argv[]) {
	assert(argc>=1);
	if (nargs>=0 && argc!=nargs+1) {
	    fprintf(stderr, "Wrong number of arguments for command.\n");
	    fprintf(stderr, "Try %s ? for help\n", argv[0]);
	    return 0;    /* is this right? */
	}
	if (nargs==-2) {
	    if (argc<2) {
		fprintf(stderr, "`%s' requires a subcommand.\n", argv[0]);
		fprintf(stderr, "Try %s ? for help\n", argv[0]);
		return 0;    /* is this right? */
	    }
	    return process_command(subhandler, argc-1, argv+1);
	}
	else if (nargs==-1) return handlern(argc, argv);
	else if (nargs==0) return handler0();
	else if (nargs==1) return handler1(argv[1]);
	else if (nargs==2) return handler2(argv[1], argv[2]);
	return 0;
    }

    void describe() {
	if (help) {
	    printf("%-*s\t%s\n", HELPINDENT, name, help);
	    fflush(stdout);
	}
    }
    void gethelp() {
	if (help) printf("%s\n", help);
	else printf("No help available\n");
	fflush(stdout);
    }

    const char *getname() const { return name; }
};

static char line[256];
static char saveline[256];
static int margc;
static const char *margv[20];

static void makeargv(void) {
    register char *cp, *cp2, c;
    register const char **argp = margv;

    margc = 0;
    cp = line;
    if (*cp == '!') {		/* Special case shell escape */
	strcpy(saveline, line);	/* save for shell command */
	*argp++ = "!";		/* No room in string to get this */
	margc++;
	cp++;
    }
    while ((c = *cp)!=0) {
	register int inquote = 0;
	while (isspace(c))
	    c = *++cp;
	if (c == '\0')
	    break;
	*argp++ = cp;
	margc += 1;
	for (cp2 = cp; c != '\0'; c = *++cp) {
	    if (inquote) {
		if (c == inquote) {
		    inquote = 0;
		    continue;
		}
	    } else {
		if (c == '\\') {
		    if ((c = *++cp) == '\0')
			break;
		} else if (c == '"') {
		    inquote = '"';
		    continue;
		} else if (c == '\'') {
		    inquote = '\'';
		    continue;
		} else if (isspace(c))
		    break;
	    }
	    *cp2++ = c;
	}
	*cp2 = '\0';
	if (c == '\0')
	    break;
	cp++;
    }
    *argp++ = 0;
}

/*
 * Make a character string into a number.
 *
 * Todo:  1.  Could take random integers (12, 0x12, 012, 0b1).
 */

static int special(const char *s) {
    char c;
    char b;
    
    switch (*s) {
     case '^':
	 b = *++s;
	 if (b == '?') {
	     c = b | 0x40;		/* DEL */
	 } 
	 else {
	     c = b & 0x1f;
	 }
	 break;
     default:
	 c = *s;
	 break;
    }
    return c;
}

/*
 * Construct a control character sequence
 * for a special character.
 */
static const char *control(cc_t c)
{
	static char buf[5];
	/*
	 * The only way I could get the Sun 3.5 compiler
	 * to shut up about
	 *	if ((unsigned int)c >= 0x80)
	 * was to assign "c" to an unsigned int variable...
	 * Arggg....
	 */
	register unsigned int uic = (unsigned int)c;

	if (uic == 0x7f)
		return ("^?");
	if (c == (cc_t)_POSIX_VDISABLE) {
		return "off";
	}
	if (uic >= 0x80) {
		buf[0] = '\\';
		buf[1] = ((c>>6)&07) + '0';
		buf[2] = ((c>>3)&07) + '0';
		buf[3] = (c&07) + '0';
		buf[4] = 0;
	} else if (uic >= 0x20) {
		buf[0] = c;
		buf[1] = 0;
	} else {
		buf[0] = '^';
		buf[1] = '@'+c;
		buf[2] = 0;
	}
	return (buf);
}



/*
 *	The following are data structures and routines for
 *	the "send" command.
 *
 */
 
struct sendlist {
    const char *name;	/* How user refers to it (case independent) */
    const char *help;	/* Help information (0 ==> no help) */
    int needconnect;		/* Need to be connected */
    int narg;			/* Number of arguments */
    int (*handler)(const char *, const char *); 
                                /* Routine to perform (for special ops) */
    int nbyte;			/* Number of bytes to send this command */
    int what;			/* Character to be sent (<0 ==> special) */
};

static int send_esc(const char *, const char *);
static int send_help(const char *, const char *);
static int send_docmd(const char *, const char *);
static int send_dontcmd(const char *, const char *);
static int send_willcmd(const char *, const char *);
static int send_wontcmd(const char *, const char *);

static int dosynch1(const char *, const char *) { return dosynch(); }

static struct sendlist Sendlist[] = {
    { "ao",	"Send Telnet Abort output",		1, 0, 0, 2, AO },
    { "ayt",	"Send Telnet 'Are You There'",		1, 0, 0, 2, AYT },
    { "brk",	"Send Telnet Break",			1, 0, 0, 2, BREAK },
    { "break",	0,					1, 0, 0, 2, BREAK },
    { "ec",	"Send Telnet Erase Character",		1, 0, 0, 2, EC },
    { "el",	"Send Telnet Erase Line",		1, 0, 0, 2, EL },
    { "escape",	"Send current escape character",	1, 0, send_esc, 1, 0 },
    { "ga",	"Send Telnet 'Go Ahead' sequence",	1, 0, 0, 2, GA },
    { "ip",	"Send Telnet Interrupt Process",	1, 0, 0, 2, IP },
    { "intp",	0,					1, 0, 0, 2, IP },
    { "interrupt", 0,					1, 0, 0, 2, IP },
    { "intr",	0,					1, 0, 0, 2, IP },
    { "nop",	"Send Telnet 'No operation'",		1, 0, 0, 2, NOP },
    { "eor",	"Send Telnet 'End of Record'",		1, 0, 0, 2, EOR },
    { "abort",	"Send Telnet 'Abort Process'",		1, 0, 0, 2, ABORT },
    { "susp",	"Send Telnet 'Suspend Process'",	1, 0, 0, 2, SUSP },
    { "eof",	"Send Telnet End of File Character",	1, 0, 0, 2, xEOF },
    { "synch",	"Perform Telnet 'Synch operation'",	1, 0, dosynch1, 2, 0 },
    { "getstatus", "Send request for STATUS",	1, 0, get_status, 6, 0 },
    { "?",	"Display send options",		0, 0, send_help, 0, 0 },
    { "help",	0,				0, 0, send_help, 0, 0 },
    { "do",	0,				0, 1, send_docmd, 3, 0 },
    { "dont",	0,				0, 1, send_dontcmd, 3, 0 },
    { "will",	0,				0, 1, send_willcmd, 3, 0 },
    { "wont",	0,				0, 1, send_wontcmd, 3, 0 },
    { 0 }
};

#define	GETSEND(name) ((struct sendlist *) genget(name, (char **) Sendlist, \
				sizeof(struct sendlist)))

static int sendcmd(int argc, const char *argv[]) {
    int count;		/* how many bytes we are going to need to send */
    int i;
/*    int question = 0;*/	/* was at least one argument a question */
    struct sendlist *s;	/* pointer to current command */
    int success = 0;
    int needconnect = 0;

    if (argc < 2) {
	printf("need at least one argument for 'send' command\n");
	printf("'send ?' for help\n");
	fflush(stdout);
	return 0;
    }
    /*
     * First, validate all the send arguments.
     * In addition, we see how much space we are going to need, and
     * whether or not we will be doing a "SYNCH" operation (which
     * flushes the network queue).
     */
    count = 0;
    for (i = 1; i < argc; i++) {
	s = GETSEND(argv[i]);
	if (s == 0) {
	    printf("Unknown send argument '%s'\n'send ?' for help.\n",
			argv[i]);
	    fflush(stdout);
	    return 0;
	} 
	else if (s == AMBIGUOUS) {
	    printf("Ambiguous send argument '%s'\n'send ?' for help.\n",
			argv[i]);
	    fflush(stdout);
	    return 0;
	}
	if (i + s->narg >= argc) {
	    fprintf(stderr,
	    "Need %d argument%s to 'send %s' command.  'send %s ?' for help.\n",
		s->narg, s->narg == 1 ? "" : "s", s->name, s->name);
	    return 0;
	}
	count += s->nbyte;
	if (s->handler == send_help) {
	    send_help(NULL, NULL);
	    return 0;
	}

	i += s->narg;
	needconnect += s->needconnect;
    }
    if (!connected && needconnect) {
	printf("?Need to be connected first.\n");
	printf("'send ?' for help\n");
	fflush(stdout);
	return 0;
    }
    /* Now, do we have enough room? */
    if (netoring.empty_count() < count) {
	printf("There is not enough room in the buffer TO the network\n");
	printf("to process your request.  Nothing will be done.\n");
	printf("('send synch' will throw away most data in the network\n");
	printf("buffer, if this might help.)\n");
	fflush(stdout);
	return 0;
    }
    /* OK, they are all OK, now go through again and actually send */
    count = 0;
    for (i = 1; i < argc; i++) {
	if ((s = GETSEND(argv[i])) == 0) {
	    fprintf(stderr, "Telnet 'send' error - argument disappeared!\n");
	    quit();
	    /*NOTREACHED*/
	}
	if (s->handler) {
	    count++;
	    success += (*s->handler)((s->narg > 0) ? argv[i+1] : 0,
				     (s->narg > 1) ? argv[i+2] : 0);
	    i += s->narg;
	} 
	else {
	    NET2ADD((const char) IAC, s->what);
	    printoption("SENT", IAC, s->what);
	    fflush(stdout);
	}
    }
    return (count == success);
}

static int send_esc(const char *, const char *) {
    NETADD(escapechar);
    return 1;
}

static int send_docmd(const char *name, const char *) {
    return send_tncmd(send_do, "do", name);
}

static int send_dontcmd(const char *name, const char *) {
    return(send_tncmd(send_dont, "dont", name));
}

static int send_willcmd(const char *name, const char *) {
    return(send_tncmd(send_will, "will", name));
}

static int send_wontcmd(const char *name, const char *) {
    return(send_tncmd(send_wont, "wont", name));
}

int send_tncmd(int (*func)(int, int), const char *cmd, const char *name) {
    char **cpp;
    extern char *telopts[];

    if (isprefix(name, "help") || isprefix(name, "?")) {
	register int col, len;

	printf("Usage: send %s <option>\n", cmd);
	printf("Valid options are:\n\t");

	col = 8;
	for (cpp = telopts; *cpp; cpp++) {
	    len = strlen(*cpp) + 1;
	    if (col + len > 65) {
		printf("\n\t");
		col = 8;
	    }
	    printf(" %s", *cpp);
	    col += len;
	}
	printf("\n");
	fflush(stdout);
	return 0;
    }
    cpp = genget(name, telopts, sizeof(char *));
    if (cpp == AMBIGUOUS) {
	fprintf(stderr,"'%s': ambiguous argument ('send %s ?' for help).\n",
					name, cmd);
	return 0;
    }
    if (cpp == 0) {
	fprintf(stderr, "'%s': unknown argument ('send %s ?' for help).\n",
					name, cmd);
	return 0;
    }
    if (!connected) {
	printf("?Need to be connected first.\n");
	return 0;
    }
    (*func)(cpp - telopts, 1);
    return 1;
}

static int send_help(const char *, const char *) {
    struct sendlist *s;	/* pointer to current command */
    for (s = Sendlist; s->name; s++) {
	if (s->help) {
	    printf("%-15s %s\n", s->name, s->help);
	    fflush(stdout);
	}
    }
    return(0);
}

/*
 * The following are the routines and data structures referred
 * to by the arguments to the "toggle" command.
 */

static int lclchars(int) {
    donelclchars = 1;
    return 1;
}

static int togdebug(int) {
    return nlink.setdebug(debug);
}


static int togcrlf(int) {
    if (crlf) {
	printf("Will send carriage returns as telnet <CR><LF>.\n");
    } 
    else {
	printf("Will send carriage returns as telnet <CR><NUL>.\n");
    }
    fflush(stdout);
    return 1;
}

int binmode;

static int togbinary(int val) {
    donebinarytoggle = 1;

    if (val >= 0) {
	binmode = val;
    } else {
	if (my_want_state_is_will(TELOPT_BINARY) &&
				my_want_state_is_do(TELOPT_BINARY)) {
	    binmode = 1;
	} else if (my_want_state_is_wont(TELOPT_BINARY) &&
				my_want_state_is_dont(TELOPT_BINARY)) {
	    binmode = 0;
	}
	val = binmode ? 0 : 1;
    }

    if (val == 1) {
	if (my_want_state_is_will(TELOPT_BINARY) &&
					my_want_state_is_do(TELOPT_BINARY)) {
	    printf("Already operating in binary mode with remote host.\n");
	} else {
	    printf("Negotiating binary mode with remote host.\n");
	    tel_enter_binary(3);
	}
    } else {
	if (my_want_state_is_wont(TELOPT_BINARY) &&
					my_want_state_is_dont(TELOPT_BINARY)) {
	    printf("Already in network ascii mode with remote host.\n");
	} else {
	    printf("Negotiating network ascii mode with remote host.\n");
	    tel_leave_binary(3);
	}
    }
    fflush(stdout);
    return 1;
}

static int togrbinary(int val) {
    donebinarytoggle = 1;

    if (val == -1)
	val = my_want_state_is_do(TELOPT_BINARY) ? 0 : 1;

    if (val == 1) {
	if (my_want_state_is_do(TELOPT_BINARY)) {
	    printf("Already receiving in binary mode.\n");
	} 
	else {
	    printf("Negotiating binary mode on input.\n");
	    tel_enter_binary(1);
	}
    } 
    else {
	if (my_want_state_is_dont(TELOPT_BINARY)) {
	    printf("Already receiving in network ascii mode.\n");
	} else {
	    printf("Negotiating network ascii mode on input.\n");
	    tel_leave_binary(1);
	}
    }
    fflush(stdout);
    return 1;
}

static int togxbinary(int val) {
    donebinarytoggle = 1;

    if (val == -1)
	val = my_want_state_is_will(TELOPT_BINARY) ? 0 : 1;

    if (val == 1) {
	if (my_want_state_is_will(TELOPT_BINARY)) {
	    printf("Already transmitting in binary mode.\n");
	} 
	else {
	    printf("Negotiating binary mode on output.\n");
	    tel_enter_binary(2);
	}
    } 
    else {
	if (my_want_state_is_wont(TELOPT_BINARY)) {
	    printf("Already transmitting in network ascii mode.\n");
	} 
	else {
	    printf("Negotiating network ascii mode on output.\n");
	    tel_leave_binary(2);
	}
    }
    fflush(stdout);
    return 1;
}


static int netdata;		/* Print out network data flow */
static int prettydump;	/* Print "netdata" output in user readable format */
static int termdata;    /* Print out terminal data flow */

static int togglehelp(int);

struct togglelist {
    const char *name;			/* name of toggle */
    const char *help;			/* help message */
    int (*handler)(int);	/* routine to do actual setting */
    int *variable;
    const char *actionexplanation;
};

static struct togglelist Togglelist[] = {
    { "autoflush", "flushing of output when sending interrupt characters",
      NULL, &autoflush,
      "flush output when sending interrupt characters" },

    { "autosynch", "automatic sending of interrupt characters in urgent mode",
      NULL, &autosynch,
      "send interrupt characters in urgent mode" },

#if 0
    { "autologin", "automatic sending of login and/or authentication info",
      NULL, &autologin,
      "send login name and/or authentication information" },
    { "authdebug", "Toggle authentication debugging",
      auth_togdebug, NULL,
      "print authentication debugging information" },
    { "autoencrypt", "automatic encryption of data stream",
      EncryptAutoEnc, NULL,
      "automatically encrypt output" },
    { "autodecrypt", "automatic decryption of data stream",
      EncryptAutoDec, NULL,
      "automatically decrypt input" },
    { "verbose_encrypt", "Toggle verbose encryption output",
      EncryptVerbose, NULL,
      "print verbose encryption output" },
    { "encdebug", "Toggle encryption debugging",
      EncryptDebug, NULL,
      "print encryption debugging information" },
#endif

    { "skiprc", "don't read ~/.telnetrc file",
      NULL, &skiprc,
      "read ~/.telnetrc file" },
    { "binary",
      "sending and receiving of binary data",
      togbinary, NULL,
      NULL },
    { "inbinary", "receiving of binary data",
      togrbinary, NULL,
      NULL },
    { "outbinary", "sending of binary data",
      togxbinary, 0,
      NULL },
    { "crlf", "sending carriage returns as telnet <CR><LF>",
      togcrlf, &crlf,
      NULL },
    { "crmod", "mapping of received carriage returns",
      NULL, &crmod,
      "map carriage return on output" },
    { "localchars", "local recognition of certain control characters",
      lclchars, &localchars,
      "recognize certain control characters" },

    { " ", "", 0 },		/* empty line */

#if defined(TN3270) && !defined(__linux__)
    { "apitrace", "(debugging) toggle tracing of API transactions",
      NULL, &apitrace,
      "trace API transactions" },
    { "cursesdata", "(debugging) toggle printing of hexadecimal curses data",
      NULL, &cursesdata,
      "print hexadecimal representation of curses data" },
#endif	/* TN3270 and not linux */

    { "debug", "debugging",
      togdebug,	&debug,
      "turn on socket level debugging" },
    { "netdata", "printing of hexadecimal network data (debugging)",
      NULL, &netdata,
      "print hexadecimal representation of network traffic" },
    { "prettydump","output of \"netdata\" to user readable format (debugging)",
      NULL, &prettydump,
      "print user readable output for \"netdata\"" },
    { "options", "viewing of options processing (debugging)",
      NULL, &showoptions,
      "show option processing" },

    { "termdata", "(debugging) toggle printing of hexadecimal terminal data",
      NULL, &termdata,
      "print hexadecimal representation of terminal traffic" },

    { "?", NULL, togglehelp },
    { "help", NULL, togglehelp },
    { 0 }
};

static int togglehelp(int) {
    struct togglelist *c;

    for (c = Togglelist; c->name; c++) {
	if (c->help) {
	    if (*c->help)
		printf("%-15s toggle %s\n", c->name, c->help);
	    else
		printf("\n");
	}
    }
    printf("\n");
    printf("%-15s %s\n", "?", "display help information");
    fflush(stdout);
    return 0;
}

static void settogglehelp(int set) {
    struct togglelist *c;

    for (c = Togglelist; c->name; c++) {
	if (c->help) {
	    if (*c->help)
		printf("%-15s %s %s\n", c->name, set ? "enable" : "disable",
						c->help);
	    else
		printf("\n");
	}
    }
    fflush(stdout);
}

#define	GETTOGGLE(name) (struct togglelist *) \
		genget(name, (char **) Togglelist, sizeof(struct togglelist))

static int toggle(int argc, const char *argv[]) {
    int retval = 1;
    const char *name;
    struct togglelist *c;

    if (argc < 2) {
	fprintf(stderr,
	    "Need an argument to 'toggle' command.  'toggle ?' for help.\n");
	return 0;
    }
    argc--;
    argv++;
    while (argc--) {
	name = *argv++;
	c = GETTOGGLE(name);
	if (c == AMBIGUOUS) {
	    fprintf(stderr, "'%s': ambiguous argument ('toggle ?' for help).\n",
					name);
	    return 0;
	} 
	else if (c == 0) {
	    fprintf(stderr, "'%s': unknown argument ('toggle ?' for help).\n",
					name);
	    return 0;
	} 
	else {
	    if (c->variable) {
		*c->variable = !*c->variable;		/* invert it */
		if (c->actionexplanation) {
		    printf("%s %s.\n", *c->variable? "Will" : "Won't",
							c->actionexplanation);
		}
	    }
	    if (c->handler) {
		retval &= (*c->handler)(-1);
	    }
	}
    }
    fflush(stdout);
    return retval;
}

/*
 * The following perform the "set" command.
 */

struct setlist {
    const char *name;				/* name */
    const char *help;				/* help information */
    void (*handler)(const char *);
    cc_t *charp;			/* where it is located at */
};

static struct setlist Setlist[] = {
#ifdef	KLUDGELINEMODE
    { "echo", 	"character to toggle local echoing on/off", 0, &echoc },
#endif
    { "escape",	"character to escape back to telnet command mode", 0, &escapechar },
    { "rlogin", "rlogin escape character", 0, &rlogin },
    { "tracefile", "file to write trace information to", SetNetTrace, (cc_t *)NetTraceFile},
    { " ", "" },
    { " ", "The following need 'localchars' to be toggled true", 0, 0 },
    { "flushoutput", "character to cause an Abort Output", 0, termFlushCharp },
    { "interrupt", "character to cause an Interrupt Process", 0, termIntCharp },
    { "quit",	"character to cause an Abort process", 0, termQuitCharp },
    { "eof",	"character to cause an EOF ", 0, termEofCharp },
    { " ", "" },
    { " ", "The following are for local editing in linemode", 0, 0 },
    { "erase",	"character to use to erase a character", 0, termEraseCharp },
    { "kill",	"character to use to erase a line", 0, termKillCharp },
    { "lnext",	"character to use for literal next", 0, termLiteralNextCharp },
    { "susp",	"character to cause a Suspend Process", 0, termSuspCharp },
    { "reprint", "character to use for line reprint", 0, termRprntCharp },
    { "worderase", "character to use to erase a word", 0, termWerasCharp },
    { "start",	"character to use for XON", 0, termStartCharp },
    { "stop",	"character to use for XOFF", 0, termStopCharp },
    { "forw1",	"alternate end of line character", 0, termForw1Charp },
    { "forw2",	"alternate end of line character", 0, termForw2Charp },
    { "ayt",	"alternate AYT character", 0, termAytCharp },
    { 0 }
};

#if	defined(CRAY) && !defined(__STDC__)
/* Work around compiler bug in pcc 4.1.5 */
    void
_setlist_init()
{
#ifndef	KLUDGELINEMODE
#define	N 5
#else
#define	N 6
#endif
	Setlist[N+0].charp = &termFlushChar;
	Setlist[N+1].charp = &termIntChar;
	Setlist[N+2].charp = &termQuitChar;
	Setlist[N+3].charp = &termEofChar;
	Setlist[N+6].charp = &termEraseChar;
	Setlist[N+7].charp = &termKillChar;
	Setlist[N+8].charp = &termLiteralNextChar;
	Setlist[N+9].charp = &termSuspChar;
	Setlist[N+10].charp = &termRprntChar;
	Setlist[N+11].charp = &termWerasChar;
	Setlist[N+12].charp = &termStartChar;
	Setlist[N+13].charp = &termStopChar;
	Setlist[N+14].charp = &termForw1Char;
	Setlist[N+15].charp = &termForw2Char;
	Setlist[N+16].charp = &termAytChar;
#undef	N
}
#endif	/* defined(CRAY) && !defined(__STDC__) */

static struct setlist *
getset(const char *name)
{
    return (struct setlist *)
		genget(name, (char **) Setlist, sizeof(struct setlist));
}

void set_escape_char(char *s) {
    if (rlogin != _POSIX_VDISABLE) {
	rlogin = (s && *s) ? special(s) : _POSIX_VDISABLE;
	printf("Telnet rlogin escape character is '%s'.\n",
	       control(rlogin));
    } 
    else {
	escapechar = (s && *s) ? special(s) : _POSIX_VDISABLE;
	printf("Telnet escape character is '%s'.\n", control(escapechar));
    }
    fflush(stdout);
}

static int setcmd(int argc, const char *argv[]) {
    int value;
    struct setlist *ct;
    struct togglelist *c;

    if (argc < 2 || argc > 3) {
	printf("Format is 'set Name Value'\n'set ?' for help.\n");
	fflush(stdout);
	return 0;
    }
    if ((argc == 2) && (isprefix(argv[1], "?") || isprefix(argv[1], "help"))) {
	for (ct = Setlist; ct->name; ct++)
	    printf("%-15s %s\n", ct->name, ct->help);
	printf("\n");
	settogglehelp(1);
	printf("%-15s %s\n", "?", "display help information");
	fflush(stdout);
	return 0;
    }

    ct = getset(argv[1]);
    if (ct == 0) {
	c = GETTOGGLE(argv[1]);
	if (c == 0) {
	    fprintf(stderr, "'%s': unknown argument ('set ?' for help).\n",
			argv[1]);
	    return 0;
	} 
	else if (c == AMBIGUOUS) {
	    fprintf(stderr, "'%s': ambiguous argument ('set ?' for help).\n",
			argv[1]);
	    return 0;
	}
	if (c->variable) {
	    if ((argc == 2) || (strcmp("on", argv[2]) == 0))
		*c->variable = 1;
	    else if (strcmp("off", argv[2]) == 0)
		*c->variable = 0;
	    else {
		printf("Format is 'set togglename [on|off]'\n'set ?' for help.\n");
		fflush(stdout);
		return 0;
	    }
	    if (c->actionexplanation) {
		printf("%s %s.\n", *c->variable? "Will" : "Won't",
							c->actionexplanation);
		fflush(stdout);
	    }
	}
	if (c->handler)
	    (*c->handler)(1);
    } 
    else if (argc != 3) {
	printf("Format is 'set Name Value'\n'set ?' for help.\n");
	fflush(stdout);
	return 0;
    } 
    else if (ct == AMBIGUOUS) {
	fprintf(stderr, "'%s': ambiguous argument ('set ?' for help).\n",
			argv[1]);
	return 0;
    } 
    else if (ct->handler) {
	(*ct->handler)(argv[2]);
	printf("%s set to \"%s\".\n", ct->name, (char *)ct->charp);
	fflush(stdout);
    } 
    else {
	if (strcmp("off", argv[2])) {
	    value = special(argv[2]);
	} else {
	    value = _POSIX_VDISABLE;
	}
	*(ct->charp) = (cc_t)value;
	printf("%s character is '%s'.\n", ct->name, control(*(ct->charp)));
	fflush(stdout);
    }
    slc_check();
    return 1;
}

static int unsetcmd(int argc, const char *argv[]) {
    struct setlist *ct;
    struct togglelist *c;
    const char *name;

    if (argc < 2) {
	fprintf(stderr,
	    "Need an argument to 'unset' command.  'unset ?' for help.\n");
	return 0;
    }
    if (isprefix(argv[1], "?") || isprefix(argv[1], "help")) {
	for (ct = Setlist; ct->name; ct++)
	    printf("%-15s %s\n", ct->name, ct->help);
	printf("\n");
	settogglehelp(0);
	printf("%-15s %s\n", "?", "display help information");
	fflush(stdout);
	return 0;
    }

    argc--;
    argv++;
    while (argc--) {
	name = *argv++;
	ct = getset(name);
	if (ct == 0) {
	    c = GETTOGGLE(name);
	    if (c == 0) {
		fprintf(stderr, "'%s': unknown argument ('unset ?' for help).\n",
			name);
		return 0;
	    } 
	    else if (c == AMBIGUOUS) {
		fprintf(stderr, "'%s': ambiguous argument ('unset ?' for help).\n",
			name);
		return 0;
	    }
	    if (c->variable) {
		*c->variable = 0;
		if (c->actionexplanation) {
		    printf("%s %s.\n", *c->variable? "Will" : "Won't",
							c->actionexplanation);
		}
	    }
	    if (c->handler)
		(*c->handler)(0);
	} 
	else if (ct == AMBIGUOUS) {
	    fprintf(stderr, "'%s': ambiguous argument ('unset ?' for help).\n",
			name);
	    return 0;
	} 
	else if (ct->handler) {
	    (*ct->handler)(0);
	    printf("%s reset to \"%s\".\n", ct->name, (char *)ct->charp);
	} 
	else {
	    *(ct->charp) = _POSIX_VDISABLE;
	    printf("%s character is '%s'.\n", ct->name, control(*(ct->charp)));
	}
    }
    fflush(stdout);
    return 1;
}

/*
 * The following are the data structures and routines for the
 * 'mode' command.
 */
#ifdef	KLUDGELINEMODE
extern int kludgelinemode;

static int dokludgemode(int) {
    kludgelinemode = 1;
    send_wont(TELOPT_LINEMODE, 1);
    send_dont(TELOPT_SGA, 1);
    send_dont(TELOPT_ECHO, 1);
    return 0;
}
#endif

static int dolinemode(int) {
#ifdef	KLUDGELINEMODE
    if (kludgelinemode)
	send_dont(TELOPT_SGA, 1);
#endif
    send_will(TELOPT_LINEMODE, 1);
    send_dont(TELOPT_ECHO, 1);
    return 1;
}

static int docharmode(int) {
#ifdef	KLUDGELINEMODE
    if (kludgelinemode)
	send_do(TELOPT_SGA, 1);
    else
#endif
    send_wont(TELOPT_LINEMODE, 1);
    send_do(TELOPT_ECHO, 1);
    return 1;
}

static int dolmmode(int bit, int on) {
    char c;
    extern int linemode;

    if (my_want_state_is_wont(TELOPT_LINEMODE)) {
	printf("?Need to have LINEMODE option enabled first.\n");
	printf("'mode ?' for help.\n");
	fflush(stdout);
	return 0;
    }

    if (on)
	c = (linemode | bit);
    else
	c = (linemode & ~bit);
    lm_mode(&c, 1, 1);
    return 1;
}

int setmode(int bit) {
    return dolmmode(bit, 1);
}

int clearmode(int bit) {
    return dolmmode(bit, 0);
}

struct modelist {
	const char	*name;		/* command name */
	const char	*help;		/* help string */
	int	(*handler)(int);	/* routine which executes command */
	int	needconnect;	/* Do we need to be connected to execute? */
	int	arg1;
};

extern int modehelp(int);

static struct modelist ModeList[] = {
    { "character", "Disable LINEMODE option",	docharmode, 1 },
#ifdef	KLUDGELINEMODE
    { "",          "(or disable obsolete line-by-line mode)", NULL },
#endif
    { "line",	   "Enable LINEMODE option",	dolinemode, 1 },
#ifdef	KLUDGELINEMODE
    { "",	   "(or enable obsolete line-by-line mode)", NULL },
#endif
    { "",          "",                                       NULL },
    { "",	"These require the LINEMODE option to be enabled", 0 },
    { "isig",	"Enable signal trapping",	setmode, 1, MODE_TRAPSIG },
    { "+isig",	0,				setmode, 1, MODE_TRAPSIG },
    { "-isig",	"Disable signal trapping",	clearmode, 1, MODE_TRAPSIG },
    { "edit",	"Enable character editing",	setmode, 1, MODE_EDIT },
    { "+edit",	0,				setmode, 1, MODE_EDIT },
    { "-edit",	"Disable character editing",	clearmode, 1, MODE_EDIT },
    { "softtabs", "Enable tab expansion",	setmode, 1, MODE_SOFT_TAB },
    { "+softtabs", 0,				setmode, 1, MODE_SOFT_TAB },
    { "-softtabs", "Disable character editing",	clearmode, 1, MODE_SOFT_TAB },
    { "litecho", "Enable literal character echo", setmode, 1, MODE_LIT_ECHO },
    { "+litecho", 0,				setmode, 1, MODE_LIT_ECHO },
    { "-litecho", "Disable literal character echo", clearmode, 1, MODE_LIT_ECHO },
    { "help",	0,				modehelp, 0 },
#ifdef	KLUDGELINEMODE
    { "kludgeline", 0,				dokludgemode, 1 },
#endif
    { "", "", 0 },
    { "?",	"Print help information",	modehelp, 0 },
    { 0 },
};


int modehelp(int) {
    struct modelist *mt;

    printf("format is:  'mode Mode', where 'Mode' is one of:\n\n");
    for (mt = ModeList; mt->name; mt++) {
	if (mt->help) {
	    if (*mt->help)
		printf("%-15s %s\n", mt->name, mt->help);
	    else
		printf("\n");
	}
    }
    fflush(stdout);
    return 0;
}

#define	GETMODECMD(name) (struct modelist *) \
		genget(name, (char **) ModeList, sizeof(struct modelist))

static int modecmd(const char *arg) {
    struct modelist *mt;

    mt = GETMODECMD(arg);
    if (mt == 0) {
	fprintf(stderr, "Unknown mode '%s' ('mode ?' for help).\n", arg);
    } 
    else if (mt == AMBIGUOUS) {
	fprintf(stderr, "Ambiguous mode '%s' ('mode ?' for help).\n", arg);
    } 
    else if (mt->needconnect && !connected) {
	printf("?Need to be connected first.\n");
	printf("'mode ?' for help.\n");
    }
    else if (mt->handler) {
	return (*mt->handler)(mt->arg1);
    }
    fflush(stdout);
    return 0;
}

/*
 * The following data structures and routines implement the
 * "display" command.
 */

static void dotog(struct togglelist *tl) {
    if (tl->variable && tl->actionexplanation) {
	if (*tl->variable) {
	    printf("will");
	} 
	else {
	    printf("won't");
	}
	printf(" %s.\n", tl->actionexplanation);
    }
    fflush(stdout);
}

static void doset(struct setlist *sl) {
    if (sl->name && *sl->name != ' ') {
	if (sl->handler == 0) {
	    printf("%-15s [%s]\n", sl->name, control(*sl->charp));
	}
	else {
	    printf("%-15s \"%s\"\n", sl->name, (char *)sl->charp);
	}
    }
    fflush(stdout);
}

static int display(int argc, const char *argv[]) {
    struct togglelist *tl;
    struct setlist *sl;

    if (argc == 1) {
	for (tl = Togglelist; tl->name; tl++) {
	    dotog(tl);
	}
	printf("\n");
	for (sl = Setlist; sl->name; sl++) {
	    doset(sl);
	}
    } 
    else {
	int i;

	for (i = 1; i < argc; i++) {
	    sl = getset(argv[i]);
	    tl = GETTOGGLE(argv[i]);
	    if (sl == AMBIGUOUS || tl == AMBIGUOUS) {
		printf("?Ambiguous argument '%s'.\n", argv[i]);
		return 0;
	    } 
	    else if (!sl && !tl) {
		printf("?Unknown argument '%s'.\n", argv[i]);
		return 0;
	    } 
	    else {
		if (tl) {
		    dotog(tl);
		}
		if (sl) {
		    doset(sl);
		}
	    }
	}
    }
    optionstatus();
    fflush(stdout);
    return 1;
}

/*
 * The following are the data structures, and many of the routines,
 * relating to command processing.
 */

/*
 * Set the escape character.
 */
static int setescape(int argc, const char *argv[]) {
	const char *arg;
	char buf[50];

	printf(
	    "Deprecated usage - please use 'set escape%s%s' in the future.\n",
				(argc > 2)? " ":"", (argc > 2)? argv[1]: "");
	if (argc > 2) {
		arg = argv[1];
	}
	else {
		printf("new escape character: ");
		(void) fgets(buf, sizeof(buf), stdin);
		arg = buf;
	}
	if (arg[0] != '\0')
		escapechar = arg[0];
	if (!In3270) {
		printf("Escape character is '%s'.\n", control(escapechar));
	}
	(void) fflush(stdout);
	return 1;
}

static int togcrmod(void) {
    crmod = !crmod;
    printf("Deprecated usage - please use 'toggle crmod' in the future.\n");
    printf("%s map carriage return on output.\n", crmod ? "Will" : "Won't");
    fflush(stdout);
    return 1;
}

int suspend(void) {
#ifdef	SIGTSTP
    setcommandmode();
    {
	long oldrows, oldcols, newrows, newcols, err;

	err = TerminalWindowSize(&oldrows, &oldcols);
	(void) kill(0, SIGTSTP);
	err += TerminalWindowSize(&newrows, &newcols);
	if (connected && !err &&
	    ((oldrows != newrows) || (oldcols != newcols))) {
		sendnaws();
	}
    }
    /* reget parameters in case they were changed */
    TerminalSaveState();
    setconnmode(0);
#else
    printf("Suspend is not supported.  Try the '!' command instead\n");
    fflush(stdout);
#endif
    return 1;
}

#if defined(TN3270)
int shell(int argc, const char **) {
    setcommandmode();

    switch(vfork()) {
    case -1:
	perror("Fork failed\n");
	break;

    case 0:
	{
	    /*
	     * Fire up the shell in the child.
	     */
	    const char *shellp, *shellname;

	    shellp = getenv("SHELL");
	    if (shellp == NULL)
		shellp = "/bin/sh";
	    if ((shellname = rindex(shellp, '/')) == 0)
		shellname = shellp;
	    else
		shellname++;
	    if (argc > 1)
		execl(shellp, shellname, "-c", &saveline[1], 0);
	    else
		execl(shellp, shellname, 0);
	    perror("Execl");
	    _exit(1);
	}
    default:
	    wait(NULL);	/* Wait for the shell to complete */
    }
    return 1;
}
#endif	/* !defined(TN3270) */

static int dobye(int isfromquit) {
    extern int resettermname;

    if (connected) {
	nlink.close(1);
	printf("Connection closed.\n");
	connected = 0;
	resettermname = 1;

	/* reset options */
	tninit();
#if	defined(TN3270)
	SetIn3270();		/* Get out of 3270 mode */
#endif	/* defined(TN3270) */
    }
    if (!isfromquit) {
#ifdef XXX
	siglongjmp(toplevel, 1);
#endif
	/* NOTREACHED */
    }
    fflush(stdout);
    return 1;			/* Keep lint, etc., happy */
}

static int bye(void) {
    if (!connected) {
	printf("Need to be connected first for `bye'.\n");
	fflush(stdout);
	return 0;
    }
    return dobye(0);
}

void quit(void) {
    dobye(1);
    Exit(0);
}

int logout(void) {
    if (!connected) {
	printf("Need to be connected first for `logout'.\n");
	fflush(stdout);
	return 0;
    }
    send_do(TELOPT_LOGOUT, 1);
    netflush();
    return 1;
}

/*
 * The ENVIRON command.
 */

struct envcmd {
	const char	*name;
	const char	*help;
	void	(*handler)(const char *, const char *);
	int	narg;
};

static void env_help(const char *, const char *);

typedef void (*envfunc)(const char *, const char *);

struct envcmd EnvList[] = {
    { "define",	"Define an environment variable",
						env_define,	2 },
    { "undefine", "Undefine an environment variable",
						(envfunc) env_undefine,	1 },
    { "export",	"Mark an environment variable for automatic export",
						(envfunc) env_export,	1 },
    { "unexport", "Don't mark an environment variable for automatic export",
						(envfunc) env_unexport,	1 },
    { "send",	"Send an environment variable", (envfunc) env_send,	1 },
    { "list",	"List the current environment variables",
						(envfunc) env_list,	0 },
    { "help",	0,				env_help,		0 },
    { "?",	"Print help information",	env_help,		0 },
    { 0 },
};

static void env_help(const char *, const char *) {
    struct envcmd *c;

    for (c = EnvList; c->name; c++) {
	if (c->help) {
	    if (*c->help)
		printf("%-15s %s\n", c->name, c->help);
	    else
		printf("\n");
	}
    }
    fflush(stdout);
}

static struct envcmd *getenvcmd(const char *name) {
    return (struct envcmd *)
		genget(name, (char **) EnvList, sizeof(struct envcmd));
}

int env_cmd(int argc, const char *argv[]) {
    struct envcmd *c;

    if (argc < 2) {
	fprintf(stderr,
	    "Need an argument to 'environ' command.  'environ ?' for help.\n");
	return 0;
    }
    c = getenvcmd(argv[1]);
    if (c == 0) {
        fprintf(stderr, "'%s': unknown argument ('environ ?' for help).\n",
    				argv[1]);
        return 0;
    }
    if (c == AMBIGUOUS) {
        fprintf(stderr, "'%s': ambiguous argument ('environ ?' for help).\n",
    				argv[1]);
        return 0;
    }
    if (c->narg + 2 != argc) {
	fprintf(stderr,
	    "Need %s%d argument%s to 'environ %s' command.  'environ ?' for help.\n",
		c->narg < argc + 2 ? "only " : "",
		c->narg, c->narg == 1 ? "" : "s", c->name);
	return 0;
    }
    (*c->handler)(argv[2], argv[3]);
    return 1;
}


/*
 * The AUTHENTICATE command.
 *
 *    auth status      Display status
 *    auth disable     Disable an authentication type
 *    auth enable      Enable an authentication type
 *
 * The ENCRYPT command.
 *
 *   encrypt enable	Enable encryption
 *   encrypt disable	Disable encryption
 *   encrypt type foo   Set encryption type
 *   encrypt start      Start encryption
 *   encrypt stop       Stop encryption
 *   encrypt input      Start encrypting input stream
 *   encrypt -input     Stop encrypting input stream
 *   encrypt output     Start encrypting output stream
 *   encrypt -output    Stop encrypting output stream
 *   encrypt status     Print status
 */


#ifdef TN3270
char *oflgs[] = { "read-only", "write-only", "read-write" };
    
static void filestuff(int fd) {
    int res;

#ifdef	F_GETOWN
    setconnmode(0);
    res = fcntl(fd, F_GETOWN, 0);
    setcommandmode();

    if (res == -1) {
	perror("fcntl");
	return;
    }
    printf("\tOwner is %d.\n", res);
#endif

    setconnmode(0);
    res = fcntl(fd, F_GETFL, 0);
    setcommandmode();

    if (res == -1) {
	perror("fcntl");
	return;
    }
    printf("\tFlags are 0x%x: %s\n", res, oflgs[res]);
    fflush(stdout);
}
#endif /* TN3270 */

/*
 * Print status about the connection.
 */
static int dostatus(int notmuch) {
    if (connected) {
	printf("Connected to %s.\n", hostname);
	if (!notmuch) {
	    int mode = getconnmode();

	    if (my_want_state_is_will(TELOPT_LINEMODE)) {
		printf("Operating with LINEMODE option\n");
		printf("%s line editing\n", (mode&MODE_EDIT) ? "Local" : "No");
		printf("%s catching of signals\n",
					(mode&MODE_TRAPSIG) ? "Local" : "No");
		slcstate();
#ifdef	KLUDGELINEMODE
	    } 
	    else if (kludgelinemode && my_want_state_is_dont(TELOPT_SGA)) {
		printf("Operating in obsolete linemode\n");
#endif
	    } 
	    else {
		printf("Operating in single character mode\n");
		if (localchars)
		    printf("Catching signals locally\n");
	    }
	    printf("%s character echo\n", (mode&MODE_ECHO) ? "Local" : "Remote");
	    if (my_want_state_is_will(TELOPT_LFLOW))
		printf("%s flow control\n", (mode&MODE_FLOW) ? "Local" : "No");
	}
    } 
    else {
	printf("No connection.\n");
    }
#if !defined(TN3270)
    printf("Escape character is '%s'.\n", control(escapechar));
    (void) fflush(stdout);
#else /* !defined(TN3270) */
    if ((!In3270) && !notmuch) {
	printf("Escape character is '%s'.\n", control(escape));
    }
    if ((argc >= 2) && !strcmp(argv[1], "everything")) {
	printf("SIGIO received %d time%s.\n",
				sigiocount, (sigiocount == 1)? "":"s");
	if (In3270) {
	    printf("Process ID %d, process group %d.\n",
					    getpid(), getpgrp(getpid()));
	    printf("Terminal input:\n");
	    filestuff(tin);
	    printf("Terminal output:\n");
	    filestuff(tout);
	    printf("Network socket:\n");
	    filestuff(net);
	}
    }
    if (In3270 && transcom) {
       printf("Transparent mode command is '%s'.\n", transcom);
    }
    fflush(stdout);
    if (In3270) {
	return 0;
    }
#endif /* TN3270 */
    return 1;
}

static int status(void) {
    int notmuch = 1;
    return dostatus(notmuch);
}

#ifdef	SIGINFO
/*
 * Function that gets called when SIGINFO is received.
 */
void ayt_status(int) {
    dostatus(1);
}
#endif

int tn(int argc, const char *argv[]) {
    register struct hostent *host = 0;
    struct sockaddr_in sn;
    struct servent *sp = 0;
    int temp;
    char *srp = NULL;
    int srlen;

    const char *cmd, *volatile user = 0;
    const char *portp = NULL;
    char *hostp = NULL;

    /* clear the socket address prior to use */
    memset(&sn, 0, sizeof(sn));

    if (connected) {
	printf("?Already connected to %s\n", hostname);
	fflush(stdout);
	setuid(getuid());
	return 0;
    }
    if (argc < 2) {
	(void) strcpy(line, "open ");
	printf("(to) ");
	fflush(stdout);
	(void) fgets(&line[strlen(line)], sizeof(line) - strlen(line), stdin);
	makeargv();
	argc = margc;
	argv = margv;
    }
    cmd = *argv;
    --argc; ++argv;
    while (argc) {
	if (isprefix(*argv, "help") || isprefix(*argv, "?"))
	    goto usage;
	if (strcmp(*argv, "-l") == 0) {
	    --argc; ++argv;
	    if (argc == 0)
		goto usage;
	    user = *argv++;
	    --argc;
	    continue;
	}
	if (strcmp(*argv, "-a") == 0) {
	    --argc; ++argv;
	    autologin = 1;
	    continue;
	}
	if (hostp == 0) {
	    /* this leaks memory - FIXME */
	    hostp = strdup(*argv++);
	    --argc;
	    continue;
	}
	if (portp == 0) {
	    portp = *argv++;
	    --argc;
	    continue;
	}
    usage:
	printf("usage: %s [-l user] [-a] host-name [port]\n", cmd);
	fflush(stdout);
	setuid(getuid());
	return 0;
    }
    if (hostp == 0)
	goto usage;

#if defined(IP_OPTIONS) && defined(HAS_IPPROTO_IP) && !defined(__WIN32__)
    if (hostp[0] == '@' || hostp[0] == '!') {
	if ((hostname = strrchr(hostp, ':')) == NULL)
	    hostname = strrchr(hostp, '@');
	hostname++;
	srp = 0;
	temp = sourceroute(hostp, &srp, &srlen);
	if (temp == 0) {
	    herror(srp);
	    setuid(getuid());
	    return 0;
	} else if (temp == -1) {
	    printf("Bad source route option: %s\n", hostp);
	    fflush(stdout);
	    setuid(getuid());
	    return 0;
	} else {
	    sn.sin_addr.s_addr = temp;
	    sn.sin_family = AF_INET;
	}
    } 
    else {
#endif
	temp = inet_addr(hostp);
	if (temp != -1) {
	    sn.sin_addr.s_addr = temp;
	    sn.sin_family = AF_INET;
	    strcpy(_hostname, hostp);
	    hostname = _hostname;
	} 
	else {
	    host = gethostbyname(hostp);
	    if (host) {
		sn.sin_family = host->h_addrtype;
#if	defined(h_addr)		/* In 4.3, this is a #define */
		memcpy((caddr_t)&sn.sin_addr,
				host->h_addr_list[0], host->h_length);
#else	/* defined(h_addr) */
		memcpy((caddr_t)&sn.sin_addr, host->h_addr, host->h_length);
#endif	/* defined(h_addr) */
		strncpy(_hostname, host->h_name, sizeof(_hostname));
		_hostname[sizeof(_hostname)-1] = '\0';
		hostname = _hostname;
	    } else {
		herror(hostp);
	        setuid(getuid());
		return 0;
	    }
	}
#if defined(IP_OPTIONS) && defined(HAS_IPPROTO_IP) && !defined(__WIN32__)
    }
#endif
    if (portp) {
	if (*portp == '-') {
	    portp++;
	    telnetport = 1;
	} else
	    telnetport = 0;
	sn.sin_port = atoi(portp);
	if (sn.sin_port == 0) {
	    sp = getservbyname(portp, "tcp");
	    if (sp)
		sn.sin_port = sp->s_port;
	    else {
		printf("%s: bad port number\n", portp);
		fflush(stdout);
	        setuid(getuid());
		return 0;
	    }
	} 
	else {
	    sn.sin_port = htons(sn.sin_port);
	}
    } 
    else {
	if (sp == 0) {
	    sp = getservbyname("telnet", "tcp");
	    if (sp == 0) {
		fprintf(stderr, "telnet: tcp/telnet: unknown service\n");
#ifndef __WIN32__
	        setuid(getuid());
#endif
		return 0;
	    }
	    sn.sin_port = sp->s_port;
	}
	telnetport = 1;
    }
    printf("Trying %s...\n", inet_ntoa(sn.sin_addr));
    fflush(stdout);
    do {
	int x = nlink.connect(debug, host, &sn, srp, srlen, tos);
	if (!x) return 0;
	else if (x==1) continue;
	connected++;
    } while (connected == 0);
    cmdrc(hostp, hostname);
    if (autologin && user == NULL) {
#ifndef __WIN32__
	struct passwd *pw;
#endif
	user = getenv("USER");
#ifndef __WIN32__
	if (user == NULL ||
	    ((pw = getpwnam(user))!=NULL && pw->pw_uid != getuid())) {
		if ((pw = getpwuid(getuid()))!=NULL)
			user = pw->pw_name;
		else
			user = NULL;
	}
#endif
    }
    if (user) {
	env_define("USER", user);
	env_export("USER");
    }
    dostatus(1);
#ifdef XXX
    if (sigsetjmp(peerdied, 1) == 0)
#endif
	telnet(user);
    nlink.close(0);
    ExitString("Connection closed by foreign host.\n",1);
    /*NOTREACHED*/
    return 0;
}

static char
	openhelp[] =	"connect to a site",
	closehelp[] =	"close current connection",
	logouthelp[] =	"forcibly logout remote user and close the connection",
	quithelp[] =	"exit telnet",
	statushelp[] =	"print status information",
	sendhelp[] =	"transmit special characters ('send ?' for more)",
	sethelp[] = 	"set operating parameters ('set ?' for more)",
	unsethelp[] = 	"unset operating parameters ('unset ?' for more)",
	togglestring[] ="toggle operating parameters ('toggle ?' for more)",
	displayhelp[] =	"display operating parameters",
#ifdef TN3270
	transcomhelp[] = "specify Unix command for transparent mode pipe",
#endif /* TN3270 */
	zhelp[] =	"suspend telnet",
/*	shellhelp[] =	"invoke a subshell", */
	envhelp[] =	"change environment variables ('environ ?' for more)",
	modestring[] = "try to enter line or character mode ('mode ?' for more)";

static char	crmodhelp[] =	"deprecated command -- use 'toggle crmod' instead";
static char	escapehelp[] =	"deprecated command -- use 'set escape' instead";

static int help(command_table *, int, const char **);

static int doquit(void) {
    quit();
    return 0;
}

static int slc_mode_import_0(void) {
    slc_mode_import(0);
    return 1;
}

static int slc_mode_import_1(void) {
    slc_mode_import(1);
    return 1;
}

static int do_slc_mode_export(void) {
    slc_mode_export();
    return 1;
}

static ptrarray<command_entry> cmdtab;
static ptrarray<command_entry> cmdtab2;
static ptrarray<command_entry> slctab;

#define BIND(a,b,c) cmdtab.add(new command_entry(a,b,c))
#define BIND2(a,b,c) cmdtab2.add(new command_entry(a,b,c))
#define BINDS(a,b,c) slctab.add(new command_entry(a,b,c))


void cmdtab_init(void) {
    BIND("close",   closehelp,    bye);
    BIND("logout",  logouthelp,   logout);
    BIND("display", displayhelp,  display);
    BIND("mode",    modestring,   modecmd);
    BIND("open",    openhelp,     tn);
    BIND("quit",    quithelp,     doquit);
    BIND("send",    sendhelp,     sendcmd);
    BIND("set",     sethelp,      setcmd);
    BIND("unset",   unsethelp,    unsetcmd);
    BIND("status",  statushelp,   status);
    BIND("toggle",  togglestring, toggle);
    BIND("slc", "set treatment of special characters\n", &slctab);

#ifdef TN3270
    BIND("transcom", transcomhelp, settranscom);
#endif /* TN3270 */

    // BIND("auth", authhelp, auth_cmd);
    // BIND("encrypt", encrypthelp, encrypt_cmd);

    BIND("z", zhelp, suspend);

#if defined(TN3270)   /* why?! */
    BIND("!", shellhelp, shell);
#endif

    BIND("environ", envhelp, env_cmd);

    BINDS("export", "Use local special character definitions",
	 do_slc_mode_export);
    BINDS("import", "Use remote special character definitions",
	 slc_mode_import_1);
    BINDS("check", "Verify remote special character definitions",
	 slc_mode_import_0);

    BIND2("escape", escapehelp, setescape);
    BIND2("crmod", crmodhelp, togcrmod);
}


static command_entry *getcmd(command_table *tab, const char *name) {
    if (!strcasecmp(name, "?") || 
	!strcasecmp(name, "h") ||
	!strcasecmp(name, "help")) return (command_entry *)HELP;

    command_entry *found = NULL;

    for (int i=0; i<tab->num(); i++) {
	command_entry *c = (*tab)[i];
	if (!strcasecmp(c->getname(), name)) return c;
	if (!strncasecmp(c->getname(), name, strlen(name))) {
	    if (found) return (command_entry *)AMBIGUOUS;
	    found = c;
	}
    }
    if (tab==&cmdtab && !found) return getcmd(&cmdtab2, name);

    return found;
}

static int process_command(command_table *tab, int argc, const char **argv) {
    command_entry *c;
    c = getcmd(tab, argv[0]);
    if (c == HELP) {
	help(tab, argc, argv);
    }
    else if (c == AMBIGUOUS) {
	printf("?Ambiguous command\n");
    }
    else if (c == NULL) {
	printf("?Invalid command\n");
    }
    else {
	if (c->call(argc, argv)) return 1;
    }
    fflush(stdout);
    return 0;
}

void command(int top, const char *tbuf, int cnt) {

    setcommandmode();
    if (!top) {
	putchar('\n');
    } 
    else {
	signal(SIGINT, SIG_DFL);
#ifdef SIGQUIT
	signal(SIGQUIT, SIG_DFL);
#endif
    }
    for (;;) {
	if (rlogin == _POSIX_VDISABLE) {
		printf("%s> ", prompt);
		fflush(stdout);
	}
	if (tbuf) {
	    char *cp = line;
	    while (cnt > 0 && (*cp++ = *tbuf++) != '\n')
		cnt--;
	    tbuf = 0;
	    if (cp == line || *--cp != '\n' || cp == line)
		goto getline;
	    *cp = '\0';
	    if (rlogin == _POSIX_VDISABLE) {
	        printf("%s\n", line);
		fflush(stdout);
	    }
	} 
	else {
	 getline:
	    if (rlogin != _POSIX_VDISABLE) {
		printf("%s> ", prompt);
		fflush(stdout);
	    }

	    if (fgets(line, sizeof(line), stdin) == NULL) {
		if (feof(stdin) || ferror(stdin)) {
		    quit();
		    /*NOTREACHED*/
		}
		break;
	    }
	}
	if (line[0] == 0)
	    break;
	makeargv();
	if (margv[0] == 0) {
	    break;
	}
	if (process_command(&cmdtab, margc, margv)) break;
    }
    if (!top) {
	if (!connected) {
#ifdef XXX
	    siglongjmp(toplevel, 1);
#endif
	    /*NOTREACHED*/
	}
#if	defined(TN3270)
	if (shell_active == 0) {
	    setconnmode(0);
	}
#else	/* defined(TN3270) */
	setconnmode(0);
#endif	/* defined(TN3270) */
    }
}

/*
 * Help command.
 */
static int help(command_table *tab, int argc, const char *argv[]) {
    int i;
    
    if (argc == 1) {
	printf("Commands may be abbreviated.  Commands are:\n\n");
	for (i = 0; i<tab->num(); i++) (*tab)[i]->describe();
	fflush(stdout);
	return 0;
    }
    for (i=1; i<argc; i++) {
	command_entry *c = getcmd(tab, argv[i]);
	if (c == HELP) {
	    printf("Print help information\n");
	}
	else if (c == AMBIGUOUS) {
	    printf("?Ambiguous help command %s\n", argv[i]);
	}
	else if (c == NULL) {
	    printf("?Invalid help command %s\n", argv[i]);
	}
	else {
	    c->gethelp();
	}
    }
    fflush(stdout);
    return 0;
}

static char *rcname = 0;
static char rcbuf[128];

void cmdrc(const char *m1, const char *m2) {
    FILE *rcfile;
    int gotmachine = 0;
    int l1 = strlen(m1);
    int l2 = strlen(m2);
    char m1save[64];

    if (skiprc) return;

    strcpy(m1save, m1);
    m1 = m1save;

    if (rcname == 0) {
	rcname = getenv("HOME");
	if (rcname)
	    strcpy(rcbuf, rcname);
	else
	    rcbuf[0] = '\0';
	strcat(rcbuf, "/.telnetrc");
	rcname = rcbuf;
    }

    rcfile = fopen(rcname, "r");
    if (!rcfile) return;

    while (fgets(line, sizeof(line), rcfile)) {
	if (line[0] == 0)
	    break;
	if (line[0] == '#')
	    continue;
	if (gotmachine) {
	    if (!isspace(line[0]))
		gotmachine = 0;
	}
	if (gotmachine == 0) {
	    if (isspace(line[0]))
		continue;
	    if (strncasecmp(line, m1, l1) == 0)
		strncpy(line, &line[l1], sizeof(line) - l1);
	    else if (strncasecmp(line, m2, l2) == 0)
		strncpy(line, &line[l2], sizeof(line) - l2);
	    else if (strncasecmp(line, "DEFAULT", 7) == 0)
		strncpy(line, &line[7], sizeof(line) - 7);
	    else
		continue;
	    if (line[0] != ' ' && line[0] != '\t' && line[0] != '\n')
		continue;
	    gotmachine = 1;
	}
	makeargv();
	if (margv[0] == 0)
	    continue;
	process_command(&cmdtab, margc, margv);
    }
    fclose(rcfile);
}

#if defined(IP_OPTIONS) && defined(HAS_IPPROTO_IP) && !defined(__WIN32__)

/*
 * Source route is handed in as
 *	[!]@hop1@hop2...[@|:]dst
 * If the leading ! is present, it is a
 * strict source route, otherwise it is
 * assmed to be a loose source route.
 *
 * We fill in the source route option as
 *	hop1,hop2,hop3...dest
 * and return a pointer to hop1, which will
 * be the address to connect() to.
 *
 * Arguments:
 *	arg:	pointer to route list to decipher
 *
 *	cpp: 	If *cpp is not equal to NULL, this is a
 *		pointer to a pointer to a character array
 *		that should be filled in with the option.
 *
 *	lenp:	pointer to an integer that contains the
 *		length of *cpp if *cpp != NULL.
 *
 * Return values:
 *
 *	Returns the address of the host to connect to.  If the
 *	return value is -1, there was a syntax error in the
 *	option, either unknown characters, or too many hosts.
 *	If the return value is 0, one of the hostnames in the
 *	path is unknown, and *cpp is set to point to the bad
 *	hostname.
 *
 *	*cpp:	If *cpp was equal to NULL, it will be filled
 *		in with a pointer to our static area that has
 *		the option filled in.  This will be 32bit aligned.
 * 
 *	*lenp:	This will be filled in with how long the option
 *		pointed to by *cpp is.
 *	
 */
static unsigned long sourceroute(char *arg, char **cpp, int *lenp) {
	static char lsr[44];
	char *cp, *cp2, *lsrp, *lsrep;
	register int tmp;
	struct in_addr sin_addr;
	register struct hostent *host = 0;
	register char c;

	/*
	 * Verify the arguments, and make sure we have
	 * at least 7 bytes for the option.
	 */
	if (cpp == NULL || lenp == NULL)
		return((unsigned long)-1);
	if (*cpp != NULL && *lenp < 7)
		return((unsigned long)-1);
	/*
	 * Decide whether we have a buffer passed to us,
	 * or if we need to use our own static buffer.
	 */
	if (*cpp) {
		lsrp = *cpp;
		lsrep = lsrp + *lenp;
	} 
	else {
		*cpp = lsrp = lsr;
		lsrep = lsrp + 44;
	}

	cp = arg;

	/*
	 * Next, decide whether we have a loose source
	 * route or a strict source route, and fill in
	 * the begining of the option.
	 */
	if (*cp == '!') {
		cp++;
		*lsrp++ = IPOPT_SSRR;
	} 
	else *lsrp++ = IPOPT_LSRR;

	if (*cp != '@')
		return((unsigned long)-1);

	lsrp++;		/* skip over length, we'll fill it in later */
	*lsrp++ = 4;

	cp++;

	sin_addr.s_addr = 0;

	for (c = 0;;) {
		if (c == ':')
			cp2 = 0;
		else for (cp2 = cp; (c = *cp2) != 0; cp2++) {
			if (c == ',') {
				*cp2++ = '\0';
				if (*cp2 == '@')
					cp2++;
			} else if (c == '@') {
				*cp2++ = '\0';
			} else if (c == ':') {
				*cp2++ = '\0';
			} else
				continue;
			break;
		}
		if (!c)
			cp2 = 0;

		if ((tmp = inet_addr(cp)) != -1) {
			sin_addr.s_addr = tmp;
		} 
		else if ((host = gethostbyname(cp))!=NULL) {
#if defined(h_addr)
			memcpy((caddr_t)&sin_addr,
				host->h_addr_list[0], host->h_length);
#else
			memcpy((caddr_t)&sin_addr, host->h_addr, host->h_length);
#endif
		} else {
			*cpp = cp;
			return(0);
		}
		memcpy(lsrp, (char *)&sin_addr, 4);
		lsrp += 4;
		if (cp2)
			cp = cp2;
		else
			break;
		/*
		 * Check to make sure there is space for next address
		 */
		if (lsrp + 4 > lsrep)
			return((unsigned long)-1);
	}
	if ((*(*cpp+IPOPT_OLEN) = lsrp - *cpp) <= 7) {
		*cpp = 0;
		*lenp = 0;
		return((unsigned long)-1);
	}
	*lsrp++ = IPOPT_NOP; /* 32 bit word align it */
	*lenp = lsrp - *cpp;
	return(sin_addr.s_addr);
}
#endif
