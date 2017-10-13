/*-------------------------------------------------------------------------
 *
 * ruleutils.h
 *		Declarations for ruleutils.c
 *
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/ruleutils.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RULEUTILS_H
#define RULEUTILS_H

#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/pg_list.h"


extern char *pg_get_indexdef_string(Oid indexrelid);
extern char *pg_get_indexdef_columns(Oid indexrelid, bool pretty);

extern char *pg_get_constraintdef_command(Oid constraintId);

extern char *deparse_expression(Node *expr, List *dpcontext,
				   bool forceprefix, bool showimplicit);

extern List *deparse_context_for(const char *aliasname, Oid relid);

extern List *deparse_context_for_plan_rtable(List *rtable, List *rtable_names);
extern List *deparse_context_for_plan_rtable_temp(List *rtable, List *rtable_names, List **temp);

extern List *set_deparse_context_planstate(List *dpcontext,
							  Node *planstate, List *ancestors);
extern List *set_deparse_context_planstate_temp(List *dpcontext,
							  Node *planstate, List *ancestors, List** temp);

extern List *select_rtable_names_for_explain(List *rtable,
								Bitmapset *rels_used);
extern List *select_rtable_names_for_explain_temp(List *rtable,
		Bitmapset *rels_used, List** temp);

extern char *generate_collation_name(Oid collid);

#endif   /* RULEUTILS_H */
