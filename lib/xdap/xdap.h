
/* Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2000-2006 Damien Sandras
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * Ekiga is licensed under the GPL license and as a special exception,
 * you have permission to link or otherwise combine this program with the
 * programs OPAL, OpenH323 and PWLIB, and distribute the combination,
 * without applying the requirements of the GNU GPL to the OPAL, OpenH323
 * and PWLIB programs, as long as you do follow the requirements of the
 * GNU GPL for all the rest of the software thus combined.
 */


/*
 * XML LDAP client
 * Copyright 2001 paul666@mailandnews.com
 * Licensed under version 2 or later of the GNU GPL
 */


#ifndef __XDAP_H_
#define __XDAP_H_

#ifndef WIN32
#include <sys/time.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libxml/parser.h>
#include <lber.h>
#include <ldap.h>

#include <glib.h>



G_BEGIN_DECLS

/* Private per element data structure */
struct edata {
	int optype;		/* LDAP operation code */
	int fertile;		/* Can have children */
	int modop;		/* Default mod_op for attributes */
	int eval;		/* evaluate element text */
};

/* somehow not declared in string.h */
#if !HAVE_DECL_STRCASECMP
int strcasecmp (const char *s1, const char *s2);
#endif


#define DEBUG 0			/* Debugging compiled in or not */
#if DEBUG
int dbgmask;			/* Debug mask */
#define D(m, f) if (dbgmask & m) f; else
#else
#define D(m, f)
#endif
/* Debug bits for fprintfs */
#define D_SHOWTREE	0x0001
#define D_MISC		0x0002
#define D_SHOWATTS	0x0004
#define D_SHOWBLK	0x0008
#define D_SHOWELS	0x0010

#define D_NODES		0x0040
#define D_SHOWOP	0x0080
#define D_TRACE		0x0100

/* Local error codes */
#define PFERR		-100
#define PFEMPTY		-101
#define PFNOCHILD	-102
#define PFUNKOP		-103
#define PFNOHOST	-104
#define PFNOPORT	-105
#define PFNOOPEN	-106
#define PFTOOMANYATTRS	-107
#define PFTOOMANYOPS	-108
#define PFBADROOT	-109
#define PFBADMETHOD	-110
#define PFNONODE	-111
#define PFBADSCOPE	-112
#define PFBADIGN	-113
#define PFNOSUB		-114
#define PFBADMODOP	-115
#define PFNOMEM		-116
#define PFNODN		-117

/* LDAP operation */
#define LD_ADD		1
#define LD_MOD		2
#define LD_DEL		3
#define LD_SEARCH	4
#ifdef DOSYS
#define LD_SYS		5
#endif

/* Ignore attribute masks */
#define IGN_EX		1
#define IGN_NL		2
#define IGN_NO		4
#define IGN_NA		8

/* Hacks */
#define MAXATTRS 200
#define MAXOPS 20


xmlDocPtr parseonly (char *, xmlEntityPtr (*)(void *, const xmlChar *),
		     xmlEntityPtr (**)(void *, const xmlChar *), int);
LDAP *ldapopen (char *, int, char *, char *, int);
int ldaprun (LDAP *, xmlDocPtr, xmlNodePtr *, int *, unsigned int *, int);
int ldapclose (LDAP *, xmlDocPtr);
int getldapinfo (xmlDocPtr, xmlNodePtr *, char **, int *, char **, char **,
		 ber_tag_t *);
char *pferrtostring (int);
void dumpres (LDAP *, LDAPMessage *);
int parsefile (char *, xmlEntityPtr (*)(void *, const xmlChar *),
	       xmlEntityPtr (**)(void *, const xmlChar *));

#define XDAPLEAKCHECK 0 /* define this to show leaks of libxml node->_private */
#define REGMAX 500 /* maximum no of slots to store malloced pointers */
#define DEBUGXDAPLEAK 0 /* show allocs/frees */
void xdapleakcheck(void);
void xdapfree(void);

G_END_DECLS

#endif