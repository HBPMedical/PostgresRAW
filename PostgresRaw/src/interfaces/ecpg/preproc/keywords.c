/*-------------------------------------------------------------------------
 *
 * keywords.c
 *	  lexical token lookup for key words in PostgreSQL
 *
 *
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/interfaces/ecpg/preproc/keywords.c,v 1.90 2010/01/02 16:58:11 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres_fe.h"

#include "parser/keywords.h"
#include "type.h"
#include "preproc.h"

#define PG_KEYWORD(a,b,c) {a,b,c},


const ScanKeyword ScanKeywords[] = {
#include "parser/kwlist.h"
};

const int	NumScanKeywords = lengthof(ScanKeywords);
