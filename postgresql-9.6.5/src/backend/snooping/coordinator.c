/*
                        NoDB Project 
        Query Processing On Raw Data Files using PostgresRAW

                   Copyright (c) 2011-2013
  Data Intensive Applications and Systems Labaratory (DIAS)
           Ecole Polytechnique Federale de Lausanne

                     All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright notice
and this permission notice appear in all copies of the software, derivative
works or modified versions, and any portions thereof, and that both notices
appear in supporting documentation.

This code is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. THE AUTHORS AND ECOLE POLYTECHNIQUE FEDERALE DE LAUSANNE
DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
USE OF THIS SOFTWARE.
*/
`
#include "postgres.h"

#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "commands/copy.h"
#include "commands/defrem.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "optimizer/planner.h"
#include "parser/parse_relation.h"
#include "rewrite/rewriteHandler.h"
#include "storage/fd.h"
#include "tcop/tcopprot.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "commands/explain.h"
#include "optimizer/clauses.h"


#include <time.h>

#include "snooping/coordinator.h"
#include "snooping/d_array.h"
//#include "snooping/inputFunctions.h"
#include "snooping/storageComponent.h"
#include "snooping/queryDescriptor.h"

#include "noDB/NoDBScan.h"

static const char BinarySignature[11] = "PGCOPY\n\377\r\n\0";

//
///*
// * Local functions
// */
//static CopyStmt* parseCopyQuery(const char *query_string);
//static CopyState generateCopyState(const CopyStmt *stmt, const char *queryString);
//static CopyStmtExecStatus prepareCopyFromFile(CopyState cstate);
//
//static List *CopyGetAttnums(TupleDesc tupDesc, Relation rel, List *attnamelist);
//static void ReceiveCopyBegin(CopyState cstate);
//
////static void copy_in_error_callback(void *arg);
////static char *limit_printout_length(const char *str);
//
//static List* traverseSubPlans(List *plans, PlannedStmt *top);
//static List* traverseMemberNodes(List *plans, PlanState **planstate, Plan *outer_plan, PlannedStmt *top);
//
//static void updateScanTarget(Scan *plan, PlanState *planstate, NodeTag oldTag, NodeTag newTag);
//static void updateSubPlans(List *plans, PlannedStmt *top, NodeTag oldTag, NodeTag newTag);
//static void updateMemberNodes(List *plans, PlanState **planstate, Plan *outer_plan, PlannedStmt *top, NodeTag oldTag, NodeTag newTag);
//static void traverseQual(List *qual, Plan *plan, Plan *outer_plan, PlannedStmt *topPlan);
//
//static void computeNextStep(int *stored, BitMap available, int maxAttr, int attribute, int *pointerID, int *directionVal, int* index);
//static void precomputeIndexAccesses2(int pos_ID, int numAtts,int numInterestingAtts, int *intAtts, bool *next_step, ParsingParameters *parameters, int *neededMapPositions, int*numOfneededMapPositions);
//
//
///* *
// * Initialization Variables
// * Enable: InvisibleDB + extra features
// * */
////bool enable_invisible_db 			= false;
////bool enable_tuple_metapointers  	= false;
////bool enable_internal_metapointers 	= false;
////bool enable_pushdown_select 		= false;
//
//
///* Execution information per relation () */
//CopyExecutionInfo CopyExec[NUMBER_OF_RELATIONS];
//int usedCopyExec;
//
//
//
//int
//findExecutionInfo(char *relation)
//{
//	int i = 0 ;
//	for( i = 0; i < NUMBER_OF_RELATIONS; i++)
//	{
//		if( strcmp (CopyExec[i].relation, relation) == 0)
//			return i;
//	}
//	return -1;
//}
//
//
//
//CopyExecutionInfo
//getExecutionInfo(int pos)
//{
//	return CopyExec[pos];
//}
//
//
//
///***********************************************/
////Functions to control global variables...
////"COPY <Relation> FROM '<path to filename>' WITH DELIMITER '<delimiter>';"
//void
//initializeRelation(char *relation, bool hasQual) //TODO: change + pos in case we are ready!!
//{
//	int pos, i, j;
//	char *filename, *command, *delimiter;
//
//	int* neededMetapointers;
//	int* neededColumns;
//
//	int *qual_attributes;
//
//	int *qualNeededMetapointers;
//	int numOfqualNeededMetapointers = 0;
//	int *parseNeededMetapointers;
//	int numOfparseNeededMetapointers = 0;
//	int *defaultNeededMetapointers;
//	int *interesting_attrPositions;
//
//	int needed_Point = 0;
////	int needed_Col;
//	int natts;
//	bool *next_step;
//	bool where_clause = false;
//
//	/* Extra check: Environment should be already loaded... */
//	Assert(isLoaded());
//	/* Make sure there is space for more relations*/
//	Assert(usedCopyExec <= NUMBER_OF_RELATIONS);
//	Assert(relation != NULL);
//
//
//	if(usedCopyExec == -1)
//	{
//		pos = 0;
//		usedCopyExec++;
//	}
//	else if(usedCopyExec != -1)
//	{
//		pos = findExecutionInfo(relation);
//		if(pos != -1 && CopyExec[pos].initialized == true)
//			return;
//		if ( pos == -1 )
//		{
//			usedCopyExec++;
//			pos = usedCopyExec;
//		}
//	}
//
//
//	filename = getInputFilename(relation);
//	delimiter = getDelimiter(relation);
//	Assert(filename != NULL);
//
//	if(delimiter == NULL)
//		*delimiter = ',';
//
//	/*
//	 * Prepare to access data file
//	 * Use source from copy command: "COPY <relation> FROM '<link to data file>' WITH DELIMITER 'delimiter'"
//	 * */
//	//256 chars for the command
//	command = (char*)palloc(MAX_COMMAND_SIZE*sizeof(char));
//	sprintf(command, "COPY %s FROM '%s' WITH DELIMITER '%s';",relation, filename, delimiter);
//	strcpy(CopyExec[pos].relation, relation);
//
//	CopyExec[pos].planCopyStmt = parseCopyQuery(command);
//	CopyExec[pos].cstate = generateCopyState(CopyExec[pos].planCopyStmt, command);
//	CopyExec[pos].status = prepareCopyFromFile(CopyExec[pos].cstate);
//	CopyExec[pos].initialized = true;
//
//	CopyExec[pos].cstate->tupleRead = 0;
//	CopyExec[pos].cstate->tupleReadMetapointers = 0;
//	CopyExec[pos].cstate->tupleStored = 0;
//
//	CopyExec[pos].cstate->filePointers_ID = -1;
//	CopyExec[pos].cstate->internalFilePointers_ID = -1;
//
//	CopyExec[pos].cstate->numOfInterestingAtt = 0;
//	CopyExec[pos].cstate->numOfneededMapPositions = 0;
//	CopyExec[pos].cstate->numOftoBeParsed = 0;
//	CopyExec[pos].cstate->pos = pos;
//	natts = CopyExec[pos].status.num_phys_attrs;
//
//
//	/* Used to temporally store new internal metapointers */
//	CopyExec[pos].cstate->temp_positions = (InternalMetaPointer*)malloc(natts*sizeof(InternalMetaPointer));
//
//	CopyExec[pos].cstate->attributes = (int*)malloc((natts + 1) * sizeof(int)); //Attributes - 1
//	CopyExec[pos].cstate->attributes[0] = -1;
//
//	/* Attributes accessed from the current query */
//	CopyExec[pos].cstate->interesting_attributes = (int*)malloc((natts + 1) * sizeof(int));
//	/* Map positions to be accessed during query processing */
//	CopyExec[pos].cstate->defaultneededMapPositions = (int*)malloc((natts + 1) * sizeof(int));
//	/* Map positions to be collected during query processing */
//	CopyExec[pos].cstate->neededMapPositions = (int*)malloc((natts + 1) * sizeof(int));
//	/* Precompute parsing steps */
//	CopyExec[pos].cstate->parameters = (ParsingParameters*) malloc (( 2 * natts) * sizeof(ParsingParameters));
//	CopyExec[pos].cstate->toBeParsed = (unsigned int*) malloc(( 2 * natts) * sizeof(unsigned int));
//
//
//
//	/* Initialize NoDB data structures (precompute all the steps) */
//
//	//Enable pointer in the end of each tuple (this should be activated by default)
//	if ( enable_tuple_metapointers )
//		CopyExec[pos].cstate->filePointers_ID = initializeTupleFilePointers(relation, NULL);
//
//	//List with all the interesting attributes (1 for YES, 0 for NO)
//	CopyExec[pos].cstate->numOfInterestingAtt =
//			getInterestingAttributes_V2(CopyExec[pos].cstate->interesting_attributes, CopyExec[pos].status.tupDesc, relation);
//
//	//List with attributes in filters ( used the qual optimization )
//	//Attributes in filters will be retrieved first and if the tuple qualifies the rest of the pointers
//	qual_attributes = getFilterAttributes(CopyExec[pos].status.tupDesc, &CopyExec[pos].cstate->numOfQualAtt,relation, hasQual);
//	if (CopyExec[pos].cstate->numOfQualAtt > 0)
//		where_clause = true;
//
//	// Initialize unused values and nulls ;-)
//	for(i = 0; i < natts; i++)
//	{
//		if(CopyExec[pos].cstate->interesting_attributes[i] != 1)
//		{
//			CopyExec[pos].cstate->values[i] = (Datum) 0;
//			CopyExec[pos].cstate->nulls[i] = true;
//		}
//		else
//			CopyExec[pos].cstate->nulls[i] = false;
//	}
//
//	/*
//	//For DEBUG
//	fprintf(stderr,"Relation: {%s}\n",relation);
//	fprintf(stderr,"\nInteresting attributes:{%d} { ",CopyExec[pos].cstate->numOfInterestingAtt);
//	for(i = 0; i < natts; i++)
//		fprintf(stderr,"%d ",CopyExec[pos].cstate->interesting_attributes[i]);
//	fprintf(stderr,"}\n");
//
//	fprintf(stderr,"Qual attributes:{%d}{ ",CopyExec[pos].cstate->numOfQualAtt);
//	for(i = 0; i < natts; i++)
//		fprintf(stderr,"%d ",qual_attributes[i]);
//	fprintf(stderr,"}\n\n");
//	 */
//
//
//	/*
//	 * Internal metapointers: adaptive mode
//	 * Algorithm:
//	 * First query: Parse to find end of tuple, then identify attributes
//	 * Following queries: First examine the set of available pointers and identify the extra pointers needed to answer the query
//	 * 	(BTW don't forget to collect them ;-)
//	 */
//
//	/*
//	 * Initialize parameters for the Storage component
//	 * --> Storage compoment must be in sync with the positional map
//	 * Positional map:
//	 * --> Passive mode: 	 If an attribute is cached then remove it from the list of interesting attributes
//	 * --> Collecting mode: If an attributed is cached we won't do any parsing for them (minimum effort)
//	 *
//	 * Query execution: Use cached attributes
//	 * newCachedElements;    --> attributes that will be collected (adaptive mode) -- if it is in the where clause then compute it
//	 * loadedElements;       --> attributes that will be loaded from the cache
//	 * where_loadedElements; --> attributes that will be loaded from the cache for the where clause
//	 */
//
//	/* Caching component */
//	if ( enable_caching )
//	{
//		/* Needed columns to answer a query */
//		neededColumns = copyList(CopyExec[pos].cstate->interesting_attributes, natts);
////		needed_Col = CopyExec[pos].cstate->numOfInterestingAtt;
//		CopyExec[pos].cstate->cache_ID = getCacheID(relation);
//
//		if ( !isInitializedRelCache(pos) ) {
//			/* Initialize data structures for cache component */
//			CopyExec[pos].cstate->cache_ID = initializeRelCache(CopyExec[pos].cstate->cache_ID, relation, NULL, natts, neededColumns);
//		}
//		else
//		{
//			/* Select columns to be cached + columns to be released */
//			computeWhichColumnsToCache(CopyExec[pos].cstate->cache_ID, neededColumns);
//			/* Organize which columns will be accessed from the cache */
//			//Update bitmap before examining which columns to cache ;-)
//			computeWhichColumnsToLoad(CopyExec[pos].cstate->cache_ID, neededColumns, qual_attributes);
//			/* Update internal structures in cache */
//			updateRelCache(CopyExec[pos].cstate->cache_ID, CopyExec[pos].status.attr);
//		}
//
//		/* If a column is cached then remove it from the interesting attributes ;-) */
//		updateInterestingAttributesUsingCache(CopyExec[pos].cstate->cache_ID,
//				CopyExec[pos].cstate->interesting_attributes, &CopyExec[pos].cstate->numOfInterestingAtt);
//	}
//
//	/* Internal metapointers component */
//	if ( enable_internal_metapointers )
//	{
//		CopyExec[pos].cstate->internalFilePointers_ID = getPositionalMapID(relation);
//
//		if( CopyExec[pos].cstate->numOfInterestingAtt > 0 )
//		{
//			j = 0;
//			interesting_attrPositions = (int*)malloc((CopyExec[pos].cstate->numOfInterestingAtt) * sizeof(int));
//			for(i = 0; i < natts; i++)
//				if(CopyExec[pos].cstate->interesting_attributes[i] == 1)
//					interesting_attrPositions[j++] = i;
//		}
//
//		/* List with the pointers needed in order to answer the query */
//		//Policy: Collect pointers before and after attribute
//		neededMetapointers = getNeededInterestingMetapointersBoth(CopyExec[pos].cstate->interesting_attributes, natts, &needed_Point);
////		neededMetapointers = getNeededInterestingMetapointersBefore(CopyExec[pos].cstate->interesting_attributes, CopyExec[pos].status.tupDesc->natts, &needed_Point);
////		neededMetapointers = getNeededInterestingMetapointersAfter(CopyExec[pos].cstate->interesting_attributes, CopyExec[pos].status.tupDesc->natts, &needed_Point);
//
//		if ( !isInitializedInternalMapMetaPointers(pos) )
//		{
//			/* Initialize positionalMap */
//			CopyExec[pos].cstate->internalFilePointers_ID =
//					initializeInternalPositionalMap(CopyExec[pos].cstate->internalFilePointers_ID, relation, NULL, natts, neededMetapointers);
//
//			//If the cache is enabled and there is where clause then the initial list should be updated
//			if ( enable_caching && where_clause)
//				updateAttributeListUsingCache(CopyExec[pos].cstate->cache_ID,
//						CopyExec[pos].cstate->interesting_attributes, &CopyExec[pos].cstate->numOfInterestingAtt,
//						qual_attributes, &CopyExec[pos].cstate->numOfQualAtt);
//		}
//		else
//		{
//			/* Organize the pointers we are going to collect */
//			computeWhichPointersToCollect(CopyExec[pos].cstate->internalFilePointers_ID, getBitMap(pos), natts, neededMetapointers);
//
//			/* List of the positions of available pointers to be retrieved from the index */
//			getAvailableInterestingMetapointers(CopyExec[pos].cstate->neededMapPositions, neededMetapointers, natts,
//					getBitMap(pos), &CopyExec[pos].cstate->numOfneededMapPositions);
//
//			/* Update InternalMap structures */
//			updateInternalPositionalMap(CopyExec[pos].cstate->internalFilePointers_ID);
//
//			/* We have interesting attributes ==> Plan ahead for the execution steps*/
//			if( CopyExec[pos].cstate->numOfInterestingAtt > 0)
//			{
//				next_step = (bool*) malloc(( 2 * CopyExec[pos].cstate->numOfInterestingAtt) * sizeof(bool));
//				for (i = 0 ; i < ( 2 * CopyExec[pos].cstate->numOfInterestingAtt); i++)
//					next_step[i] = false;
//				/*
//				 * Precompute future steps for parsing
//				 * 1) Consider available pointers in positional map
//				 * 2) Decide which attributes will be retrieved with parsing
//				 */
//				precomputeIndexAccesses2(CopyExec[pos].cstate->internalFilePointers_ID, natts,
//						CopyExec[pos].cstate->numOfInterestingAtt,
//						interesting_attrPositions,
//						next_step,
//						CopyExec[pos].cstate->parameters,
//						CopyExec[pos].cstate->neededMapPositions,
//						&CopyExec[pos].cstate->numOfneededMapPositions);
//
//				/* List of parsing during query execution --> the needed pointer should have been retrived from the index before that */
//				CopyExec[pos].cstate->numOftoBeParsed = 0;
//				j = 0;
//				for (i = 0 ; i < ( 2 * CopyExec[pos].cstate->numOfInterestingAtt ); i++)
//				{
//					if (next_step[i] == true)
//					{
//						CopyExec[pos].cstate->toBeParsed[j++] = i;
//						CopyExec[pos].cstate->numOftoBeParsed++;
//					}
//				}
//
//				/* Attributes in the filter */
//				if( CopyExec[pos].cstate->numOfQualAtt > 0 ) // || CopyExec[pos].cstate->numOftoBeParsed > 0)
//				{
//					if ( enable_caching && where_clause)
//						updateAttributeListUsingCache(CopyExec[pos].cstate->cache_ID,
//								CopyExec[pos].cstate->interesting_attributes, &CopyExec[pos].cstate->numOfInterestingAtt,
//								qual_attributes,&CopyExec[pos].cstate->numOfQualAtt);
//
//					//Available pointers needed for the attributes in the where clause
//					qualNeededMetapointers = getQualAvailableMetapointers(qual_attributes, natts, getBitMap(pos), &numOfqualNeededMetapointers);
//					//Pointers needed for the parsing --> This list includes all the pointers needed either to collect (new) pointers or to answer the qual part
//					parseNeededMetapointers = getParseNeededMetapointers(CopyExec[pos].cstate->numOftoBeParsed, CopyExec[pos].cstate->toBeParsed, natts,
//							CopyExec[pos].cstate->parameters, &numOfparseNeededMetapointers);
//
//					//Merge the two previous lists (Pointers )
//					defaultNeededMetapointers = mergeLists(qualNeededMetapointers, parseNeededMetapointers,
//							natts, &CopyExec[pos].cstate->numOfdefaultneededMapPositions);
//
//					//List with positions needed by default to answer a query
//					j = 0;
//					for(i = 0; i <= natts; i++)
//						CopyExec[pos].cstate->defaultneededMapPositions[i] = -1;
//					for(i = 0; i <= natts; i++)
//						if(defaultNeededMetapointers[i] == 1)
//							CopyExec[pos].cstate->defaultneededMapPositions[j++] = i;
//
//					//Update neededMapPositions considering defaultneededMapPositions (for qual and parsing)
//					CopyExec[pos].cstate->neededMapPositions = updateMetapointerLists(CopyExec[pos].cstate->neededMapPositions, CopyExec[pos].cstate->numOfneededMapPositions,
//							CopyExec[pos].cstate->defaultneededMapPositions, CopyExec[pos].cstate->numOfdefaultneededMapPositions,natts);
//					CopyExec[pos].cstate->numOfneededMapPositions -= CopyExec[pos].cstate->numOfdefaultneededMapPositions;
//
//					//free
//					free(defaultNeededMetapointers);
//					defaultNeededMetapointers = NULL;
//
//					free(parseNeededMetapointers);
//					parseNeededMetapointers = NULL;
//
//					free(qualNeededMetapointers);
//					qualNeededMetapointers = NULL;
//				}
//				//free
//				free(next_step);
//				next_step = NULL;
//			}
//
//			//free
//			free(neededMetapointers);
//			neededMetapointers = NULL;
//
//			if( CopyExec[pos].cstate->numOfInterestingAtt > 0 )
//			{
//				free(interesting_attrPositions);
//				interesting_attrPositions = NULL;
//			}
//		}
//	}
//
//	if ( enable_caching && where_clause)
//		updateAttributeListUsingCache(CopyExec[pos].cstate->cache_ID,
//				CopyExec[pos].cstate->interesting_attributes, &CopyExec[pos].cstate->numOfInterestingAtt,
//				qual_attributes,&CopyExec[pos].cstate->numOfQualAtt);
//
//	if ( !enable_caching && where_clause && enable_internal_metapointers)
//		updateAttributeListUsingPositionaMap(CopyExec[pos].cstate->filePointers_ID,
//				CopyExec[pos].cstate->interesting_attributes, &CopyExec[pos].cstate->numOfInterestingAtt,
//				qual_attributes);
//
//
///*
//	//For DEBUG
//	fprintf(stderr,"\nInteresting attributes:{%d} { ",CopyExec[pos].cstate->numOfInterestingAtt);
//	for(i = 0; i < natts; i++)
//		fprintf(stderr,"%d ",CopyExec[pos].cstate->interesting_attributes[i]);
//	fprintf(stderr,"}\n");
//
//	fprintf(stderr,"Qual attributes:{%d}{ ",CopyExec[pos].cstate->numOfQualAtt);
//	for(i = 0; i < natts; i++)
//		fprintf(stderr,"%d ",qual_attributes[i]);
//	fprintf(stderr,"}\n\n");
//
////	fprintf(stderr,"\nNeededMapPositionss:{%d} { ",CopyExec[pos].cstate->numOfneededMapPositions);
////	for(i = 0; i < natts; i++)
////		fprintf(stderr,"%d ",CopyExec[pos].cstate->neededMapPositions[i]);
////	fprintf(stderr,"}\n");
////
////	fprintf(stderr,"Qual attributes:{%d}{ ",CopyExec[pos].cstate->numOfdefaultneededMapPositions);
////	for(i = 0; i < natts; i++)
////		fprintf(stderr,"%d ",CopyExec[pos].cstate->defaultneededMapPositions[i]);
////	fprintf(stderr,"}\n\n");
//*/
//
//
//	precomputeDataTransformations(pos, CopyExec[pos].cstate->fcinfo, CopyExec[pos].cstate->interesting_attributes, CopyExec[pos].cstate->numOfInterestingAtt, natts);
//	precomputeQualDataTransformations(pos, CopyExec[pos].cstate->fcinfo, qual_attributes, CopyExec[pos].cstate->numOfQualAtt, natts);
//
//	prepareGlobalPositionalMapInfo(CopyExec[pos].cstate->internalFilePointers_ID,
//			CopyExec[pos].cstate->neededMapPositions, CopyExec[pos].cstate->numOfneededMapPositions,
//			CopyExec[pos].cstate->defaultneededMapPositions, CopyExec[pos].cstate->numOfdefaultneededMapPositions);
//	prepareCollectPositionalMapInfo(CopyExec[pos].cstate->internalFilePointers_ID);
//
//
//	if ( CopyExec[pos].cstate->numOfInterestingAtt == 0 && CopyExec[pos].cstate->numOfQualAtt == 0 )
//	{
//		if(enable_caching && isRelCacheReady(CopyExec[pos].cstate->cache_ID))
//			CopyExec[pos].status.loadFromCacheOnly = true;
//		else if (getNumberOfTuples(CopyExec[pos].cstate->filePointers_ID) > 0)
//			//No interesting attributes to load and no cache enabled ;-) but only after colelcting the end-of-tuple pointers
//			CopyExec[pos].status.loadWithoutInteresting = true;
//	}
//	free(qual_attributes);
//	qual_attributes = NULL;
//
////		if ( enable_caching )
////			printRelCacheTotal(CopyExec[pos].cstate->cache_ID);
////		if ( enable_internal_metapointers )
////			printPositionalMapTotal(CopyExec[pos].cstate->internalFilePointers_ID);
////		printCopyStateData(pos);
////		printQueryDescriptor();
//
//}
//
//
///*
// * Same as above:
// * Re-initialize the relation in case we scan the same relation and in the initial scan pointers have been collected
// * TODO: It is needed when there is difference from the previous execution
// */
//void
//reInitRelation(CopyState cstate, CopyStmtExecStatus status)
//{
//	int i, j;
//
//	int* neededMetapointers;
//	int* neededColumns;
//
//	int *qual_attributes;
//
//	int *qualNeededMetapointers;
//	int numOfqualNeededMetapointers = 0;
//	int *parseNeededMetapointers;
//	int numOfparseNeededMetapointers = 0;
//	int *defaultNeededMetapointers;
//	int *interesting_attrPositions;
//
//	int needed_Point = 0;
////	int needed_Col = 0;
//	int natts;
//	bool *next_step;
//	bool where_clause = false;
//
//	cstate->tupleRead = 0;
//	cstate->tupleReadMetapointers = 0;
//	cstate->tupleStored = 0;
//	natts = status.num_phys_attrs;
//
//	restartFilePointer(cstate);
//	if( enable_internal_metapointers )
//		updateInternalPositionalMapStatus2(cstate->internalFilePointers_ID);
//	if ( enable_caching )
//		updateRelCacheStatus2(cstate->cache_ID);
//
//
//	cstate->numOfInterestingAtt = getInterestingAttributes_V2(cstate->interesting_attributes, status.tupDesc, cstate->rel->rd_rel->relname.data);
//
//	qual_attributes = getFilterAttributes(status.tupDesc, &cstate->numOfQualAtt, cstate->rel->rd_rel->relname.data, true);
//	if (cstate->numOfQualAtt > 0)
//		where_clause = true;
//
//	/* Examine caching */
//	if ( enable_caching )
//	{
//		/*Needed columns to answer a query*/
//		neededColumns = copyList(cstate->interesting_attributes, natts);
////		needed_Col = cstate->numOfInterestingAtt;
//
//		/* Select columns to be cached + columns to be released */
//		computeWhichColumnsToCache(cstate->cache_ID, neededColumns);
//		/* Organize which columns will be accessed from the caceh*/
//		//Update bitmap before examining which columns to cache
//		computeWhichColumnsToLoad(cstate->cache_ID, neededColumns, qual_attributes);
//		/* Update internal structures in Cache*/
//		updateRelCache(cstate->cache_ID, status.attr);
//		/* If a column is cached then remove it from the interesting attributes */
//		updateInterestingAttributesUsingCache(cstate->cache_ID,
//				cstate->interesting_attributes, &cstate->numOfInterestingAtt);
//	}
//
//	if ( enable_internal_metapointers )
//	{
//		if( cstate->numOfInterestingAtt > 0 )
//		{
//			j = 0;
//			interesting_attrPositions = (int*)malloc(cstate->numOfInterestingAtt * sizeof(int));
//			for(i = 0; i < natts; i++)
//				if(cstate->interesting_attributes[i] == 1)
//					interesting_attrPositions[j++] = i;
//		}
//
//		/* List with the pointers needed in order to answer the query */
//		//Policy: Collect pointers before and after attribute
//		neededMetapointers = getNeededInterestingMetapointersBoth(cstate->interesting_attributes, natts, &needed_Point);
////		neededMetapointers = getNeededInterestingMetapointersBefore(CopyExec[pos].cstate->interesting_attributes, CopyExec[pos].status.tupDesc->natts, &needed_Point);
////		neededMetapointers = getNeededInterestingMetapointersAfter(CopyExec[pos].cstate->interesting_attributes, CopyExec[pos].status.tupDesc->natts, &needed_Point);
//
//
//		/* Organize the pointers we are going to collect */
//		computeWhichPointersToCollect(cstate->internalFilePointers_ID, getBitMap(cstate->internalFilePointers_ID), natts, neededMetapointers);
//		/* List of the positions of available pointers to be retrieved from the index */
//		getAvailableInterestingMetapointers(cstate->neededMapPositions, neededMetapointers, natts,
//				getBitMap(cstate->internalFilePointers_ID), &cstate->numOfneededMapPositions);
//		/* Update InternalMap structures */
//		updateInternalPositionalMap(cstate->internalFilePointers_ID);
//
//		/* We have interesting attributes ==> Plan ahead for the execution steps*/
//		if( cstate->numOfInterestingAtt > 0)
//		{
//			next_step = (bool*) malloc(( 2 * cstate->numOfInterestingAtt) * sizeof(bool));
//			for (i = 0 ; i < ( 2 * cstate->numOfInterestingAtt); i++)
//				next_step[i] = false;
//			/* Precompute future steps */
//			precomputeIndexAccesses2(cstate->internalFilePointers_ID, natts,
//					cstate->numOfInterestingAtt,
//					interesting_attrPositions,
//					next_step,
//					cstate->parameters,
//					cstate->neededMapPositions,
//					&cstate->numOfneededMapPositions);
//
//			/* List of parsing during query execution --> the needed pointer should have been retrived from the index before that */
//			cstate->numOftoBeParsed = 0;
//			j = 0;
//			for (i = 0 ; i < ( 2 * cstate->numOfInterestingAtt); i++)
//			{
//				if (next_step[i] == true)
//				{
//					cstate->toBeParsed[j++] = i;
//					cstate->numOftoBeParsed++;
//				}
//			}
//
//			if( cstate->numOfQualAtt > 0 ) // || CopyExec[pos].cstate->numOftoBeParsed > 0)
//			{
//				if ( enable_caching && where_clause)
//					updateAttributeListUsingCache(cstate->cache_ID,
//							cstate->interesting_attributes, &cstate->numOfInterestingAtt,
//							qual_attributes,&cstate->numOfQualAtt);
//
//				//Available pointers needed for the attributes in the where clause
//				qualNeededMetapointers = getQualAvailableMetapointers(qual_attributes, natts, getBitMap(cstate->internalFilePointers_ID), &numOfqualNeededMetapointers);
//				//Pointers needed for the parsing --> This list includes all the pointers needed either to collect (new) pointers or to answer the qual part
//				parseNeededMetapointers = getParseNeededMetapointers(cstate->numOftoBeParsed, cstate->toBeParsed, natts,
//						cstate->parameters, &numOfparseNeededMetapointers);
//
//				//Merge the two previous lists (Pointers )
//				defaultNeededMetapointers = mergeLists(qualNeededMetapointers, parseNeededMetapointers,
//						natts, &cstate->numOfdefaultneededMapPositions);
//
//				//List with positions needed by default to answer a query
//				j = 0;
//				for(i = 0; i <= natts; i++)
//					cstate->defaultneededMapPositions[i] = -1;
//				for(i = 0; i <= natts; i++)
//					if(defaultNeededMetapointers[i] == 1)
//						cstate->defaultneededMapPositions[j++] = i;
//
//				//Update neededMapPositions considering defaultneededMapPositions (for qual and parsing)
//				cstate->neededMapPositions = updateMetapointerLists(cstate->neededMapPositions, cstate->numOfneededMapPositions,
//						cstate->defaultneededMapPositions, cstate->numOfdefaultneededMapPositions,natts);
//						cstate->numOfneededMapPositions -= cstate->numOfdefaultneededMapPositions;
//
//				free(defaultNeededMetapointers);
//				defaultNeededMetapointers = NULL;
//
//				free(parseNeededMetapointers);
//				parseNeededMetapointers = NULL;
//
//				free(qualNeededMetapointers);
//				qualNeededMetapointers = NULL;
//			}
//			//free
//			free(next_step);
//			next_step = NULL;
//		}
//
//		//free
//		free(neededMetapointers);
//		neededMetapointers = NULL;
//
//		if( cstate->numOfInterestingAtt > 0 )
//		{
//			free(interesting_attrPositions);
//			interesting_attrPositions = NULL;
//		}
//	}
//
//	if ( enable_caching && where_clause)
//		updateAttributeListUsingCache(cstate->cache_ID,
//				cstate->interesting_attributes, &cstate->numOfInterestingAtt,
//				qual_attributes,&cstate->numOfQualAtt);
//
//	if ( !enable_caching && where_clause && enable_internal_metapointers)
//		updateAttributeListUsingPositionaMap(cstate->filePointers_ID,
//				cstate->interesting_attributes, &cstate->numOfInterestingAtt,
//				qual_attributes);
//
//
//	precomputeDataTransformations(cstate->pos, cstate->fcinfo, cstate->interesting_attributes, cstate->numOfInterestingAtt, natts);
//	precomputeQualDataTransformations(cstate->pos, cstate->fcinfo, qual_attributes, cstate->numOfQualAtt, natts);
//
//	prepareGlobalPositionalMapInfo(cstate->internalFilePointers_ID,
//			cstate->neededMapPositions, cstate->numOfneededMapPositions,
//			cstate->defaultneededMapPositions, cstate->numOfdefaultneededMapPositions);
//	prepareCollectPositionalMapInfo(cstate->internalFilePointers_ID);
//
//
//	if ( cstate->numOfInterestingAtt == 0 && cstate->numOfQualAtt == 0 )
//	{
//		if(enable_caching && isRelCacheReady(cstate->cache_ID))
//			status.loadFromCacheOnly = true;
//		else if (getNumberOfTuples(cstate->filePointers_ID) > 0)
//			//No interesting attributes to load and no cache enabled ;-) but only after colelcting the end-of-tuple pointers
//			status.loadWithoutInteresting = true;
//	}
//
//	free(qual_attributes);
//	qual_attributes = NULL;
//}
//
//
//
///*
// * Initialize Loading Module
// */
//void
//initializeLoadModule(void)
//{
//	int i;
//	usedCopyExec = -1;
//	Assert(isLoaded());
//	for( i = 0; i < NUMBER_OF_RELATIONS; i++)
//	{
//		CopyExec[i].initialized = false;
//		strcpy(CopyExec[i].relation, "\0");
//		/* Initialize IntegrityCheck structuer */
//		CopyExec[i].IntegrityCheck.firstTime = true;
//		CopyExec[i].IntegrityCheck.disable_varcharin = false;
//		CopyExec[i].IntegrityCheck.disable_int4in = false;
//		CopyExec[i].IntegrityCheck.disable_need_transcoding = false;
//	}
//}
//
//
//void
//restartFilePointer(CopyState cstate)
//{
//	if(cstate->copy_file != NULL) {
//		 fseek(cstate->copy_file, 0, SEEK_SET);
//	}
//}
//
//
///*
// * Reset Loading Module
// */
//void
//resetLoadModule(char *relation)
//{
//	int pos = findExecutionInfo(relation);
//	if(pos == -1)
//		return;
//	CopyExec[pos].initialized = false;
//
////	if(CopyExec[pos].cstate->available != NULL)
////		freeBitMap(CopyExec[pos].cstate->available);
//
//	/*Disable Integrity Check after the first query*/
//	if( CopyExec[pos].IntegrityCheck.firstTime  )
//	{
//		CopyExec[pos].IntegrityCheck.firstTime = false;
//		CopyExec[pos].IntegrityCheck.disable_varcharin = true;
//		CopyExec[pos].IntegrityCheck.disable_int4in = true;
//		CopyExec[pos].IntegrityCheck.disable_need_transcoding = true;
//	}
//
//
//	freeCopyStmtExecutionStatus(relation);
//	freeCopyState(relation);
//
////	strcpy(CopyExec[pos].relation,"");
////	usedCopyExec--;
//
//}
//
//
//
///*
// * freeCopyState: Free struct CopyState based on input data
// */
//void
//freeCopyState(char *relation)
//{
//	int pos = findExecutionInfo(relation);
//	Assert(pos	!= -1);
////	int processed = 0;
////	processed = cstate->processed;
////	elog(INFO, "Processed tuples: %d",processed);
//
//	pfree(CopyExec[pos].cstate->fcinfo);
//	pfree(CopyExec[pos].cstate->values);
//	pfree(CopyExec[pos].cstate->nulls);
//	pfree(CopyExec[pos].cstate->field_strings);
//
//	free(CopyExec[pos].cstate->attributes);
//	CopyExec[pos].cstate->attributes = NULL;
//
//	free(CopyExec[pos].cstate->temp_positions);
//	CopyExec[pos].cstate->temp_positions = NULL;
//
//	free(CopyExec[pos].cstate->interesting_attributes);
//	CopyExec[pos].cstate->interesting_attributes = NULL;
//
//	free(CopyExec[pos].cstate->neededMapPositions);
//	CopyExec[pos].cstate->neededMapPositions = NULL;
//
//	free(CopyExec[pos].cstate->defaultneededMapPositions);
//	CopyExec[pos].cstate->defaultneededMapPositions = NULL;
//
//	free(CopyExec[pos].cstate->toBeParsed);
//	CopyExec[pos].cstate->toBeParsed = NULL;
//
//	free(CopyExec[pos].cstate->parameters);
//	CopyExec[pos].cstate->parameters = NULL;
//
//	pfree(CopyExec[pos].cstate->attribute_buf.data);
//	pfree(CopyExec[pos].cstate->line_buf.data);
//	free(CopyExec[pos].cstate->raw_buf);
//	pfree(CopyExec[pos].cstate);
//}
//
//
//
///*
// * Free freeCopyStmtExecutionStatus struct (CopyState struct needed...)
// */
//void
//freeCopyStmtExecutionStatus(char *relation)
//{
//	int pos = findExecutionInfo(relation);
//	Assert(pos	!= -1);
//
//	error_context_stack = CopyExec[pos].status.errcontext.previous;
//
////	MemoryContextSwitchTo(CopyExec[pos].status.oldcontext);
//
//	/* Execute AFTER STATEMENT insertion triggers */
//	ExecASInsertTriggers(CopyExec[pos].status.estate, CopyExec[pos].status.resultRelInfo);
//
//	/* Handle queued AFTER triggers */
//	AfterTriggerEndQuery(CopyExec[pos].status.estate);
//
//
//	pfree(CopyExec[pos].status.in_functions);
//	pfree(CopyExec[pos].status.typioparams);
//	pfree(CopyExec[pos].status.defmap);
//	pfree(CopyExec[pos].status.defexprs);
//
//	ExecResetTupleTable(CopyExec[pos].status.estate->es_tupleTable, false);
//	ExecCloseIndices(CopyExec[pos].status.resultRelInfo);
//
//	//No need to free...
////	FreeExecutorState(CopyExec[pos].status.estate);
//
//	if (!(CopyExec[pos].cstate->filename == NULL))
//	{
//		if (FreeFile(CopyExec[pos].cstate->copy_file))
//			ereport(ERROR,
//					(errcode_for_file_access(),
//					 errmsg("could not read from file \"%s\": %m",
//							 CopyExec[pos].cstate->filename)));
//	}
//
//	/*
//	 * If we skipped writing WAL, then we need to sync the heap (but not
//	 * indexes since those use WAL anyway)
//	 */
//	if (CopyExec[pos].status.hi_options & HEAP_INSERT_SKIP_WAL)
//		heap_sync(CopyExec[pos].cstate->rel);
//
//}
//
//
//
///*
// *	Create CopyStmt struct for Copy query: query_string
// */
//CopyStmt *
//parseCopyQuery(const char *query_string)
//{
////	MemoryContext oldcontext;
//	List	   *parsetree_list = NIL;
//	List 	   *plantree_list  = NIL;
//	ListCell   *parsetree_item;
//	int length = 0;
//
////	oldcontext = MemoryContextSwitchTo(MessageContext);
//	/*
//	 * Do basic parsing of the query or queries (this should be safe even if
//	 * we are in aborted transaction state!)
//	 */
//	parsetree_list = pg_parse_query(query_string);
//
//	/*
//	 * Switch back to transaction context to enter the loop.
//	 */
////	MemoryContextSwitchTo(oldcontext);
//
//	/*
//	 * Run through the raw parsetree and process each it.
//	 */
//	length = list_length(parsetree_list);
//	if( length != 1)
//	{
//		elog(INFO, "More than one raw parsed trees returned...");
//		return NULL;
//	}
//
//	foreach(parsetree_item, parsetree_list)
//	{
//		Node	   *parsetree = (Node *) lfirst(parsetree_item);
//		List	   *querytree_list;
//
////		oldcontext = MemoryContextSwitchTo(MessageContext);
//
//		querytree_list = pg_analyze_and_rewrite(parsetree, query_string,
//												NULL, 0);
//
//		plantree_list = pg_plan_queries(querytree_list, 0, NULL);
//	}
//
////	parsetree = (Node *) list_head(parsetree_list);
////	oldcontext = MemoryContextSwitchTo(MessageContext);
////	querytree_list = pg_analyze_and_rewrite(parsetree, query_string, NULL, 0);
////	plantree_list = pg_plan_queries(querytree_list, 0, NULL);
//
//	if(!IsA(((Node *) linitial(plantree_list)), CopyStmt))
//	{
//		fprintf(stderr, "Wrong type of plan...");
//		return NULL;
//	}
//	return (CopyStmt*) linitial(plantree_list);
//}
//
//
///*
// * SnoopDB: We care only for the case of "delimiter" (However, I did not remove the other checks..,)
// * Initialized once...
// */
//CopyState
//generateCopyState(const CopyStmt *stmt, const char *queryString)
//{
//	CopyState	cstate;
//	bool		is_from = stmt->is_from;
//	bool		pipe = (stmt->filename == NULL);
//	List	   *attnamelist = stmt->attlist;
//	List	   *force_quote = NIL;
//	List	   *force_notnull = NIL;
//	bool		force_quote_all = false;
//	bool		format_specified = false;
//	AclMode		required_access = (is_from ? ACL_INSERT : ACL_SELECT);
//	AclMode		relPerms;
//	AclMode		remainingPerms;
//	ListCell   *option;
//	TupleDesc	tupDesc;
////	int			num_phys_attrs;
//
//	/* Allocate workspace and zero all fields */
//	cstate = (CopyStateData *) palloc0(sizeof(CopyStateData));
//	/* Extract options from the statement node tree */
//	foreach(option, stmt->options)
//	{
//		DefElem    *defel = (DefElem *) lfirst(option);
//
//		if (strcmp(defel->defname, "format") == 0)
//		{
//			char	   *fmt = defGetString(defel);
//
//			if (format_specified)
//				ereport(ERROR,
//						(errcode(ERRCODE_SYNTAX_ERROR),
//						 errmsg("conflicting or redundant options")));
//			format_specified = true;
//			if (strcmp(fmt, "text") == 0)
//				 /* default format */ ;
////			else if (strcmp(fmt, "csv") == 0)
////				cstate->csv_mode = true;
//			else if (strcmp(fmt, "binary") == 0)
//				cstate->binary = true;
//			else
//				ereport(ERROR,
//						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
//						 errmsg("COPY format \"%s\" not recognized", fmt)));
//		}
////		else if (strcmp(defel->defname, "oids") == 0)
////		{
////			if (cstate->oids)
////				ereport(ERROR,
////						(errcode(ERRCODE_SYNTAX_ERROR),
////						 errmsg("conflicting or redundant options")));
////			cstate->oids = defGetBoolean(defel);
////		}
//		else if (strcmp(defel->defname, "delimiter") == 0)
//		{
//			if (cstate->delim)
//				ereport(ERROR,
//						(errcode(ERRCODE_SYNTAX_ERROR),
//						 errmsg("conflicting or redundant options")));
//			cstate->delim = defGetString(defel);
//		}
//		else if (strcmp(defel->defname, "null") == 0)
//		{
//			if (cstate->null_print)
//				ereport(ERROR,
//						(errcode(ERRCODE_SYNTAX_ERROR),
//						 errmsg("conflicting or redundant options")));
//			cstate->null_print = defGetString(defel);
//		}
////		else if (strcmp(defel->defname, "header") == 0)
////		{
////			if (cstate->header_line)
////				ereport(ERROR,
////						(errcode(ERRCODE_SYNTAX_ERROR),
////						 errmsg("conflicting or redundant options")));
////			cstate->header_line = defGetBoolean(defel);
////		}
////		else if (strcmp(defel->defname, "quote") == 0)
////		{
////			if (cstate->quote)
////				ereport(ERROR,
////						(errcode(ERRCODE_SYNTAX_ERROR),
////						 errmsg("conflicting or redundant options")));
////			cstate->quote = defGetString(defel);
////		}
////		else if (strcmp(defel->defname, "escape") == 0)
////		{
////			if (cstate->escape)
////				ereport(ERROR,
////						(errcode(ERRCODE_SYNTAX_ERROR),
////						 errmsg("conflicting or redundant options")));
////			cstate->escape = defGetString(defel);
////		}
//		else if (strcmp(defel->defname, "force_quote") == 0)
//		{
//			if (force_quote || force_quote_all)
//				ereport(ERROR,
//						(errcode(ERRCODE_SYNTAX_ERROR),
//						 errmsg("conflicting or redundant options")));
//			if (defel->arg && IsA(defel->arg, A_Star))
//				force_quote_all = true;
//			else if (defel->arg && IsA(defel->arg, List))
//				force_quote = (List *) defel->arg;
//			else
//				ereport(ERROR,
//						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
//						 errmsg("argument to option \"%s\" must be a list of column names",
//								defel->defname)));
//		}
//		else if (strcmp(defel->defname, "force_not_null") == 0)
//		{
//			if (force_notnull)
//				ereport(ERROR,
//						(errcode(ERRCODE_SYNTAX_ERROR),
//						 errmsg("conflicting or redundant options")));
//			if (defel->arg && IsA(defel->arg, List))
//				force_notnull = (List *) defel->arg;
//			else
//				ereport(ERROR,
//						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
//						 errmsg("argument to option \"%s\" must be a list of column names",
//								defel->defname)));
//		}
//		else
//			ereport(ERROR,
//					(errcode(ERRCODE_SYNTAX_ERROR),
//					 errmsg("option \"%s\" not recognized",
//							defel->defname)));
//	}
//
//	/*
//	 * Check for incompatible options (must do these two before inserting
//	 * defaults)
//	 */
//	if (cstate->binary && cstate->delim)
//		ereport(ERROR,
//				(errcode(ERRCODE_SYNTAX_ERROR),
//				 errmsg("cannot specify DELIMITER in BINARY mode")));
//
//	if (cstate->binary && cstate->null_print)
//		ereport(ERROR,
//				(errcode(ERRCODE_SYNTAX_ERROR),
//				 errmsg("cannot specify NULL in BINARY mode")));
//
//	/* Set defaults for omitted options */
//	if (!cstate->delim)
//		cstate->delim =  "\t";
////		cstate->delim = cstate->csv_mode ? "," : "\t";
//
//	if (!cstate->null_print)
//		cstate->null_print = "\\N";
////		cstate->null_print = cstate->csv_mode ? "" : "\\N";
//	cstate->null_print_len = strlen(cstate->null_print);
//
////	if (cstate->csv_mode)
////	{
////		if (!cstate->quote)
////			cstate->quote = "\"";
////		if (!cstate->escape)
////			cstate->escape = cstate->quote;
////	}
//
//	/* Only single-byte delimiter strings are supported. */
//	if (strlen(cstate->delim) != 1)
//		ereport(ERROR,
//				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//			  errmsg("COPY delimiter must be a single one-byte character")));
//
//	/* Disallow end-of-line characters */
//	if (strchr(cstate->delim, '\r') != NULL ||
//		strchr(cstate->delim, '\n') != NULL)
//		ereport(ERROR,
//				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
//			 errmsg("COPY delimiter cannot be newline or carriage return")));
//
//	if (strchr(cstate->null_print, '\r') != NULL ||
//		strchr(cstate->null_print, '\n') != NULL)
//		ereport(ERROR,
//				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
//				 errmsg("COPY null representation cannot use newline or carriage return")));
//
//	/*
//	 * Disallow unsafe delimiter characters in non-CSV mode.  We can't allow
//	 * backslash because it would be ambiguous.  We can't allow the other
//	 * cases because data characters matching the delimiter must be
//	 * backslashed, and certain backslash combinations are interpreted
//	 * non-literally by COPY IN.  Disallowing all lower case ASCII letters is
//	 * more than strictly necessary, but seems best for consistency and
//	 * future-proofing.  Likewise we disallow all digits though only octal
//	 * digits are actually dangerous.
//	 */
////	if (!cstate->csv_mode &&
//	if (strchr("\\.abcdefghijklmnopqrstuvwxyz0123456789",
//			   cstate->delim[0]) != NULL)
//		ereport(ERROR,
//				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
//				 errmsg("COPY delimiter cannot be \"%s\"", cstate->delim)));
//
//	/* Check header */
////	if (!cstate->csv_mode && cstate->header_line)
////	if (cstate->header_line)
////		ereport(ERROR,
////				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
////				 errmsg("COPY HEADER available only in CSV mode")));
//
//	/* Check quote */
////	if (!cstate->csv_mode && cstate->quote != NULL)
////	if (cstate->quote != NULL)
////		ereport(ERROR,
////				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
////				 errmsg("COPY quote available only in CSV mode")));
//
////	if (cstate->csv_mode && strlen(cstate->quote) != 1)
////		ereport(ERROR,
////				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
////				 errmsg("COPY quote must be a single one-byte character")));
//
////	if (cstate->csv_mode && cstate->delim[0] == cstate->quote[0])
////		ereport(ERROR,
////				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
////				 errmsg("COPY delimiter and quote must be different")));
//
//	/* Check escape */
////	if (!cstate->csv_mode && cstate->escape != NULL)
////	if (cstate->escape != NULL)
////		ereport(ERROR,
////				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
////				 errmsg("COPY escape available only in CSV mode")));
//
////	if (cstate->csv_mode && strlen(cstate->escape) != 1)
////		ereport(ERROR,
////				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
////				 errmsg("COPY escape must be a single one-byte character")));
//
//	/* Check force_quote */
////	if (!cstate->csv_mode && (force_quote != NIL || force_quote_all))
//	if ( (force_quote != NIL || force_quote_all))
//		ereport(ERROR,
//				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//				 errmsg("COPY force quote available only in CSV mode")));
//	if ((force_quote != NIL || force_quote_all) && is_from)
//		ereport(ERROR,
//				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//				 errmsg("COPY force quote only available using COPY TO")));
//
//	/* Check force_notnull */
////	if (!cstate->csv_mode && force_notnull != NIL)
//	if (force_notnull != NIL)
//		ereport(ERROR,
//				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//				 errmsg("COPY force not null available only in CSV mode")));
//	if (force_notnull != NIL && !is_from)
//		ereport(ERROR,
//				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//			  errmsg("COPY force not null only available using COPY FROM")));
//
//	/* Don't allow the delimiter to appear in the null string. */
//	if (strchr(cstate->null_print, cstate->delim[0]) != NULL)
//		ereport(ERROR,
//				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//		errmsg("COPY delimiter must not appear in the NULL specification")));
//
//	/* Don't allow the CSV quote char to appear in the null string. */
////	if (cstate->csv_mode &&
////		strchr(cstate->null_print, cstate->quote[0]) != NULL)
////		ereport(ERROR,
////				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
////				 errmsg("CSV quote character must not appear in the NULL specification")));
//
//	/* Disallow file COPY except to superusers. */
//	if (!pipe && !superuser())
//		ereport(ERROR,
//				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
//				 errmsg("must be superuser to COPY to or from a file"),
//				 errhint("Anyone can COPY to stdout or from stdin. "
//						 "psql's \\copy command also works for anyone.")));
//
//	if (stmt->relation)
//	{
//		Assert(!stmt->query);
//		cstate->queryDesc = NULL;
//
//		/* Open and lock the relation, using the appropriate lock type. */
//		cstate->rel = heap_openrv(stmt->relation,
//							 (is_from ? RowExclusiveLock : AccessShareLock));
//
//		tupDesc = RelationGetDescr(cstate->rel);
//
//		/* Check relation permissions. */
//		relPerms = pg_class_aclmask(RelationGetRelid(cstate->rel), GetUserId(),
//									required_access, ACLMASK_ALL);
//		remainingPerms = required_access & ~relPerms;
//		if (remainingPerms != 0)
//		{
//			/* We don't have table permissions, check per-column permissions */
//			List	   *attnums;
//			ListCell   *cur;
//
//			attnums = CopyGetAttnums(tupDesc, cstate->rel, attnamelist);
//			foreach(cur, attnums)
//			{
//				int			attnum = lfirst_int(cur);
//
//				if (pg_attribute_aclcheck(RelationGetRelid(cstate->rel),
//										  attnum,
//										  GetUserId(),
//										  remainingPerms) != ACLCHECK_OK)
//					aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_CLASS,
//								   RelationGetRelationName(cstate->rel));
//			}
//		}
//
//		/* check read-only transaction */
//		if (XactReadOnly && is_from && !cstate->rel->rd_islocaltemp)
//			PreventCommandIfReadOnly("COPY FROM");
//
////		/* Don't allow COPY w/ OIDs to or from a table without them */
////		if (cstate->oids && !cstate->rel->rd_rel->relhasoids)
////			ereport(ERROR,
////					(errcode(ERRCODE_UNDEFINED_COLUMN),
////					 errmsg("table \"%s\" does not have OIDs",
////							RelationGetRelationName(cstate->rel))));
//	}
//	else /* SnoopDB: Failed to define relation in the input command */
//	{
//		List	   *rewritten;
//		Query	   *query;
//		PlannedStmt *plan;
//		DestReceiver *dest;
//
//		Assert(!is_from);
//		cstate->rel = NULL;
//
////		/* Don't allow COPY w/ OIDs from a select */
////		if (cstate->oids)
////			ereport(ERROR,
////					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
////					 errmsg("COPY (SELECT) WITH OIDS is not supported")));
//
//		/*
//		 * Run parse analysis and rewrite.	Note this also acquires sufficient
//		 * locks on the source table(s).
//		 *
//		 * Because the parser and planner tend to scribble on their input, we
//		 * make a preliminary copy of the source querytree.  This prevents
//		 * problems in the case that the COPY is in a portal or plpgsql
//		 * function and is executed repeatedly.  (See also the same hack in
//		 * DECLARE CURSOR and PREPARE.)  XXX FIXME someday.
//		 */
//		rewritten = pg_analyze_and_rewrite((Node *) copyObject(stmt->query),
//										   queryString, NULL, 0);
//
//		/* We don't expect more or less than one result query */
//		if (list_length(rewritten) != 1)
//			elog(ERROR, "unexpected rewrite result");
//
//		query = (Query *) linitial(rewritten);
//		Assert(query->commandType == CMD_SELECT);
//		Assert(query->utilityStmt == NULL);
//
//		/* Query mustn't use INTO, either */
//		if (query->intoClause)
//			ereport(ERROR,
//					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//					 errmsg("COPY (SELECT INTO) is not supported")));
//
//		/* plan the query */
//		plan = planner(query, 0, NULL);
//
//		/*
//		 * Use a snapshot with an updated command ID to ensure this query sees
//		 * results of any previously executed queries.
//		 */
//		PushUpdatedSnapshot(GetActiveSnapshot());
//
//		/* Create dest receiver for COPY OUT */
//		dest = CreateDestReceiver(DestCopyOut);
//		((DR_copy *) dest)->cstate = cstate;
//
//		/* Create a QueryDesc requesting no output */
//		cstate->queryDesc = CreateQueryDesc(plan, queryString,
//											GetActiveSnapshot(),
//											InvalidSnapshot,
//											dest, NULL, 0);
//
//		/*
//		 * Call ExecutorStart to prepare the plan for execution.
//		 *
//		 * ExecutorStart computes a result tupdesc for us
//		 */
//		ExecutorStart(cstate->queryDesc, 0);
//
//		tupDesc = cstate->queryDesc->tupDesc;
//	}
//
//	/* Generate or convert list of attributes to process */
//	cstate->attnumlist = CopyGetAttnums(tupDesc, cstate->rel, attnamelist);
//
////	num_phys_attrs = tupDesc->natts;
////	/* Convert FORCE QUOTE name list to per-column flags, check validity */
////	cstate->force_quote_flags = (bool *) palloc0(num_phys_attrs * sizeof(bool));
////	if (force_quote_all)
////	{
////		int			i;
////
////		for (i = 0; i < num_phys_attrs; i++)
////			cstate->force_quote_flags[i] = true;
////	}
////	else if (force_quote)
////	{
////		List	   *attnums;
////		ListCell   *cur;
////
////		attnums = CopyGetAttnums(tupDesc, cstate->rel, force_quote);
////
////		foreach(cur, attnums)
////		{
////			int			attnum = lfirst_int(cur);
////
////			if (!list_member_int(cstate->attnumlist, attnum))
////				ereport(ERROR,
////						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
////				   errmsg("FORCE QUOTE column \"%s\" not referenced by COPY",
////						  NameStr(tupDesc->attrs[attnum - 1]->attname))));
////			cstate->force_quote_flags[attnum - 1] = true;
////		}
////	}
//
////	/* Convert FORCE NOT NULL name list to per-column flags, check validity */
////	cstate->force_notnull_flags = (bool *) palloc0(num_phys_attrs * sizeof(bool));
////	if (force_notnull)
////	{
////		List	   *attnums;
////		ListCell   *cur;
////
////		attnums = CopyGetAttnums(tupDesc, cstate->rel, force_notnull);
////
////		foreach(cur, attnums)
////		{
////			int			attnum = lfirst_int(cur);
////
////			if (!list_member_int(cstate->attnumlist, attnum))
////				ereport(ERROR,
////						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
////				errmsg("FORCE NOT NULL column \"%s\" not referenced by COPY",
////					   NameStr(tupDesc->attrs[attnum - 1]->attname))));
////			cstate->force_notnull_flags[attnum - 1] = true;
////		}
////	}
//
//	/* Set up variables to avoid per-attribute overhead. */
//	initStringInfo(&cstate->attribute_buf);
//	initStringInfo(&cstate->line_buf);
//	cstate->line_buf_converted = false;
////	cstate->raw_buf = (char *) palloc(RAW_BUF_SIZE + 1);
//	cstate->raw_buf = (char *) malloc(RAW_BUF_SIZE + 1);
//	cstate->raw_buf_index = cstate->raw_buf_len = 0;
//	cstate->processed = 0;
//
//	/*
//	 * Set up encoding conversion info.  Even if the client and server
//	 * encodings are the same, we must apply pg_client_to_server() to validate
//	 * data in multibyte encodings.
//	 */
//	cstate->client_encoding = pg_get_client_encoding();
//	cstate->need_transcoding =
//		(cstate->client_encoding != GetDatabaseEncoding() ||
//		 pg_database_encoding_max_length() > 1);
//	/* See Multibyte encoding comment above */
//	cstate->encoding_embeds_ascii = PG_ENCODING_IS_CLIENT_ONLY(cstate->client_encoding);
//
//	cstate->copy_dest = COPY_FILE;		/* default */
//	cstate->filename = stmt->filename;
//
//	//Close Heap before proceeding... (We don't need it any more)
//	/*
//	 * Close the relation or query.  If reading, we can release the
//	 * AccessShareLock we got; if writing, we should hold the lock until end
//	 * of transaction to ensure that updates will be committed before lock is
//	 * released.
//	 */
//	if (cstate->rel)
//		heap_close(cstate->rel, (is_from ? NoLock : AccessShareLock));
//	else
//	{
//		/* Close down the query and free resources. */
//		ExecutorEnd(cstate->queryDesc);
//		FreeQueryDesc(cstate->queryDesc);
//		PopActiveSnapshot();
//	}
//
//	return cstate;
//}
//
//
//
///*
// * Prepare struct with query execution status
// */
//CopyStmtExecStatus
//prepareCopyFromFile(CopyState cstate)
//{
//	CopyStmtExecStatus status;
//	bool pipe = (cstate->filename == NULL);
//	int	attnum;
//
//	status.estate = CreateExecutorState(); // for ExecConstraints()
//	status.oldcontext = CurrentMemoryContext;
//	status.hi_options = 0; // start with default heap_insert options
//
//	status.loadFromCacheOnly = false;
//	status.loadWithoutInteresting  = false;
//
//	/*
//	*Check if input relation is actually a relation :
//	* #define RELKIND_RELATION 'r'
//	* #define RELKIND_SEQUENCE 'S'
//	* #define RELKIND_VIEW	   'v'
//	*/
//	Assert(cstate->rel);
//	if (cstate->rel->rd_rel->relkind != RELKIND_RELATION)
//	{
//		if (cstate->rel->rd_rel->relkind == RELKIND_VIEW)
//			ereport(ERROR,
//					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
//					 errmsg("cannot copy to view \"%s\"",
//							RelationGetRelationName(cstate->rel))));
//		else if (cstate->rel->rd_rel->relkind == RELKIND_SEQUENCE)
//			ereport(ERROR,
//					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
//					 errmsg("cannot copy to sequence \"%s\"",
//							RelationGetRelationName(cstate->rel))));
//		else
//			ereport(ERROR,
//					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
//					 errmsg("cannot copy to non-table relation \"%s\"",
//							RelationGetRelationName(cstate->rel))));
//	}
//
//
//	/*----------
//	 * Check to see if we can avoid writing WAL
//	 *
//	 * If archive logging/streaming is not enabled *and* either
//	 *	- table was created in same transaction as this COPY
//	 *	- data is being written to relfilenode created in this transaction
//	 * then we can skip writing WAL.  It's safe because if the transaction
//	 * doesn't commit, we'll discard the table (or the new relfilenode file).
//	 * If it does commit, we'll have done the heap_sync at the bottom of this
//	 * routine first.
//	 *
//	 * As mentioned in comments in utils/rel.h, the in-same-transaction test
//	 * is not completely reliable, since in rare cases rd_createSubid or
//	 * rd_newRelfilenodeSubid can be cleared before the end of the transaction.
//	 * However this is OK since at worst we will fail to make the optimization.
//	 *
//	 * Also, if the target file is new-in-transaction, we assume that checking
//	 * FSM for free space is a waste of time, even if we must use WAL because
//	 * of archiving.  This could possibly be wrong, but it's unlikely.
//	 *
//	 * The comments for heap_insert and RelationGetBufferForTuple specify that
//	 * skipping WAL logging is only safe if we ensure that our tuples do not
//	 * go into pages containing tuples from any other transactions --- but this
//	 * must be the case if we have a new table or new relfilenode, so we need
//	 * no additional work to enforce that.
//	 *----------
//	 */
//	if (cstate->rel->rd_createSubid != InvalidSubTransactionId ||
//		cstate->rel->rd_newRelfilenodeSubid != InvalidSubTransactionId)
//	{
//		status.hi_options |= HEAP_INSERT_SKIP_FSM;
//		if (!XLogIsNeeded())
//			status.hi_options |= HEAP_INSERT_SKIP_WAL;
//	}
//
//	if (pipe)
//	{
//		if (whereToSendOutput == DestRemote)
//			ReceiveCopyBegin(cstate);
//		else
//			cstate->copy_file = stdin;
//	}
//	else
//	{
//		struct stat st;
//
//		cstate->copy_file = AllocateFile(cstate->filename, PG_BINARY_R);
//
//		if (cstate->copy_file == NULL)
//			ereport(ERROR,
//					(errcode_for_file_access(),
//					 errmsg("could not open file \"%s\" for reading: %m",
//							cstate->filename)));
//
//		fstat(fileno(cstate->copy_file), &st);
//		if (S_ISDIR(st.st_mode))
//			ereport(ERROR,
//					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
//					 errmsg("\"%s\" is a directory", cstate->filename)));
//	}
//	status.tupDesc = RelationGetDescr(cstate->rel);
//	status.attr = status.tupDesc->attrs;
//	status.num_phys_attrs = status.tupDesc->natts;
//	status.attr_count = list_length(cstate->attnumlist);//?????
//	status.num_defaults = 0;
//
//	/*
//	 * We need a ResultRelInfo so we can use the regular executor's
//	 * index-entry-making machinery.  (There used to be a huge amount of code
//	 * here that basically duplicated execUtils.c ...)
//	 */
//	status.resultRelInfo = makeNode(ResultRelInfo);
//	status.resultRelInfo->ri_RangeTableIndex = 1;		// dummy
//	status.resultRelInfo->ri_RelationDesc = cstate->rel;
//	status.resultRelInfo->ri_TrigDesc = CopyTriggerDesc(cstate->rel->trigdesc);
//	if (status.resultRelInfo->ri_TrigDesc)
//	{
//		status.resultRelInfo->ri_TrigFunctions = (FmgrInfo *)
//			palloc0(status.resultRelInfo->ri_TrigDesc->numtriggers * sizeof(FmgrInfo));
//		status.resultRelInfo->ri_TrigWhenExprs = (List **)
//			palloc0(status.resultRelInfo->ri_TrigDesc->numtriggers * sizeof(List *));
//	}
//	status.resultRelInfo->ri_TrigInstrument = NULL;
//
//	ExecOpenIndices(status.resultRelInfo);
//
//	status.estate->es_result_relations = status.resultRelInfo;
//	status.estate->es_num_result_relations = 1;
//	status.estate->es_result_relation_info = status.resultRelInfo;
//
//	/* Set up a tuple slot too */
//	status.slot = ExecInitExtraTupleSlot(status.estate);
//	ExecSetSlotDescriptor(status.slot, status.tupDesc);
//	status.econtext = GetPerTupleExprContext(status.estate);
//
//	/*
//	 * Pick up the required catalog information for each attribute in the
//	 * relation, including the input function, the element type (to pass to
//	 * the input function), and info about defaults and constraints. (Which
//	 * input function we use depends on text/binary format choice.)
//	 */
//	status.in_functions = (FmgrInfo *) palloc(status.num_phys_attrs * sizeof(FmgrInfo));
//	cstate->fcinfo = (FunctionCallInfoData *) palloc(status.num_phys_attrs * sizeof(FunctionCallInfoData));
//
//	status.typioparams = (Oid *) palloc(status.num_phys_attrs * sizeof(Oid));
//	status.defmap = (int *) palloc(status.num_phys_attrs * sizeof(int));
//	status.defexprs = (ExprState **) palloc(status.num_phys_attrs * sizeof(ExprState *));
//
//	for (attnum = 1; attnum <= status.num_phys_attrs; attnum++)
//	{
//		// We don't need info for dropped attributes //
//		if (status.attr[attnum - 1]->attisdropped)
//			continue;
//
//		// Fetch the input function and typioparam info //
//		if (cstate->binary)
//			getTypeBinaryInputInfo(status.attr[attnum - 1]->atttypid,
//								   &status.in_func_oid, &status.typioparams[attnum - 1]);
//		else
//			getTypeInputInfo(status.attr[attnum - 1]->atttypid,
//							 &status.in_func_oid, &status.typioparams[attnum - 1]);
//		fmgr_info(status.in_func_oid, &status.in_functions[attnum - 1]);
//
//
//		//noDB: Add struct for each attribute
//		InitFunctionCallInfoData(cstate->fcinfo[attnum - 1], &status.in_functions[attnum - 1], 3, NULL, NULL);
//
//		//status.fcinfo[attnum - 1].arg[0] = CStringGetDatum(str); --> we need the str for it ;-)
//		cstate->fcinfo[attnum - 1].arg[1] = ObjectIdGetDatum(status.typioparams[attnum - 1]);
//		cstate->fcinfo[attnum - 1].arg[2] = Int32GetDatum(status.attr[attnum - 1]->atttypmod);
//		cstate->fcinfo[attnum - 1].argnull[0] = false;
//		cstate->fcinfo[attnum - 1].argnull[1] = false;
//		cstate->fcinfo[attnum - 1].argnull[2] = false;
//
//
//		// Get default info if needed //
//		if (!list_member_int(cstate->attnumlist, attnum))
//		{
//			// attribute is NOT to be copied from input /
//			// use default value if one exists //
//			Node	   *defexpr = build_column_default(cstate->rel, attnum);
//
//			if (defexpr != NULL)
//			{
//				status.defexprs[status.num_defaults] = ExecPrepareExpr((Expr *) defexpr,
//						status.estate);
//				status.defmap[status.num_defaults] = attnum - 1;
//				status.num_defaults++;
//			}
//		}
//	}
//	/* Prepare to catch AFTER triggers. */
//	AfterTriggerBeginQuery(); //SnoopDB: maybe not needed
//
//	/*
//	 * Check BEFORE STATEMENT insertion triggers. It's debateable whether we
//	 * should do this for COPY, since it's not really an "INSERT" statement as
//	 * such. However, executing these triggers maintains consistency with the
//	 * EACH ROW triggers that we already fire on COPY.
//	 */
//	ExecBSInsertTriggers(status.estate, status.resultRelInfo);
//
//	if (!cstate->binary)
//		status.file_has_oids = false;	// must rely on user to tell us...
////		status.file_has_oids = cstate->oids;	// must rely on user to tell us...
//	else
//	{
//		// Read and verify binary header
//		char		readSig[11];
//		int32		tmp;
//
//		// Signature
//		if (CopyGetData(cstate, readSig, 11, 11) != 11 ||
//			memcmp(readSig, BinarySignature, 11) != 0)
//			ereport(ERROR,
//					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//					 errmsg("COPY file signature not recognized")));
//		// Flags field
//		if (!CopyGetInt32(cstate, &tmp))
//			ereport(ERROR,
//					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//					 errmsg("invalid COPY file header (missing flags)")));
//		status.file_has_oids = (tmp & (1 << 16)) != 0;
//		tmp &= ~(1 << 16);
//		if ((tmp >> 16) != 0)
//			ereport(ERROR,
//					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//				 errmsg("unrecognized critical flags in COPY file header")));
//		// Header extension length
//		if (!CopyGetInt32(cstate, &tmp) ||
//			tmp < 0)
//			ereport(ERROR,
//					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//					 errmsg("invalid COPY file header (missing length)")));
//		// Skip extension header, if present
//		while (tmp-- > 0)
//		{
//			if (CopyGetData(cstate, readSig, 1, 1) != 1)
//				ereport(ERROR,
//						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//						 errmsg("invalid COPY file header (wrong length)")));
//		}
//	}
//	if (status.file_has_oids && cstate->binary)
//	{
//		getTypeBinaryInputInfo(OIDOID, &status.in_func_oid, &status.oid_typioparam);
//		fmgr_info(status.in_func_oid, &status.oid_in_function);
//	}
//	/* Initialize state variables */
//	cstate->fe_eof = false;
//	cstate->eol_type = EOL_UNKNOWN;
////	cstate->cur_relname = RelationGetRelationName(cstate->rel);
////	cstate->cur_lineno = 0;
////	cstate->cur_attname = NULL;
////	cstate->cur_attval = NULL;
//
//	/*Initialize pointers for reading files*/
////	cstate->cur_tuplePointer = 0;
////	cstate->tupleStored = 0;
////	cstate->curReadVector = 0 ;
////	cstate->curReadVectorPosition = 0;
//
////	bistate = GetBulkInsertState();
//
//	/* Set up callback to identify error line number */
////	status.errcontext.callback = copy_in_error_callback;
//	status.errcontext.arg = (void *) cstate;
//	status.errcontext.previous = error_context_stack;
//	error_context_stack = &status.errcontext;
//
//	/*Allocate memory*/
//	cstate->values = (Datum *) palloc(status.num_phys_attrs * sizeof(Datum));
//	cstate->nulls = (bool *) palloc(status.num_phys_attrs * sizeof(bool));
//
//	//Added for testing
//	MemSet(cstate->values, 0, status.num_phys_attrs * sizeof(Datum));
//	MemSet(cstate->nulls, true, status.num_phys_attrs * sizeof(bool));
//
//	/* create workspace for CopyReadAttributes results */
//	cstate->nfields = status.file_has_oids ? (status.attr_count + 1) : status.attr_count;
//	cstate->field_strings = (char **) palloc(cstate->nfields * sizeof(char *));
//
//	return status;
//}
//
//bool
//getExecutionStatusCache(int pos)
//{
//	return CopyExec[pos].status.loadFromCacheOnly;
//}
//
//bool
//getExecutionStatusInteresting(int pos)
//{
//	return CopyExec[pos].status.loadWithoutInteresting;
//}
//
//
//
//
///*
// * CopyGetAttnums - build an integer list of attnums to be copied
// *
// * The input attnamelist is either the user-specified column list,
// * or NIL if there was none (in which case we want all the non-dropped
// * columns).
// *
// * rel can be NULL ... it's only used for error reports.
// */
//static List *
//CopyGetAttnums(TupleDesc tupDesc, Relation rel, List *attnamelist)
//{
//	List	   *attnums = NIL;
//
//	if (attnamelist == NIL)
//	{
//		/* Generate default column list */
//		Form_pg_attribute *attr = tupDesc->attrs;
//		int			attr_count = tupDesc->natts;
//		int			i;
//
//		for (i = 0; i < attr_count; i++)
//		{
//			if (attr[i]->attisdropped)
//				continue;
//			attnums = lappend_int(attnums, i + 1);
//		}
//	}
//	else
//	{
//		/* Validate the user-supplied list and extract attnums */
//		ListCell   *l;
//
//		foreach(l, attnamelist)
//		{
//			char	   *name = strVal(lfirst(l));
//			int			attnum;
//			int			i;
//
//			/* Lookup column name */
//			attnum = InvalidAttrNumber;
//			for (i = 0; i < tupDesc->natts; i++)
//			{
//				if (tupDesc->attrs[i]->attisdropped)
//					continue;
//				if (namestrcmp(&(tupDesc->attrs[i]->attname), name) == 0)
//				{
//					attnum = tupDesc->attrs[i]->attnum;
//					break;
//				}
//			}
//			if (attnum == InvalidAttrNumber)
//			{
//				if (rel != NULL)
//					ereport(ERROR,
//							(errcode(ERRCODE_UNDEFINED_COLUMN),
//					errmsg("column \"%s\" of relation \"%s\" does not exist",
//						   name, RelationGetRelationName(rel))));
//				else
//					ereport(ERROR,
//							(errcode(ERRCODE_UNDEFINED_COLUMN),
//							 errmsg("column \"%s\" does not exist",
//									name)));
//			}
//			/* Check for duplicates */
//			if (list_member_int(attnums, attnum))
//				ereport(ERROR,
//						(errcode(ERRCODE_DUPLICATE_COLUMN),
//						 errmsg("column \"%s\" specified more than once",
//								name)));
//			attnums = lappend_int(attnums, attnum);
//		}
//	}
//
//	return attnums;
//}
//
//
//
///*
// * The following function have been copied from /commands/copy.c file and
// * are used without any changes.
// *
// */
//static void
//ReceiveCopyBegin(CopyState cstate)
//{
//	if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
//	{
//		/* new way */
//		StringInfoData buf;
//		int			natts = list_length(cstate->attnumlist);
//		int16		format = (cstate->binary ? 1 : 0);
//		int			i;
//
//		pq_beginmessage(&buf, 'G');
//		pq_sendbyte(&buf, format);		/* overall format */
//		pq_sendint(&buf, natts, 2);
//		for (i = 0; i < natts; i++)
//			pq_sendint(&buf, format, 2);		/* per-column formats */
//		pq_endmessage(&buf);
//		cstate->copy_dest = COPY_NEW_FE;
//		cstate->fe_msgbuf = makeStringInfo();
//	}
//	else if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 2)
//	{
//		/* old way */
//		if (cstate->binary)
//			ereport(ERROR,
//					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//			errmsg("COPY BINARY is not supported to stdout or from stdin")));
//		pq_putemptymessage('G');
//		cstate->copy_dest = COPY_OLD_FE;
//	}
//	else
//	{
//		/* very old way */
//		if (cstate->binary)
//			ereport(ERROR,
//					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//			errmsg("COPY BINARY is not supported to stdout or from stdin")));
//		pq_putemptymessage('D');
//		cstate->copy_dest = COPY_OLD_FE;
//	}
//	/* We *must* flush here to ensure FE knows it can send. */
//	pq_flush();
//}
//
//
//
//
//
///*
// * CopyGetData reads data from the source (file or frontend)
// *
// * We attempt to read at least minread, and at most maxread, bytes from
// * the source.	The actual number of bytes read is returned; if this is
// * less than minread, EOF was detected.
// *
// * Note: when copying from the frontend, we expect a proper EOF mark per
// * protocol; if the frontend simply drops the connection, we raise error.
// * It seems unwise to allow the COPY IN to complete normally in that case.
// *
// * NB: no data conversion is applied here.
// */
//int
//CopyGetData(CopyState cstate, void *databuf, int minread, int maxread)
//{
//	int			bytesread = 0;
//
//	switch (cstate->copy_dest)
//	{
//		case COPY_FILE:
//			bytesread = fread(databuf, 1, maxread, cstate->copy_file);
//			if (ferror(cstate->copy_file))
//				ereport(ERROR,
//						(errcode_for_file_access(),
//						 errmsg("could not read from COPY file: %m")));
//			break;
//		case COPY_OLD_FE:
//
//			/*
//			 * We cannot read more than minread bytes (which in practice is 1)
//			 * because old protocol doesn't have any clear way of separating
//			 * the COPY stream from following data.  This is slow, but not any
//			 * slower than the code path was originally, and we don't care
//			 * much anymore about the performance of old protocol.
//			 */
//			if (pq_getbytes((char *) databuf, minread))
//			{
//				/* Only a \. terminator is legal EOF in old protocol */
//				ereport(ERROR,
//						(errcode(ERRCODE_CONNECTION_FAILURE),
//						 errmsg("unexpected EOF on client connection")));
//			}
//			bytesread = minread;
//			break;
//		case COPY_NEW_FE:
//			while (maxread > 0 && bytesread < minread && !cstate->fe_eof)
//			{
//				int			avail;
//
//				while (cstate->fe_msgbuf->cursor >= cstate->fe_msgbuf->len)
//				{
//					/* Try to receive another message */
//					int			mtype;
//
//			readmessage:
//					mtype = pq_getbyte();
//					if (mtype == EOF)
//						ereport(ERROR,
//								(errcode(ERRCODE_CONNECTION_FAILURE),
//							 errmsg("unexpected EOF on client connection")));
//					if (pq_getmessage(cstate->fe_msgbuf, 0))
//						ereport(ERROR,
//								(errcode(ERRCODE_CONNECTION_FAILURE),
//							 errmsg("unexpected EOF on client connection")));
//					switch (mtype)
//					{
//						case 'd':		/* CopyData */
//							break;
//						case 'c':		/* CopyDone */
//							/* COPY IN correctly terminated by frontend */
//							cstate->fe_eof = true;
//							return bytesread;
//						case 'f':		/* CopyFail */
//							ereport(ERROR,
//									(errcode(ERRCODE_QUERY_CANCELED),
//									 errmsg("COPY from stdin failed: %s",
//									   pq_getmsgstring(cstate->fe_msgbuf))));
//							break;
//						case 'H':		/* Flush */
//						case 'S':		/* Sync */
//
//							/*
//							 * Ignore Flush/Sync for the convenience of client
//							 * libraries (such as libpq) that may send those
//							 * without noticing that the command they just
//							 * sent was COPY.
//							 */
//							goto readmessage;
//						default:
//							ereport(ERROR,
//									(errcode(ERRCODE_PROTOCOL_VIOLATION),
//									 errmsg("unexpected message type 0x%02X during COPY from stdin",
//											mtype)));
//							break;
//					}
//				}
//				avail = cstate->fe_msgbuf->len - cstate->fe_msgbuf->cursor;
//				if (avail > maxread)
//					avail = maxread;
//				pq_copymsgbytes(cstate->fe_msgbuf, databuf, avail);
//				databuf = (void *) ((char *) databuf + avail);
//				maxread -= avail;
//				bytesread += avail;
//			}
//			break;
//	}
//
//	return bytesread;
//}
//
///*----------
// * CopySendData sends output data to the destination (file or frontend)
// *----------
// */
////void
////CopySendData(CopyState cstate, void *databuf, int datasize)
////{
////	appendBinaryStringInfo(cstate->fe_msgbuf, (char *) databuf, datasize);
////}
//
//
///*
// * These functions do apply some data conversion
// */
//
///*
// * CopySendInt32 sends an int32 in network byte order
// */
////void
////CopySendInt32(CopyState cstate, int32 val)
////{
////	uint32		buf;
////
////	buf = htonl((uint32) val);
////	CopySendData(cstate, &buf, sizeof(buf));
////}
//
///*
// * CopyGetInt32 reads an int32 that appears in network byte order
// *
// * Returns true if OK, false if EOF
// */
//bool
//CopyGetInt32(CopyState cstate, int32 *val)
//{
//	uint32		buf;
//
//	if (CopyGetData(cstate, &buf, sizeof(buf), sizeof(buf)) != sizeof(buf))
//	{
//		*val = 0;				/* suppress compiler warning */
//		return false;
//	}
//	*val = (int32) ntohl(buf);
//	return true;
//}
//
///*
// * CopySendInt16 sends an int16 in network byte order
// */
////void
////CopySendInt16(CopyState cstate, int16 val)
////{
////	uint16		buf;
////
////	buf = htons((uint16) val);
////	CopySendData(cstate, &buf, sizeof(buf));
////}
//
///*
// * CopyGetInt16 reads an int16 that appears in network byte order
// */
////bool
////CopyGetInt16(CopyState cstate, int16 *val)
////{
////	uint16		buf;
////
////	if (CopyGetData(cstate, &buf, sizeof(buf), sizeof(buf)) != sizeof(buf))
////	{
////		*val = 0;				/* suppress compiler warning */
////		return false;
////	}
////	*val = (int16) ntohs(buf);
////	return true;
////}
//
//
///*
// * Auxilliary function that converts a Heaptuple into TupleTableSlot
// */
//TupleTableSlot *
//heapTupleToTupleTableSlot(HeapTuple tuple, TupleTableSlot *ss_ScanTupleSlot, bool done)
//{
//	TupleTableSlot *slot;
//	if(done == 0)
//	{
//		slot = ss_ScanTupleSlot;
//		slot->tts_isempty = false;
//		slot->tts_shouldFree = false;
//		slot->tts_shouldFreeMin = false;
//		slot->tts_tuple = tuple;
//		slot->tts_tupleDescriptor = ss_ScanTupleSlot->tts_tupleDescriptor;
//		slot->tts_mintuple = NULL;
//		slot->tts_nvalid = 0;
//	}
//	else
//		slot = NULL;
//
//	return slot;
//}
//
//
////typedef struct TupleTableSlot
////{
////	NodeTag		type;
////	bool		tts_isempty;	/* true = slot is empty */
////	bool		tts_shouldFree; /* should pfree tts_tuple? */
////	bool		tts_shouldFreeMin;		/* should pfree tts_mintuple? */
////	bool		tts_slow;		/* saved state for slot_deform_tuple */
////	HeapTuple	tts_tuple;		/* physical tuple, or NULL if virtual */
////	TupleDesc	tts_tupleDescriptor;	/* slot's tuple descriptor */
////	MemoryContext tts_mcxt;		/* slot itself is in this context */
////	Buffer		tts_buffer;		/* tuple's buffer, or InvalidBuffer */
////	int			tts_nvalid;		/* # of valid values in tts_values */
////	Datum	   *tts_values;		/* current per-attribute values */
////	bool	   *tts_isnull;		/* current per-attribute isnull flags */
////	MinimalTuple tts_mintuple;	/* minimal tuple, or NULL if none */
////	HeapTupleData tts_minhdr;	/* workspace for minimal-tuple-only case */
////	long		tts_off;		/* saved state for slot_deform_tuple */
////} TupleTableSlot;
//
//
//TupleTableSlot *
//generateTupleTableSlot( TupleTableSlot *ss_ScanTupleSlot, Datum *values, bool *isnull)
//{
//	TupleTableSlot *slot;
//
//	slot = ss_ScanTupleSlot;
//	slot->tts_isempty = false;
//	slot->tts_shouldFree = false;
//	slot->tts_shouldFreeMin = false;
////	slot->tts_tuple = tuple;
//	slot->tts_mintuple = NULL;
//
//	slot->tts_tupleDescriptor = ss_ScanTupleSlot->tts_tupleDescriptor;
//
//	slot->tts_nvalid = ss_ScanTupleSlot->tts_tupleDescriptor->natts;
//	slot->tts_values = values;
//	slot->tts_isnull = isnull;
//
//	return slot;
//}
//
//
///*
// * Auxilliary function for scan nodes, return the proper ScanState struct
// */
//ScanState *
//getProperScanState(PlanState *planstate)
//{
//	ScanState *node;
//
//	switch (nodeTag(planstate))
//	{
//			/*
//			 * scan nodes
//			 */
//		case T_SeqScanState:
//			node = (SeqScanState *) planstate;
//			break;
//
//		case T_IndexScanState:
//
//			if (((IndexScanState *)planstate)->iss_NumRuntimeKeys != 0 && !((IndexScanState *)planstate)->iss_RuntimeKeysReady)
//				node = NULL;//reScan will be called...
//			else //&node->ss
//				node = &((IndexScanState *) planstate)->ss;
//			break;
//
//		case T_BitmapHeapScanState:
//			node = &((BitmapHeapScanState *) planstate)->ss;
//			break;
//
//		case T_TidScanState:
//			node = &((TidScanState *) planstate)->ss;
//			break;
//
//		case T_SubqueryScanState:
//			node = &((SubqueryScanState *) planstate)->ss;
//			break;
//
//		case T_FunctionScanState:
//			node = &((FunctionScanState *) planstate)->ss;
//			break;
//
//		case T_ValuesScanState:
//			node = &((ValuesScanState *) planstate)->ss;
//			break;
//
//		case T_CteScanState:
//			node = &((CteScanState *) planstate)->ss;
//			break;
//
//		case T_WorkTableScanState://TODO:check if the statement below is valid...
//			node = &((WorkTableScanState *) planstate)->ss;
//			break;
//
//		default:
////			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(planstate));
//			node = NULL;
//			break;
//	}
//
//	return node;
//}
//
//
///*
// * Search through a PlanState tree for a scan nodes and update Tag to newTag.
// * Based on ExplainNode (commnad/explain.c)
// * Currently this aply only to SeqScan.
// * TODO: Change so as upadate other nodes as well (e.g. IndexSan if needed) + Scan to different table ==> different policy!!!
// */
//
//List *
//traversePlanTree(Plan *plan, PlanState *planstate, Plan *outer_plan, PlannedStmt *topPlan)
//{
//		List *planstateList = NIL;
//		List *tmpList = NIL;
//		ScanState *node;
//		Assert(plan);
//
//		//Check Plan Nodes (plannodes.h)
//		switch (nodeTag(plan))
//		{
//			case T_IndexScan:
//			case T_SeqScan:
//			case T_BitmapHeapScan:
//			case T_TidScan:
//			case T_SubqueryScan:
//			case T_FunctionScan:
//			case T_ValuesScan:
//			case T_CteScan:
//			case T_WorkTableScan:
//			{
//				if (plan->qual)
//				{
//					changeParseStatus(PS_filterList);
//					traverseQual(plan->qual, plan, NULL, topPlan);
//					changeParseStatus(PS_idle);
//				}
//
//				node = getProperScanState(planstate);
//				if ( node )
//					planstateList = lcons(node, planstateList);
//				break;
//			}
//			case T_BitmapIndexScan:
//				break;
//			case T_NestLoop:
//			case T_MergeJoin:
//			case T_HashJoin:
//				break;
//			case T_SetOp:
//				break;
//			default:
//				break;
//		}
//
//		//check For Plan State Nodes (execnodes.h) --> No...
////		switch (nodeTag(planstate))
////		{
////		}
//
//		/* initPlan-s */
//		if (plan->initPlan) //PlanState here
//		{
//			tmpList = traverseSubPlans(planstate->initPlan, topPlan);
//			planstateList = list_concat(planstateList, tmpList);
//		}
//
//		/* lefttree */
//		if (outerPlan(plan)) {
//			/*
//			 * Ordinarily we don't pass down our own outer_plan value to our child
//			 * nodes, but in bitmap scan trees we must, since the bottom
//			 * BitmapIndexScan nodes may have outer references.
//			 */
//			tmpList = traversePlanTree(outerPlan(plan), outerPlanState(planstate), IsA(plan, BitmapHeapScan) ? outer_plan : NULL, topPlan);
//			planstateList = list_concat(planstateList, tmpList);
//		}
//
//		/* righttree */
//		if (innerPlan(plan)) {
//			tmpList = traversePlanTree(innerPlan(plan), innerPlanState(planstate), outerPlan(plan), topPlan);
//			planstateList = list_concat(planstateList, tmpList);
//		}
//
//		/* special child plans */
//		switch (nodeTag(plan))
//		{
//			case T_ModifyTable:
//				{
//					tmpList = traverseMemberNodes(((ModifyTable *) plan)->plans,
//												((ModifyTableState *) planstate)->mt_plans,
//												outer_plan, topPlan);
//					planstateList = list_concat(planstateList, tmpList);
//					break;
//				}
//			case T_Append:
//				{
//					tmpList = traverseMemberNodes(((Append *) plan)->appendplans,
//												((AppendState *) planstate)->appendplans,
//												outer_plan, topPlan);
//					planstateList = list_concat(planstateList, tmpList);
//					break;
//				}
//			case T_BitmapAnd:
//				{
//					tmpList = traverseMemberNodes(((BitmapAnd *) plan)->bitmapplans,
//												((BitmapAndState *) planstate)->bitmapplans,
//												outer_plan, topPlan);
//					planstateList = list_concat(planstateList, tmpList);
//					break;
//				}
//			case T_BitmapOr:
//				{
//					tmpList = traverseMemberNodes(((BitmapOr *) plan)->bitmapplans,
//												((BitmapOrState *) planstate)->bitmapplans,
//												outer_plan, topPlan);
//					planstateList = list_concat(planstateList, tmpList);
//					break;
//				}
//			case T_SubqueryScan:
//				{
//					SubqueryScan *subqueryscan = (SubqueryScan *) plan;
//					SubqueryScanState *subquerystate = (SubqueryScanState *) planstate;
//					tmpList = traversePlanTree(subqueryscan->subplan, subquerystate->subplan, NULL, topPlan);
//					planstateList = list_concat(planstateList, tmpList);
//				}
//				break;
//			default:
//				break;
//		}
//
//		/* subPlan-s */
//		if (planstate->subPlan) //PlanState here
//		{
//			tmpList = traverseSubPlans(planstate->subPlan, topPlan);
//			planstateList = list_concat(planstateList, tmpList);
//		}
//
//		return planstateList;
//}
//
//
///*
// * Traverse a list of SubPlans (or initPlans, which also use SubPlan nodes).
// */
//static List*
//traverseSubPlans(List *plans, PlannedStmt *top)
//{
//	ListCell   *lst;
//	List *planstateList = NIL;
//	foreach(lst, plans)
//	{
//		SubPlanState *sps = (SubPlanState *) lfirst(lst);
//		SubPlan    *sp = (SubPlan *) sps->xprstate.expr;
//		List *tmpList = traversePlanTree(exec_subplan_get_plan(top, sp),sps->planstate, NULL, top);
//		planstateList = list_concat(planstateList, tmpList);
//	}
//	return planstateList;
//}
//
//
//static List*
//traverseMemberNodes(List *plans, PlanState **planstate, Plan *outer_plan, PlannedStmt *top)
//{
//	ListCell   *lst;
//	List *planstateList = NIL;
//	int			j = 0;
//	foreach(lst, plans)
//	{
//		Plan	   *subnode = (Plan *) lfirst(lst);
//		List *tmpList = traversePlanTree(subnode, planstate[j], outer_plan, top);
//		planstateList = list_concat(planstateList, tmpList);
//		j++;
//	}
//	return planstateList;
//}
//
//
//static void
//traverseQual(List *qual, Plan *plan, Plan *outer_plan, PlannedStmt *topPlan)
//{
//	List	   *context;
//	Node	   *node;
////	char	   *exprstr;
//
//	ExplainState es;
//
//	/* Initialize ExplainState. */
//	ExplainInitState(&es);
//
//	es.rtable = topPlan->rtable;
//	es.pstmt = topPlan;
//
//	/* No work if empty qual */
//	if (qual == NIL)
//		return;
//
//	/* Convert AND list to explicit AND */
//	node = (Node *) make_ands_explicit(qual);
//
//	/* Set up deparsing context */
//	context = deparse_context_for_plan((Node *) plan,
//									   (Node *) outer_plan,
//									   es.rtable,
//									   es.pstmt->subplans);
//
//	/* Deparse the expression */
////	exprstr = deparse_expression(node, context, false, false);
//	deparse_expression(node, context, false, false);
//
//}
//
//
//
//void
//updatePlanTree(Plan *plan, PlanState *planstate, Plan *outer_plan, PlannedStmt *topPlan, NodeTag oldTag, NodeTag newTag)
//{
//		Assert(plan);
//
//		//Check Plan Nodes (plannodes.h)
//		switch (nodeTag(plan))
//		{
//			case T_IndexScan:
//			case T_SeqScan:
//			case T_BitmapHeapScan:
//			case T_TidScan:
//			case T_SubqueryScan:
//			case T_FunctionScan:
//			case T_ValuesScan:
//			case T_CteScan:
//			case T_WorkTableScan:
//				updateScanTarget((Scan*)plan, planstate, oldTag, newTag);
//				break;
//			case T_BitmapIndexScan: //only in ExecInitNode but not in ExecProcNode (?)
//				updateScanTarget((Scan*)plan, planstate, oldTag, newTag);
//				break;
//			case T_NestLoop:
//			case T_MergeJoin:
//			case T_HashJoin:
//				break;
//			case T_SetOp:
//				break;
//			default:
//				break;
//		}
//
//		//check For Plan State Nodes (execnodes.h) --> No...
////		switch (nodeTag(planstate))
////		{
////		}
//
//		/* initPlan-s */
//		if (plan->initPlan) //PlanState here
//			updateSubPlans(planstate->initPlan, topPlan, oldTag, newTag);
//
//		/* lefttree */
//		if (outerPlan(plan)) {
//			/*
//			 * Ordinarily we don't pass down our own outer_plan value to our child
//			 * nodes, but in bitmap scan trees we must, since the bottom
//			 * BitmapIndexScan nodes may have outer references.
//			 */
//			updatePlanTree(outerPlan(plan), outerPlanState(planstate), IsA(plan, BitmapHeapScan) ? outer_plan : NULL, topPlan, oldTag, newTag);
//		}
//
//		/* righttree */
//		if (innerPlan(plan))
//			updatePlanTree(innerPlan(plan), innerPlanState(planstate), outerPlan(plan), topPlan, oldTag, newTag);
//
//		/* special child plans */
//		switch (nodeTag(plan))
//		{
//			case T_ModifyTable:
//				{
//					updateMemberNodes(((ModifyTable *) plan)->plans,
//										((ModifyTableState *) planstate)->mt_plans,
//										outer_plan, topPlan, oldTag, newTag);
//					break;
//				}
//			case T_Append:
//				{
//					updateMemberNodes(((Append *) plan)->appendplans,
//										((AppendState *) planstate)->appendplans,
//										outer_plan, topPlan, oldTag, newTag);
//					break;
//				}
//			case T_BitmapAnd:
//				{
//					updateMemberNodes(((BitmapAnd *) plan)->bitmapplans,
//										((BitmapAndState *) planstate)->bitmapplans,
//										outer_plan, topPlan, oldTag, newTag);
//					break;
//				}
//			case T_BitmapOr:
//				{
//					updateMemberNodes(((BitmapOr *) plan)->bitmapplans,
//										((BitmapOrState *) planstate)->bitmapplans,
//										outer_plan, topPlan, oldTag, newTag);
//					break;
//				}
//			case T_SubqueryScan:
//				{
//					SubqueryScan *subqueryscan = (SubqueryScan *) plan;
//					SubqueryScanState *subquerystate = (SubqueryScanState *) planstate;
//					updatePlanTree(subqueryscan->subplan, subquerystate->subplan, NULL, topPlan, oldTag, newTag);
//				}
//				break;
//			default:
//				break;
//		}
//
//		/* subPlan-s */
//		if (planstate->subPlan) //PlanState here
//			updateSubPlans(planstate->subPlan, topPlan, oldTag, newTag);
//
//}
//
//
//
//static void
//updateSubPlans(List *plans, PlannedStmt *top, NodeTag oldTag, NodeTag newTag)
//{
//	ListCell   *lst;
//	foreach(lst, plans)
//	{
//		SubPlanState *sps = (SubPlanState *) lfirst(lst);
//		SubPlan    *sp = (SubPlan *) sps->xprstate.expr;
//		updatePlanTree(exec_subplan_get_plan(top, sp),sps->planstate, NULL, top, oldTag, newTag);
//	}
//}
//
//
//static void
//updateMemberNodes(List *plans, PlanState **planstate, Plan *outer_plan, PlannedStmt *top, NodeTag oldTag, NodeTag newTag)
//{
//	ListCell   *lst;
//	int			j = 0;
//	foreach(lst, plans)
//	{
//		Plan	   *subnode = (Plan *) lfirst(lst);
//		updatePlanTree(subnode, planstate[j], outer_plan, top, oldTag, newTag);
//		j++;
//	}
//}
//
//
///*
// * Show the target of a Scan node
// */
//static void
//updateScanTarget(Scan *plan, PlanState *planstate, NodeTag oldTag, NodeTag newTag)
//{
//
//	switch (nodeTag(plan))
//	{
//		case T_SeqScan:
//		case T_IndexScan:
//		case T_BitmapHeapScan:
//		case T_TidScan:
//		case T_FunctionScan:
//		case T_ValuesScan:
//		case T_CteScan:
//		case T_WorkTableScan:
//		case T_SubqueryScan:
//		{
//			if(planstate->type == oldTag )
//				planstate->type = newTag;
//			break;
//		}
//		default:
////			fprintf(stderr,"\n Unknown type... \n");
//			break;
//	}
//}
//
//
///*
// Print Initialization information
// */
//void
//printSystemInformation(void)
//{
//	fprintf(stdout, "\n");
//	fprintf(stdout, "\t\t*****************************************\n");
//	fprintf(stdout, "\t\t*                                       *\n");
//	fprintf(stdout, "\t\t*        --- Welcome to noDB ---        *\n");
//	fprintf(stdout, "\t\t*                                       *\n");
//	fprintf(stdout, "\t\t*****************************************\n");
//	fprintf(stdout, "\nExecution parameters:\n");
//	fprintf(stdout, "noDB plugin: ");
//	if(enable_invisible_db)
//		fprintf(stdout, "ENABLED\n");
//	else
//		fprintf(stdout, "DISABLED\n");
//
//	fprintf(stdout, "Collect End-Of-Tuple pointers: ");
//	if(enable_tuple_metapointers)
//		fprintf(stdout, "ENABLED\n");
//	else
//		fprintf(stdout, "DISABLED\n");
//
//	fprintf(stdout, "Adaptive index: ");
//	if(enable_internal_metapointers)
//		fprintf(stdout, "ENABLED\n");
//	else
//		fprintf(stdout, "DISABLED\n");
//
//	fprintf(stdout, "Push down selection: ");
//	if(enable_pushdown_select)
//		fprintf(stdout, "ENABLED\n");
//	else
//		fprintf(stdout, "DISABLED\n");
//
//	printConfiguration();
//
//}
//
//
//
///*
//	Find from which attribute we should start parsing and in which direction
//*/
//static void
//computeNextStep(int *stored, BitMap available, int maxAttr, int attribute, int *pointerID, int *directionVal, int *index)
//{
//	int i, j;
//	int i1,i2;
//	int j1,j2;
//
//	//If first attribute
//	if ( attribute == 0 )
//	{
//		*pointerID = 0;
//		*directionVal = 0;
//		*index = 0;
//		return;
//	}
//
//	//If last attribute
//	if ( attribute == maxAttr )
//	{
//		*pointerID = maxAttr;
//		*directionVal = 1;
//		*index = 0;
//		return;
//	}
//
//
//	for ( i1 = attribute; i1 > 0; i1--) {
//		if( stored[i1] ) {
//			break;
//		}
//	}
//
//	for ( i2 = attribute; i2 > 0; i2--) {
//		if( available.bitmap[i2] != 0 ) {
//			break;
//		}
//	}
//
//	if (i1 < i2)
//		i = i2;
//	else
//		i = i1;
//	if( i < 0)
//		i = 0;
//
//	for ( j1 = (attribute + 1); j1 < (maxAttr + 1); j1++) {
//		if( stored[j1] ) {
//			break;
//		}
//	}
//	for ( j2 = (attribute + 1); j2 < (maxAttr + 1); j2++) {
//		if( available.bitmap[j2] != 0 ) {
//			break;
//		}
//	}
//
//	if (j1 < j2)
//		j = j1;
//	else
//		j = j2;
//
//	if ( (abs(attribute - i) + 1 )  <= abs(attribute - j) )
//	{
//		if (i1 < i2)
//		{
//			*pointerID = i2;
//			*index = 1;
//		}
//		else
//		{
//			*pointerID = i1;
//			*index = 0;
//		}
//
//		*directionVal = 0;
//	}
//	else
//	{
//		if (j1 <= j2)
//		{
//			*pointerID = j1;
//			*index = 0;
//		}
//		else
//		{
//			*pointerID = j2;
//			*index = 1;
//		}
//		*directionVal = 1;
//	}
//}
//
///*
// *	Greedy algorithm: find the best pointer for each attribute
// */
//static void
//precomputeIndexAccesses2(int pos_ID, int numAtts,int numInterestingAtts, int *intAtts, bool *next_step, ParsingParameters *parameters, int *neededMapPositions, int *count)
//{
//	int i;
//	int j;
//	int k;
//	int pos = pos_ID;
//	int base;
//	int how_many;
//	int *stored;
//	int *consec_for;
//	int *consec_back;
//	BitMap available;
//	int pointerID;
//	int directionVal;
//	int index;
//	int found =0;
//
//	available = *InternalPositionalMap[pos].available;
//
//	consec_back	= (int*) malloc( numInterestingAtts * sizeof(int));
//	consec_for	= (int*) malloc( numInterestingAtts * sizeof(int));
//	stored   	= (int*) malloc( (numAtts + 1) * sizeof(int));
//
//	/*
//	 * Init stored
//	 * 1 valid
//	 * 0 NOT ;-)
//	 */
//	stored[0] = 1;
//	for ( i = 1; i < numAtts; i++ )
//		stored[i] = 0;
//	stored[numAtts] = 1;
//
//
//
//	for ( i = 0; i < numInterestingAtts; i++ )
//	{
//		consec_for[i] = 0;
//		consec_back[i] = 0;
//		parameters[i].attribute_id = -1;
//		parameters[i].how_many = -1;
//		parameters[i].direction = -1;
//		parameters[i].index = -1;
//	}
//
//	/*Compute number of consecutive attributes after attribute x*/
//	for ( i = 0; i < numInterestingAtts; i++ )
//	{
//		base = intAtts[i];
//		for ( j = (i + 1); j < numInterestingAtts; j++ )
//		{
//			if( (base + 1) ==  intAtts[j])
//			{
//				base++;
//				consec_for[i]++;
//			}
//			else
//				break;
//		}
//	}
//
//	/*Compute number of consecutive attributes before attribute x*/
//	for ( i = (numInterestingAtts - 1); i >= 0; i-- )
//	{
//		base = intAtts[i];
//		for ( j = (i - 1); j >=0 ; j-- )
//		{
//			if( (base - 1) ==  intAtts[j])
//			{
//				base--;
//				consec_back[i]++;
//			}
//			else
//				break;
//		}
//	}
//
///****************************************/
////	fprintf(stderr,"Interesting attributes: { ");
////	for ( i = 0; i < numInterestingAtts; i++ )
////		fprintf(stderr,"%d ",intAtts[i]);
////	fprintf(stderr,"}\n");
////
////	fprintf(stderr,"Forward: { ");
////	for ( i = 0; i < numInterestingAtts; i++ )
////		fprintf(stderr,"%d ",consec_for[i]);
////	fprintf(stderr,"}\n");
////
////	fprintf(stderr,"Backward: { ");
////	for ( i = 0; i < numInterestingAtts; i++ )
////		fprintf(stderr,"%d ",consec_back[i]);
////	fprintf(stderr,"}\n");
////
////	fprintf(stderr,"neededMapPositions Before {%d}: { ",*count);
////	for ( i = 0; i < (numAtts + 1); i++ )
////		fprintf(stderr,"%d ",neededMapPositions[i]);
////	fprintf(stderr,"}\n");
//
///****************************************/
//	/*
//	 *  a) read from attribute buffer --> 0
//	 *  b) read from bitmap --> 1
//	 *  c) Parse for new attributes forward  --> 2 with pos from index
//	 *  d) Parse for new attributes forward  --> 3 with pos from attribute buffer
//	 *  e) Parse for new attributes backward --> 4 with pos from index
//	 *  f) Parse for new attributes backward --> 5 with pos from attribute buffer
//	 */
//	pos = 0;
//	for ( j = 0; j < numInterestingAtts; j++ )
//	{
//		i = intAtts[j];
////		fprintf(stderr, "Attribute: %d\n",i);
//
//		if( available.bitmap[i] )
//			stored[i] = 1;
//
//		/* Pointer before interesting attribute */
//		if( !stored[i] )
//		{
//			computeNextStep(stored, available, numAtts, i, &pointerID, &directionVal, &index);
//			next_step[pos] = true;
//			if (index )
//				if(pointerID != numAtts && pointerID != 0) //We don't care about the first and the last pointer
//				{
//					//check if pointerID exists
//					found = 0;
//					for ( k = 0 ; k < *count; k++)
//					{
//						if (neededMapPositions[k] == pointerID)
//						{
//							found = 1;
//							break;
//						}
//					}
//					if (!found)
//					{ //Corner case in which due to parsing we are going to need more pointers than the precomputed!!!
//						neededMapPositions[*count] = pointerID;
//						(*count)++;
//					}
//				}
//
////			fprintf(stderr,"Direction = %d, index = %d, pointerID = %d\n",directionVal, index, pointerID);
//
//			if ( !directionVal )
//			{
//				//how_many --> pointers before + pointers after the attribute ;-)
//				how_many = (i - pointerID) + consec_for[j] + 1;
//				for ( k = pointerID ; k <= (pointerID + how_many); k++ )
//					stored[k] = 1;
//
//				parameters[pos].attribute_id = pointerID;
//				parameters[pos].how_many = how_many - 1;
//				parameters[pos].direction = directionVal;
//				parameters[pos].index = index;
//			}
//			else //Parse backward starting from pointerID
//			{
//				how_many = (pointerID - i) + consec_back[j] + 1;
//				for ( k = pointerID; k >= (pointerID - how_many); k-- )
//					stored[k] = 1;
//
//				parameters[pos].attribute_id = pointerID;
//				parameters[pos].how_many = how_many - 2;
//				parameters[pos].direction = directionVal;
//				parameters[pos].index = index;
//			}
//		}
//
//		pos++;
//		/* Pointer after interesting attribute */
//		if( available.bitmap[i + 1] )
//		{
//			stored[i+1] = 1;
//		}
//
//		if( !stored[i + 1] )
//		{
//			//Why not i+1???
//			computeNextStep(stored, available, numAtts, i, &pointerID, &directionVal, &index);
//			next_step[pos] = true;
//			if (index && available.bitmap[pointerID] == 1)
//				if(pointerID != numAtts && pointerID != 0)
//				{
//					//check if pointerID exists
//					found = 0;
//					for ( k = 0 ; k < *count; k++)
//					{
//						if (neededMapPositions[k] == pointerID)
//						{
//							found = 1;
//							break;
//						}
//					}
//					if (!found)
//					{
//						neededMapPositions[*count] = pointerID;
//						(*count)++;
//					}
//				}
//
////			fprintf(stderr,"Direction = %d, index = %d, pointerID = %d\n",directionVal, index, pointerID);
//
//			if ( !directionVal )
//			{
//				//how_many --> pointers before + pointers after the attribute ;-)
//				how_many = (i - pointerID) + consec_for[j] + 1;
//				for ( k = pointerID ; k <= (pointerID + how_many); k++ )
//					stored[k] = 1;
//
//				parameters[pos].attribute_id = pointerID;
//				parameters[pos].how_many = how_many - 1;
//				parameters[pos].direction = directionVal;
//				parameters[pos].index = index;
//			}
//			else //Parse backward starting from pointerID
//			{
//				how_many = (pointerID - i) + consec_back[j] + 1;
//				for ( k = pointerID; k >= (pointerID - how_many); k-- )
//					stored[k] = 1;
//
//				parameters[pos].attribute_id = pointerID;
//				parameters[pos].how_many = how_many - 2;
//				parameters[pos].direction = directionVal;
//				parameters[pos].index = index;
//			}
//		}
//		pos++;
//	}
//
//
//
////	fprintf(stderr,"nextStep: { ");
////	for ( i = 0; i < (2 * numInterestingAtts); i++ )
////	{
////		if(next_step[i] == false)
////			fprintf(stderr,"read ");
////		else
////			fprintf(stderr,"parse ");
////	}
////	fprintf(stderr,"}\n");
////
////	printBitMap(available);
////
////	fprintf(stderr,"stored: { ");
////	for ( i = 0; i < (numAtts + 1); i++ )
////		fprintf(stderr,"%d ",stored[i]);
////	fprintf(stderr,"}\n");
//
//
//
//	free(stored);
//	stored = NULL;
//
//	free(consec_back);
//	consec_back = NULL;
//
//	free(consec_for);
//	consec_for = NULL;
//
//}
//
//
//void
//printCopyStateData(int pos)
//{
//	int i;
//	CopyState temp = CopyExec[pos].cstate;
//
//	fprintf(stderr,"---------Pos = %d-----------\n",pos);
//
//	fprintf(stderr,"tupleRead = %ld\n",temp->tupleRead);
//	fprintf(stderr,"tupleReadMetapointers = %ld\n",temp->tupleReadMetapointers);
//	fprintf(stderr,"tupleStored = %d\n",temp->tupleStored);
//
//	fprintf(stderr,"interesting_attributes = {%d}: { ",temp->numOfInterestingAtt);
//	for ( i = 0; i < temp->nfields; i++ )
//		fprintf(stderr,"%d ",temp->interesting_attributes[i]);
//	fprintf(stderr,"}\n");
//
//	fprintf(stderr,"numOfQualAtt = {%d} \n",temp->numOfQualAtt);
//
//	fprintf(stderr,"neededMapPositions = {%d}: { ",temp->numOfneededMapPositions);
//	for ( i = 0; i < temp->numOfneededMapPositions; i++ )
//		fprintf(stderr,"%d ",temp->neededMapPositions[i]);
//	fprintf(stderr,"}\n");
//
//	fprintf(stderr,"defaultneededMapPositions = {%d}: { ",temp->numOfdefaultneededMapPositions);
//	for ( i = 0; i < temp->numOfdefaultneededMapPositions; i++ )
//		fprintf(stderr,"%d ",temp->defaultneededMapPositions[i]);
//	fprintf(stderr,"}\n");
//
//	fprintf(stderr,"toBeParsed = {%d}: { ",temp->numOftoBeParsed);
//	for ( i = 0; i < temp->numOftoBeParsed; i++ )
//		fprintf(stderr,"%d ",temp->toBeParsed[i]);
//	fprintf(stderr,"}\n");
//
//	fprintf(stderr,"----------------------------------\n");
//}
//

