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
#include "commands/explain.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "optimizer/planner.h"
#include "optimizer/clauses.h"
#include "parser/parse_relation.h"
#include "rewrite/rewriteHandler.h"
#include "storage/fd.h"
#include "tcop/tcopprot.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/numeric.h"
#include "utils/datum.h"

#include <time.h>



#include "snooping/common.h"
#include "snooping/queryDescriptor.h"


#include "noDB/NoDBCache.h"
#include "noDB/NoDBEOLCacheWorld.h"

//#include "noDB/NoDBExecutorWithTimer.h"
#include "noDB/NoDBExecutor.h"

#include "noDB/NoDBScan.h"
#include "noDB/NoDBScanStrategy.h"
#include "noDB/NoDBExecInfo.h"


#include "noDB/auxiliary/NoDBMalloc.h"
#include "noDB/auxiliary/NoDBPM.h"
#include "noDB/auxiliary/NoDBRelation.h"
#include "noDB/auxiliary/NoDBTimer.h"


#define MAX_COMMAND_SIZE 512
#define MAX_OPTION_SIZE 50


static const char BinarySignature[11] = "PGCOPY\n\377\r\n\0";

static NoDBPlanState_t  *NoDBPlanStateInit(NoDBScanStrategy_t *curStrategy, NoDBScanStrategyIterator_t  *it, char *relation);
static int              NoDBReadFromCacheOnly(NoDBPlanState_t *plan);
static TupleTableSlot   *(*NoDBGetExecPlan(NoDBScanState_t cstate)) (struct NoDBScanStateData_t *cstate, bool *pass);
static bool             (*NoDBGetReadFile(struct NoDBPlanState_t *plan)) (NoDBScanState_t cstate);
static long int         NoDBComputeBytesToSeek(NoDBCache_t *cache, NoDBRow_t firstTuple, NoDBRow_t lastTuple);


static void             ScanStateFree(NoDBScanState_t cstate);
static void             ScanStmtExecutionStatusFree(NoDBScanExecStatusStmt_t status, NoDBScanState_t cstate);


static NoDBScanState_t          GetScanState(const CopyStmt *stmt, const char *queryString);
static CopyStmt                 *GetQueryCopyStmt(const char *query_string);
static NoDBScanExecStatusStmt_t GetScanExecStatusStmt(NoDBScanState_t cstate);
static void                     NoDBScanStateInit(NoDBScanState_t cstate, ScanState *scanInfo);


static List             *CopyGetAttnums(TupleDesc tupDesc, Relation rel, List *attnamelist);
static void             ReceiveCopyBegin(NoDBScanState_t cstate);
static int              CopyGetData(NoDBScanState_t cstate, void *databuf, int minread, int maxread);
static bool             CopyGetInt32(NoDBScanState_t cstate, int32 *val);


static ScanState        *getProperScanState(PlanState *planstate);
static List             *traverseSubPlans(List *plans, PlannedStmt *top);
static List             *traverseMemberNodes(List *plans, PlanState **planstate, Plan *outer_plan, PlannedStmt *top);
static void             traverseQual(List *qual, Plan *plan, Plan *outer_plan, PlannedStmt *topPlan);
static void             recompute_limits(LimitState *node);



/*
 * Initialization Variables
 * Enable: InvisibleDB + extra features
 */
bool enable_invisible_db 			= false;
bool enable_tuple_metapointers  	= false;
bool enable_internal_metapointers 	= false;
bool enable_pushdown_select 		= false;





NoDBScanOperator_t *NoDBScanOperatorInit(ScanState *scanInfo)
{
    NoDBScanOperator_t *scanOper;

    char *filename;
    char *command;
    char *delimiter;
    bool header;
    char *relation = scanInfo->ss_currentRelation->rd_rel->relname.data;

    char *delimiterOption;
    char *headerOption;

    /* Extra check: Environment should be already loaded... */
    //TODO: update with the new version of the code that doesn't need init
    //TODO:NoDB specific fucntions + add time stamp if the snoop.conf has changed
    Assert(isLoaded());

    filename = getInputFilename(relation);
    delimiter = getDelimiter(relation);
    header = getHeader(relation);


    Assert(filename != NULL);
    /* *DP* for this test to pass, relation must be defined in scanInfo, be present in the snoop.conf file,
     * and the corresponding snoop parameters must be loaded correctly */
    if(delimiter == NULL)
        *delimiter = ',';


    scanOper = (NoDBScanOperator_t*)palloc(1 * sizeof(NoDBScanOperator_t));

    // Access data file using copy command: "COPY <relation> FROM '<link to data file>' WITH DELIMITER 'delimiter'"
    command = (char*)palloc(MAX_COMMAND_SIZE * sizeof(char));
    delimiterOption = (char*)palloc(MAX_OPTION_SIZE * sizeof(char));
    headerOption = (char*)palloc(MAX_OPTION_SIZE * sizeof(char));

    if ( strcmp( delimiter, "\\t") == 0  || strcmp( delimiter, "\\r") == 0 || strcmp( delimiter, "\\n") == 0)
        sprintf(delimiterOption, "DELIMITER E'%s'", delimiter);
    else
        sprintf(delimiterOption, "DELIMITER '%s'", delimiter);

    /*
     * *DP* Adding support for csv files with header.
     * Using the original structure of the COPY code (in csv mode), this option
     * defines a bool that is later used to ignore the first line of the file
     */
    if (header)
		sprintf(headerOption, "HEADER TRUE");
    else
		sprintf(headerOption, "HEADER FALSE");

    sprintf(command, "COPY %s FROM '%s' WITH (FORMAT csv, %s, %s);",relation, filename, delimiterOption, headerOption);
    fprintf(stderr,"%s\n",command);
    /* *DP* csv mode is now selected ("FORMAT csv" parameter)
     * In csv mode, the following default values are used :
     * '' (empty string) as NULL value
     * '"' (double quote) as quote character
     * '"' (double quote) as escape character in quoted field
     *
     * Text mode was previously used, for which defaults are
     * '\N' for NULL values and \t (tab) as delimiter
     * HEADER, quote and escape support is not available in text mode */

    scanOper->planCopyStmt  = GetQueryCopyStmt(command);
    scanOper->cstate        = GetScanState(scanOper->planCopyStmt, command);
    scanOper->status        = GetScanExecStatusStmt(scanOper->cstate);

    // Initialize NoDB specific fields of NoDBScanState_t
    NoDBScanStateInit(scanOper->cstate, scanInfo);

    return scanOper;
}


void NoDBScanStateReInit(ScanState *scanInfo)
{
    NoDBScanStrategy_t          *curStrategy;
    NoDBScanStrategyIterator_t  *it;

    NoDBScanState_t cstate = scanInfo->scanOper->cstate;
    char * relation = scanInfo->ss_currentRelation->rd_rel->relname.data;

    if (cstate->processed >= 0)
    {
	    // Before getting next update the size of the used caches
		NoDBFinalizeStrategy(cstate);

		// a) there is EOL
        if ( cstate->tupleRead !=  cstate->processed) {
        	// Move file pointer to the proper tuple
            if ( (NoDBCacheGetBegin(cstate->plan->eolCache) + NoDBCacheGetUsedRows(cstate->plan->eolCache)) >=  cstate->processed )
            {
                long int bytes = NoDBComputeBytesToSeek(cstate->plan->eolCache, cstate->tupleRead, cstate->processed);
                fseek(cstate->copy_file, bytes, SEEK_CUR);
                cstate->raw_buf_len = 0;
                cstate->raw_buf_index = 0;
                cstate->tupleStored = 0;
                cstate->tupleRead = cstate->processed;
            }
            else
    			Assert(false);
        }

        // Before asking for a new strategy examine if there are more tuples in the file
    	if (cstate->tupleRead >= (NoDBCacheGetBegin(cstate->plan->eolCache) + NoDBCacheGetUsedRows(cstate->plan->eolCache)))
        {
            if(!NoDBTryReFillRawBuf(cstate)) {//We have to read more tuples from the file -- No Re-scan
//                cstate->execNoDBPlan = (NoDBBreakDown) ? &NoDBExecPlanWT : &NoDBExecPlanWT;
            	cstate->execNoDBPlan = NoDBGetExecPlan (cstate);
                cstate->plan->readFile = NoDBGetReadFile(cstate->plan);
                return;
            }
        }
    }

    //Restart file pointer
	if(cstate->copy_file != NULL) {
		 fseek(cstate->copy_file, 0, SEEK_SET);
	}

	cstate->processed = 0;
	cstate->tupleRead = 0;
	cstate->tupleStored = 0;
    cstate->raw_buf_len = 0;
    cstate->raw_buf_index = 0;


	//Re-init NoDB parameters
    //Release any locks and ask for new writing set
    if(NoDBExecReleaseCacheLocks(cstate->execInfo, cstate->readAllAtts))
    {
    	NoDBColVectorDestroy(cstate->writeDataCols);
    	//cstate->writeDataCols = NoDBNothingPolicy();
    	cstate->writeDataCols = NoDBAggressiveCachePolicy(cstate->execInfo, cstate->readAllAtts);
    }
    if(NoDBExecReleasePMLocks(cstate->execInfo, cstate->readAllAtts))
    {
    	NoDBColVectorDestroy(cstate->writePositionCols);
    	cstate->writePositionCols = NoDBAggressivePMPolicy(cstate->execInfo, cstate->readAllAtts);
//    	cstate->writePositionCols = NoDBNothingPolicy();
    }


    it = NoDBScanStrategyIterator(cstate->execInfo->rel, getQueryLimit(),
            cstate->readFilterAtts, cstate->readRestAtts, cstate->writeDataCols, cstate->writePositionCols);
    curStrategy = NoDBScanStrategyIteratorGet(it);
    prettyPrint(curStrategy);

    //Extend EOL cache if necessary
    if((NoDBCacheGetMaxRows(cstate->plan->eolCache) - NoDBCacheGetUsedRows(cstate->plan->eolCache)) < cstate->plan->curStrategy->nrows)
        NoDBCacheSetMaxRows(cstate->plan->eolCache, cstate->processed + cstate->plan->curStrategy->nrows);

    //Re-initialize plan
    cstate->plan->curStrategy   = curStrategy;
    cstate->plan->iterator      = it;
    cstate->plan->eolCache      = NoDBInitEOLCache(relation, curStrategy->nrows);
    cstate->plan->nEOL          = NoDBCacheGetUsedRows(cstate->plan->eolCache);

    cstate->plan->hasEOL = ( cstate->plan->nEOL > 0) ? true : false;
    cstate->plan->readFile = NoDBGetReadFile(cstate->plan);

    cstate->plan->nRows = curStrategy->nrows;
    cstate->plan->nMin = cstate->plan->nRows;

    //Read not only from cache
    if( !NoDBReadFromCacheOnly(cstate->plan))
    {
        if (cstate->plan->nEOL > 0 )
        	cstate->plan->nMin = (cstate->plan->nEOL < cstate->plan->nRows) ? cstate->plan->nEOL : cstate->plan->nRows;
    }
    cstate->plan->tuplesToRead = cstate->plan->nMin;
    cstate->execNoDBPlan = NoDBGetExecPlan (cstate);
}



void NoDBScanOperatorDestroy(NoDBScanOperator_t *scanOper)
{
    ScanStmtExecutionStatusFree(scanOper->status, scanOper->cstate);
    ScanStateFree(scanOper->cstate);
    pfree(scanOper);
}



static void
NoDBScanStateInit(NoDBScanState_t cstate, ScanState *scanInfo)
{
    int i;
    int natts;
    int size;
    char *relation;
    NoDBScanStrategy_t          *curStrategy;
    NoDBScanStrategyIterator_t  *it;


    relation = scanInfo->ss_currentRelation->rd_rel->relname.data;
    natts = cstate->tupDesc->natts;

    // Used to temporally store new internal metapointers
    cstate->attributes = (NoDBPMPair_t*) palloc( (natts + 1) * sizeof(NoDBPMPair_t));
    cstate->attributes[0].pointer = 0;

    cstate->readAllAtts    = getQueryAttributes( cstate->tupDesc, relation);
    if(scanInfo->ps.qual)
    {
        cstate->readFilterAtts = getQueryFilterAttributes(cstate->tupDesc, relation);
        cstate->readRestAtts   = getQueryRestAttributes(cstate->readAllAtts, cstate->readFilterAtts);
    }
    else
    {
        cstate->readFilterAtts = NoDBColVectorInit(0);
        cstate->readRestAtts   = getQueryAttributes( cstate->tupDesc, relation);
    }

    // Init values and nulls members for the readinf
    for(i = 0; i < natts; i++)
    {
        cstate->values[i] = (Datum) 0;
        cstate->nulls[i] = true;
    }

//    for(i = 0; i < NoDBColVectorSize(cstate->readAllAtts); i++)
//    {
//        NoDBCol_t col = NoDBColVectorGet(cstate->readAllAtts, i);
//        cstate->nulls[col] = false;
//    }


    //Init Relation if not already there
    cstate->execInfo = NoDBGetExecInfo(relation);
    if( !cstate->execInfo )
    {
        NoDBRelation_t *rel = NoDBRelationInit(relation);
        for ( i = 0; i < natts; i++) {
            NoDBRelationAddColumn(rel, sizeof(NoDBDatum_t), cstate->attr[i]->attbyval);
        }
        cstate->execInfo = NoDBExecInfoInit(rel, cstate->filename, 0, 0);
        //Estimation
        NoDBExecSetNumberOffBlocks(cstate->execInfo);
    }

    if ( (size = NoDBColVectorSize(cstate->readAllAtts)) > 0 )
    	cstate->lastfield = NoDBColVectorGet(cstate->readAllAtts, size - 1);
    else
    	cstate->lastfield = 0;

    //TODO:Add some policy here...
//    cstate->writeDataCols = NoDBNothingPolicy();
  cstate->writeDataCols = NoDBAggressiveCachePolicy(cstate->execInfo, cstate->readAllAtts);

  cstate->writePositionCols = NoDBAggressivePMPolicy(cstate->execInfo, cstate->readAllAtts);
//    cstate->writePositionCols = NoDBNothingPolicy();


    it          = NoDBScanStrategyIterator(cstate->execInfo->rel, getQueryLimit(),
            cstate->readFilterAtts, cstate->readRestAtts, cstate->writeDataCols, cstate->writePositionCols);
    curStrategy = NoDBScanStrategyIteratorGet(it);
    prettyPrint(curStrategy);

    cstate->plan             = NoDBPlanStateInit(curStrategy, it, relation);
    cstate->ss_ScanTupleSlot = scanInfo->ss_ScanTupleSlot;
    cstate->qual             = scanInfo->ps.qual;
    cstate->econtext         = scanInfo->ps.ps_ExprContext;

    if(NoDBBreakDown) {
        cstate->timer = (NoDBTimer_t*) palloc(1 * sizeof(NoDBTimer_t));
        NoDBTimerSetZero(cstate->timer);
    }
    cstate->execNoDBPlan = NoDBGetExecPlan (cstate);

}


static
NoDBPlanState_t* NoDBPlanStateInit(NoDBScanStrategy_t *curStrategy, NoDBScanStrategyIterator_t  *it, char *relation)
{
    NoDBPlanState_t *plan;

    plan = (NoDBPlanState_t*)palloc( 1 * sizeof(NoDBPlanState_t));

    plan->curStrategy = curStrategy;
    plan->iterator = it;

    plan->eolCache = NoDBInitEOLCache(relation, curStrategy->nrows);
    plan->nEOL     = NoDBCacheGetUsedRows(plan->eolCache);

    plan->hasEOL = ( plan->nEOL > 0) ? true : false;
    plan->readFile = NoDBGetReadFile(plan);


    plan->nRows = curStrategy->nrows;
    plan->nMin = plan->nRows;

    //Read only from cache
    if( !NoDBReadFromCacheOnly(plan))
    {
        if (plan->nEOL > 0 )
            plan->nMin = (plan->nEOL < plan->nRows) ? plan->nEOL : plan->nRows;
    }
    plan->tuplesToRead = plan->nMin;

    return plan;
}


/*
 * UpdateStrategy()
 *
 */
void NoDBUpdateStrategy(NoDBScanState_t cstate)
{
	NoDBPlanState_t *plan = cstate->plan;

	// Update policy
	if ( plan->nEOL > 0)
		plan->nEOL -= plan->tuplesToRead;
	if ( plan->nRows > 0)
		plan->nRows -= plan->tuplesToRead;

	if( plan->nEOL == 0) {
		plan->hasEOL = false;
	    plan->readFile = NoDBGetReadFile(cstate->plan);
	}

	// Get new strategy using the iterator
	if (plan->nRows == 0)
	{
	    // Before getting next update the size of the used caches
		NoDBFinalizeStrategy(cstate);

		// a) there is EOL
        if ( cstate->tupleRead !=  cstate->processed) {
        	// Move file pointer to the proper tuple
            if ( (NoDBCacheGetBegin(plan->eolCache) + NoDBCacheGetUsedRows(plan->eolCache)) >=  cstate->processed )
            {
                long int bytes = NoDBComputeBytesToSeek(plan->eolCache, cstate->tupleRead, cstate->processed);
                fseek(cstate->copy_file, bytes, SEEK_CUR);
                cstate->raw_buf_len = 0;
                cstate->raw_buf_index = 0;
                cstate->tupleStored = 0;
                cstate->tupleRead = cstate->processed;
            }
            else
    			Assert(false);
        }

        // Test if there are more tuples
        if (cstate->tupleRead >= (NoDBCacheGetBegin(plan->eolCache) + NoDBCacheGetUsedRows(plan->eolCache)))
        {
            if(NoDBTryReFillRawBuf(cstate)) {//hit_eof -- Strategy to read from file
//                cstate->execNoDBPlan = (NoDBBreakDown) ? &NoDBExecPlanWT : &NoDBExecPlan;
                cstate->execNoDBPlan = &NoDBExecPlan;
                cstate->plan->readFile = NoDBGetReadFile(cstate->plan);
                return;
            }
        }

        // Get next strategy
        NoDBScanStrategyIteratorNext(plan->iterator);
        plan->curStrategy = NoDBScanStrategyIteratorGet(plan->iterator);
        prettyPrint(plan->curStrategy);

        // Extend EOL cache if needed
        if((NoDBCacheGetMaxRows(plan->eolCache) - NoDBCacheGetUsedRows(plan->eolCache)) < plan->curStrategy->nrows)
            NoDBCacheSetMaxRows(plan->eolCache, cstate->processed + plan->curStrategy->nrows);

        plan->nRows = plan->curStrategy->nrows;
        plan->nMin = plan->nRows;
        // We will access the file (directly or using PM)
        if( !NoDBReadFromCacheOnly(cstate->plan)) {
            if ( plan->nEOL > 0 )
                plan->nMin = (plan->nEOL < plan->nRows) ? plan->nEOL : plan->nRows;
        }
        cstate->execNoDBPlan = NoDBGetExecPlan (cstate);
	}
	else
	    plan->nMin = plan->nRows;

	plan->tuplesToRead = plan->nMin;
}


void NoDBFinalizeStrategy(NoDBScanState_t cstate)
{
	int i;
	NoDBPlanState_t *plan = cstate->plan;

	for (i = 0; i < plan->curStrategy->nwriteToCacheByRef; i++)
    {
        NoDBRow_t begin = NoDBCacheGetBegin(plan->curStrategy->writeToCacheByRef[i].cache);
        NoDBCacheSetUsedRows(plan->curStrategy->writeToCacheByRef[i].cache, cstate->processed - begin);
    }

    for (i = 0; i < plan->curStrategy->nwriteToCacheByValue; i++)
    {
        NoDBRow_t begin = NoDBCacheGetBegin(plan->curStrategy->writeToCacheByValue[i].cache);
        NoDBCacheSetUsedRows(plan->curStrategy->writeToCacheByValue[i].cache, cstate->processed - begin);
    }

    //Cache positions
    for (i = 0; i < plan->curStrategy->nwriteToPM; i++)
    {
        NoDBRow_t begin = NoDBCacheGetBegin(plan->curStrategy->writeToPM[i].cache);
        NoDBCacheSetUsedRows(plan->curStrategy->writeToPM[i].cache, cstate->processed - begin);
    }

    //Update number of rows
    NoDBExecSetNumberOffRows(cstate->execInfo);
}

static TupleTableSlot *(*NoDBGetExecPlan(NoDBScanState_t cstate)) (struct NoDBScanStateData_t *cstate, bool *pass)
{
    TupleTableSlot *(*pointer) (struct NoDBScanStateData_t *cstate, bool *pass);

//    if ( NoDBBreakDown )
//    {
//        if( !NoDBReadFromCacheOnly(cstate->plan) )
//            pointer = (cstate->qual && NoDBColVectorSize(cstate->readFilterAtts) > 0) ? &NoDBExecPlanWithFiltersWT : &NoDBExecPlanWT;
//        else
//            pointer = (cstate->qual && NoDBColVectorSize(cstate->readFilterAtts) > 0) ? &NoDBExecPlanWithFiltersCacheOnlyWT : &NoDBExecPlanCacheOnlyWT;
//    }
//    else
    {
        if( !NoDBReadFromCacheOnly(cstate->plan) )
            pointer = (cstate->qual && NoDBColVectorSize(cstate->readFilterAtts) > 0) ? &NoDBExecPlanWithFilters : &NoDBExecPlan;
        else
            pointer = (cstate->qual && NoDBColVectorSize(cstate->readFilterAtts) > 0) ? &NoDBExecPlanWithFiltersCacheOnly : &NoDBExecPlanCacheOnly;

    }
    return pointer;
}

//    bool                        (*readFile) (NoDBScanState_t cstate);

static bool  (*NoDBGetReadFile(struct NoDBPlanState_t *plan)) (NoDBScanState_t cstate)
{
    bool (*pointer) (NoDBScanState_t cstate);

//    if ( NoDBBreakDown ) {
//        pointer = (plan->nEOL > 0) ? &NoDBGetNextTupleFromFileWithEOLWT : &NoDBGetNextTupleFromFileWT;
//    }
//    else
    {
        pointer = (plan->nEOL > 0) ? &NoDBGetNextTupleFromFileWithEOL : &NoDBGetNextTupleFromFile;
    }
    return pointer;
}


static int
NoDBReadFromCacheOnly(NoDBPlanState_t *plan)
{
    if ( (plan->curStrategy->nreadPostFilterWithPM == 0 && plan->curStrategy->nreadPreFilterWithPM == 0 && NoDBColVectorSize(plan->curStrategy->readPreFilterWithFile) == 0) &&
          (plan-> curStrategy->nreadPostFilterWithPM == 0 && plan->curStrategy->nreadPostFilterViaPM == 0 && NoDBColVectorSize(plan->curStrategy->readPostFilterWithFile) == 0) &&
          (plan->curStrategy->nreadPostFilterWithCache > 0 || plan->curStrategy->nreadPreFilterWithCache > 0) )
        return 1;

    return 0;
}


static long int
NoDBComputeBytesToSeek(NoDBCache_t *cache, NoDBRow_t firstTuple, NoDBRow_t lastTuple)
{
	int i;
	unsigned long bytes = 0;
	for ( i = firstTuple ; i < lastTuple; i++)
		bytes += NoDBCacheGetShortInt(cache, 0, i);
	return bytes;
}



static void
ScanStateFree(NoDBScanState_t cstate)
{

    pfree(cstate->fcinfo);
    pfree(cstate->values);
    pfree(cstate->nulls);
    pfree(cstate->field_strings);
    pfree(cstate->force_notnull_flags);
    pfree(cstate->attrlen);

    pfree(cstate->attributes);

    if( NoDBBreakDown ) {
        pfree(cstate->timer);
    }

    NoDBColVectorDestroy(cstate->readAllAtts);
    NoDBColVectorDestroy(cstate->readFilterAtts);
    NoDBColVectorDestroy(cstate->readRestAtts);


    NoDBScanStrategyIteratorDestroy(cstate->plan->iterator);
    pfree(cstate->plan);

    /* Handle queued AFTER triggers */
    AfterTriggerEndQuery(cstate->estate);
    ExecResetTupleTable(cstate->estate->es_tupleTable, false);


    pfree(cstate->attribute_buf.data);
    pfree(cstate->line_buf.data);
    pfree(cstate->raw_buf);
    pfree(cstate);
}


static void
ScanStmtExecutionStatusFree(NoDBScanExecStatusStmt_t status, NoDBScanState_t cstate)
{

    error_context_stack = status.errcontext.previous;

    pfree(status.in_functions);
    pfree(status.typioparams);
    pfree(status.defmap);
    pfree(status.defexprs);

    ExecCloseIndices(status.resultRelInfo);

    //No need to free...
//  FreeExecutorState(CopyExec[pos].status.estate);

    if (!(cstate->filename == NULL))
    {
        if (FreeFile(cstate->copy_file))
            ereport(ERROR,
                    (errcode_for_file_access(),
                     errmsg("could not read from file \"%s\": %m",
                             cstate->filename)));
    }

    /*
     * If we skipped writing WAL, then we need to sync the heap (but not
     * indexes since those use WAL anyway)
     */
    if (status.hi_options & HEAP_INSERT_SKIP_WAL)
        heap_sync(cstate->rel);

}


/*
 *	Create CopyStmt struct for Copy query: query_string
 */
static CopyStmt *
GetQueryCopyStmt(const char *query_string)
{
//	MemoryContext oldcontext;
	List	   *parsetree_list = NIL;
	List 	   *plantree_list  = NIL;
	ListCell   *parsetree_item;
	int length = 0;

	/*
	 * Do basic parsing of the query or queries (this should be safe even if
	 * we are in aborted transaction state!)
	 */
	parsetree_list = pg_parse_query(query_string);

	/*
	 * Run through the raw parsetree and process each it.
	 */
	length = list_length(parsetree_list);
	if( length != 1)
	{
		elog(INFO, "More than one raw parsed trees returned...");
		return NULL;
	}

	foreach(parsetree_item, parsetree_list)
	{
		Node	   *parsetree = (Node *) lfirst(parsetree_item);
		List	   *querytree_list;

		querytree_list = pg_analyze_and_rewrite(parsetree, query_string,
												NULL, 0);
		plantree_list = pg_plan_queries(querytree_list, 0, NULL);
	}

	if(!IsA(((Node *) linitial(plantree_list)), CopyStmt))
	{
		fprintf(stderr, "Wrong type of plan...");
		return NULL;
	}
	return (CopyStmt*) linitial(plantree_list);
}


/*
 * *DP* Based on DoCopy(const CopyStmt *stmt, const char *queryString) in src/backend/commands/copy.c
 * NoDB: We care only for the case of "delimiter"
 * (However, I did not remove the other fields in case we need them in a future version)
 * *DP* We now also use the "header" and "null" options (see statement built in NoDBScanOperatorInit)
 * Some possible parameters are not used for now (format, quote, escape, force_not_null...)
 * Output parameters such as null_print, force_null, ...
 * are not used, as postgresRAW never prints anything (only file reading)
 */
static NoDBScanState_t
GetScanState(const CopyStmt *stmt, const char *queryString)
{
	NoDBScanState_t	cstate;
	bool		is_from = stmt->is_from;
	bool		pipe = (stmt->filename == NULL);
	List	   *attnamelist = stmt->attlist;
	List	   *force_quote = NIL;
	List	   *force_notnull = NIL;
	bool		force_quote_all = false;
	bool		format_specified = false;
	AclMode		required_access = (is_from ? ACL_INSERT : ACL_SELECT);
	AclMode		relPerms;
	AclMode		remainingPerms;
	ListCell   *option;
	TupleDesc	tupDesc;
	int			num_phys_attrs;

	/* Allocate workspace and zero all fields */
	cstate = (NoDBScanStateData_t *) palloc0(sizeof(NoDBScanStateData_t));
	/* Extract options from the statement node tree */
	foreach(option, stmt->options)
	{
		DefElem    *defel = (DefElem *) lfirst(option);

		if (strcmp(defel->defname, "format") == 0)
		{
			char	   *fmt = defGetString(defel);

			if (format_specified)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			format_specified = true;
			if (strcmp(fmt, "text") == 0)
				cstate->csv_mode = false;
				 /* default format */
			else if (strcmp(fmt, "csv") == 0) /* *DP* We might want to use the csv mode instead of text */
				cstate->csv_mode = true;
			else if (strcmp(fmt, "binary") == 0)
				cstate->binary = true;
			else
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("COPY format \"%s\" not recognized", fmt)));
		}
//		else if (strcmp(defel->defname, "oids") == 0)
//		{
//			if (cstate->oids)
//				ereport(ERROR,
//						(errcode(ERRCODE_SYNTAX_ERROR),
//						 errmsg("conflicting or redundant options")));
//			cstate->oids = defGetBoolean(defel);
//		}
		else if (strcmp(defel->defname, "delimiter") == 0)
		{
			if (cstate->delim)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->delim = defGetString(defel);
		}
		else if (strcmp(defel->defname, "null") == 0)
		{
			if (cstate->null_print)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->null_print = defGetString(defel);
		}
		else if (strcmp(defel->defname, "header") == 0)
		{
			if (cstate->header_line)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->header_line = defGetBoolean(defel);
		}
		else if (strcmp(defel->defname, "quote") == 0)
		{
			if (cstate->quote)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->quote = defGetString(defel);
		}
		else if (strcmp(defel->defname, "escape") == 0)
		{
			if (cstate->escape)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->escape = defGetString(defel);
		}
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
		else
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("option \"%s\" not recognized",
							defel->defname)));
	}

	//fprintf(stdout,"cstate->csv_mode : %s\n", cstate->csv_mode ? "True" : "False");

	/*
	 * Check for incompatible options (must do these two before inserting
	 * defaults)
	 */
//	if (cstate->binary && cstate->delim)
//		ereport(ERROR,
//				(errcode(ERRCODE_SYNTAX_ERROR),
//				 errmsg("cannot specify DELIMITER in BINARY mode")));
//
//	if (cstate->binary && cstate->null_print)
//		ereport(ERROR,
//				(errcode(ERRCODE_SYNTAX_ERROR),
//				 errmsg("cannot specify NULL in BINARY mode")));

	/* Set defaults for omitted options */
	if (!cstate->delim)
		cstate->delim = cstate->csv_mode ? "," : "\t";

	if (!cstate->null_print)
		cstate->null_print = cstate->csv_mode ? "" : "\\N";
	cstate->null_print_len = strlen(cstate->null_print);

	if (cstate->csv_mode)
	{
		if (!cstate->quote)
			cstate->quote = "\"";
		if (!cstate->escape)
			cstate->escape = cstate->quote;
	}

	/* Only single-byte delimiter strings are supported. */
	if (strlen(cstate->delim) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			  errmsg("COPY delimiter must be a single one-byte character")));

	/* Disallow end-of-line characters */
	if (strchr(cstate->delim, '\r') != NULL ||
		strchr(cstate->delim, '\n') != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("COPY delimiter cannot be newline or carriage return")));

//	if (strchr(cstate->null_print, '\r') != NULL ||
//		strchr(cstate->null_print, '\n') != NULL)
//		ereport(ERROR,
//				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
//				 errmsg("COPY null representation cannot use newline or carriage return")));

	/*
	 * Disallow unsafe delimiter characters in non-CSV mode.  We can't allow
	 * backslash because it would be ambiguous.  We can't allow the other
	 * cases because data characters matching the delimiter must be
	 * backslashed, and certain backslash combinations are interpreted
	 * non-literally by COPY IN.  Disallowing all lower case ASCII letters is
	 * more than strictly necessary, but seems best for consistency and
	 * future-proofing.  Likewise we disallow all digits though only octal
	 * digits are actually dangerous.
	 */
	if (!cstate->csv_mode &&
		strchr("\\.abcdefghijklmnopqrstuvwxyz0123456789",
			   cstate->delim[0]) != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("COPY delimiter cannot be \"%s\"", cstate->delim)));

	/* Check header */
	if (!cstate->csv_mode && cstate->header_line)
	if (cstate->header_line)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY HEADER available only in CSV mode")));

	/* Check quote */
	if (!cstate->csv_mode && cstate->quote != NULL)
	if (cstate->quote != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY quote available only in CSV mode")));

	if (cstate->csv_mode && strlen(cstate->quote) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY quote must be a single one-byte character")));

	if (cstate->csv_mode && cstate->delim[0] == cstate->quote[0])
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("COPY delimiter and quote must be different")));

	/* Check escape */
	if (!cstate->csv_mode && cstate->escape != NULL)
	if (cstate->escape != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY escape available only in CSV mode")));

	if (cstate->csv_mode && strlen(cstate->escape) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY escape must be a single one-byte character")));

//	/* Check force_quote */
//	if (!cstate->csv_mode && (force_quote != NIL || force_quote_all))
//		ereport(ERROR,
//				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//				 errmsg("COPY force quote available only in CSV mode")));
//	if ((force_quote != NIL || force_quote_all) && is_from)
//		ereport(ERROR,
//				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//				 errmsg("COPY force quote only available using COPY TO")));
//
//	/* Check force_notnull */
//	if (!cstate->csv_mode && force_notnull != NIL)
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
	/* Don't allow the CSV quote char to appear in the null string. */
	if (cstate->csv_mode &&
		strchr(cstate->null_print, cstate->quote[0]) != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("CSV quote character must not appear in the NULL specification")));

	/* Disallow file COPY except to superusers. */
	if (!pipe && !superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to COPY to or from a file"),
				 errhint("Anyone can COPY to stdout or from stdin. "
						 "psql's \\copy command also works for anyone.")));

	if (stmt->relation)
	{
		Assert(!stmt->query);
		cstate->queryDesc = NULL;

		/* Open and lock the relation, using the appropriate lock type. */
		cstate->rel = heap_openrv(stmt->relation,
							 (is_from ? RowExclusiveLock : AccessShareLock));

		tupDesc = RelationGetDescr(cstate->rel);

		/* Check relation permissions. */
		relPerms = pg_class_aclmask(RelationGetRelid(cstate->rel), GetUserId(),
									required_access, ACLMASK_ALL);
		remainingPerms = required_access & ~relPerms;
		if (remainingPerms != 0)
		{
			/* We don't have table permissions, check per-column permissions */
			List	   *attnums;
			ListCell   *cur;

			attnums = CopyGetAttnums(tupDesc, cstate->rel, attnamelist);
			foreach(cur, attnums)
			{
				int			attnum = lfirst_int(cur);

				if (pg_attribute_aclcheck(RelationGetRelid(cstate->rel),
										  attnum,
										  GetUserId(),
										  remainingPerms) != ACLCHECK_OK)
					aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_CLASS,
								   RelationGetRelationName(cstate->rel));
			}
		}

		/* check read-only transaction */
		if (XactReadOnly && is_from && !cstate->rel->rd_islocaltemp)
			PreventCommandIfReadOnly("COPY FROM");

//		/* Don't allow COPY w/ OIDs to or from a table without them */
//		if (cstate->oids && !cstate->rel->rd_rel->relhasoids)
//			ereport(ERROR,
//					(errcode(ERRCODE_UNDEFINED_COLUMN),
//					 errmsg("table \"%s\" does not have OIDs",
//							RelationGetRelationName(cstate->rel))));
	}
	else /* i.e. !(stmt->relation)
		  * SnoopDB: Failed to define relation in the input command
		  * *DP* Pretty sure this is not used in the case of PostgresRAW :
		  * the code below makes sense only in case of an original COPY statement
		  * A relation name was not found by GetQueryCopyStmt() when parsing the "command"
		  * string and building a CopyStmt (the "*stmt" arg of this function)
		  * In the real COPY case, it means we have a "command" like COPY (SELECT stmt) TO output
		  * In case of PostgresRAW :
		  * NoDBScanOperatorInit created the "command"'s COPY statement with a well-defined
		  * "relation" parameter. If the relation name was not found in snoop.conf, an error
		  * should have occurred at the beginning of NoDBScanOperatorInit.
		  * This does not however prove that the relation exists in DB. Maybe GetQueryCopyStmt()
		  * returns a CopyStmt* ("stmt" arg of this function) with an empty ->relation field when
		  * the relation does not exist in the database ???
		  * Still, the following code does not deal with this case.
		  **/
	{
		fprintf(stderr,"No relation defined in NoDB copy statement, case not handled\n");
	}
//		List	   *rewritten;
//		Query	   *query;
//		PlannedStmt *plan;
//		DestReceiver *dest;
//
//		Assert(!is_from);
//		cstate->rel = NULL;
//
//		/* Don't allow COPY w/ OIDs from a select */
//		if (cstate->oids)
//			ereport(ERROR,
//					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
//					 errmsg("COPY (SELECT) WITH OIDS is not supported")));
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
//		 /* *DP* Accessing the subquery ? (i.e. SELECT stmt of "COPY (SELECT stmt) TO output") */
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

	/* Generate or convert list of attributes to process */
	cstate->attnumlist = CopyGetAttnums(tupDesc, cstate->rel, attnamelist);

	num_phys_attrs = tupDesc->natts;

	/* *DP* FORCE_QUOTE option : only for COPY .. TO .. statements */
//	/* Convert FORCE QUOTE name list to per-column flags, check validity */
//	cstate->force_quote_flags = (bool *) palloc0(num_phys_attrs * sizeof(bool));
//	if (force_quote_all)
//	{
//		int			i;
//
//		for (i = 0; i < num_phys_attrs; i++)
//			cstate->force_quote_flags[i] = true;
//	}
//	else if (force_quote)
//	{
//		List	   *attnums;
//		ListCell   *cur;
//
//		attnums = CopyGetAttnums(tupDesc, cstate->rel, force_quote);
//
//		foreach(cur, attnums)
//		{
//			int			attnum = lfirst_int(cur);
//
//			if (!list_member_int(cstate->attnumlist, attnum))
//				ereport(ERROR,
//						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
//				   errmsg("FORCE QUOTE column \"%s\" not referenced by COPY",
//						  NameStr(tupDesc->attrs[attnum - 1]->attname))));
//			cstate->force_quote_flags[attnum - 1] = true;
//		}
//	}

	/* Convert FORCE NOT NULL name list to per-column flags, check validity */
	cstate->force_notnull_flags = (bool *) palloc0(num_phys_attrs * sizeof(bool));
	if (force_notnull)
	{
		List	   *attnums;
		ListCell   *cur;

		attnums = CopyGetAttnums(tupDesc, cstate->rel, force_notnull);

		foreach(cur, attnums)
		{
			int			attnum = lfirst_int(cur);

			if (!list_member_int(cstate->attnumlist, attnum))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
				errmsg("FORCE NOT NULL column \"%s\" not referenced by COPY",
					   NameStr(tupDesc->attrs[attnum - 1]->attname))));
			cstate->force_notnull_flags[attnum - 1] = true;
		}
	}

	/* Set up variables to avoid per-attribute overhead. */
	initStringInfo(&cstate->attribute_buf);
	initStringInfo(&cstate->line_buf);
	cstate->line_buf_converted = false;

	cstate->raw_buf = (char *) palloc(RAW_BUF_SIZE + 1);
	cstate->raw_buf_index = cstate->raw_buf_len = 0;
	cstate->processed = 0;
	cstate->tupleRead = 0;

	/*
	 * Set up encoding conversion info.  Even if the client and server
	 * encodings are the same, we must apply pg_client_to_server() to validate
	 * data in multibyte encodings.
	 */
	cstate->client_encoding = pg_get_client_encoding();
	cstate->need_transcoding =
		(cstate->client_encoding != GetDatabaseEncoding() ||
		 pg_database_encoding_max_length() > 1);
	/* See Multibyte encoding comment above */
	cstate->encoding_embeds_ascii = PG_ENCODING_IS_CLIENT_ONLY(cstate->client_encoding);

	cstate->copy_dest = COPY_FILE;		/* default */
	cstate->filename = stmt->filename;

//	if (is_from)
//		CopyFrom(cstate);		/* copy from file to database */
//	else
//		DoCopyTo(cstate);		/* copy from database to file */

	/* *DP* The original query processing for real COPY statements was done above,
	 * but it is just an init for PostgresRAW
	 * The GetScanExecStatusStmt function below is based on CopyFrom, and it basically
	 * does the file reading. Thus the lock obtained here on the corresponding table is
	 * not valid anymore when GetScanExecStatusStmt is called (released 10 lines below)
	 * No problem as it was not actually needed ? (Files are read-only in PostgresRAW, afaik.)
	 */

	//Close Heap before proceeding... (We don't need it any more)
	/*
	 * Close the relation or query.  If reading, we can release the
	 * AccessShareLock we got; if writing, we should hold the lock until end
	 * of transaction to ensure that updates will be committed before lock is
	 * released.
	 */
	if (cstate->rel)
		heap_close(cstate->rel, (is_from ? NoLock : AccessShareLock));
	else
	{
		/* Close down the query and free resources. */
		ExecutorEnd(cstate->queryDesc);
		FreeQueryDesc(cstate->queryDesc);
		PopActiveSnapshot();
	}

	return cstate;
}

/*
 * CopyGetAttnums - build an integer list of attnums to be copied
 *
 * The input attnamelist is either the user-specified column list,
 * or NIL if there was none (in which case we want all the non-dropped
 * columns).
 *
 * rel can be NULL ... it's only used for error reports.
 */
static List *
CopyGetAttnums(TupleDesc tupDesc, Relation rel, List *attnamelist)
{
	List	   *attnums = NIL;

	if (attnamelist == NIL)
	{
		/* Generate default column list */
		Form_pg_attribute *attr = tupDesc->attrs;
		int			attr_count = tupDesc->natts;
		int			i;

		for (i = 0; i < attr_count; i++)
		{
			if (attr[i]->attisdropped)
				continue;
			attnums = lappend_int(attnums, i + 1);
		}
	}
	else
	{
		/* Validate the user-supplied list and extract attnums */
		ListCell   *l;

		foreach(l, attnamelist)
		{
			char	   *name = strVal(lfirst(l));
			int			attnum;
			int			i;

			/* Lookup column name */
			attnum = InvalidAttrNumber;
			for (i = 0; i < tupDesc->natts; i++)
			{
				if (tupDesc->attrs[i]->attisdropped)
					continue;
				if (namestrcmp(&(tupDesc->attrs[i]->attname), name) == 0)
				{
					attnum = tupDesc->attrs[i]->attnum;
					break;
				}
			}
			if (attnum == InvalidAttrNumber)
			{
				if (rel != NULL)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
					errmsg("column \"%s\" of relation \"%s\" does not exist",
						   name, RelationGetRelationName(rel))));
				else
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
							 errmsg("column \"%s\" does not exist",
									name)));
			}
			/* Check for duplicates */
			if (list_member_int(attnums, attnum))
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_COLUMN),
						 errmsg("column \"%s\" specified more than once",
								name)));
			attnums = lappend_int(attnums, attnum);
		}
	}

	return attnums;
}


/*
 * The following function have been copied from /commands/copy.c file and
 * are used without any changes.
 *
 */
static void
ReceiveCopyBegin(NoDBScanState_t cstate)
{
	if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
	{
		/* new way */
		StringInfoData buf;
		int			natts = list_length(cstate->attnumlist);
		int16		format = (cstate->binary ? 1 : 0);
		int			i;

		pq_beginmessage(&buf, 'G');
		pq_sendbyte(&buf, format);		/* overall format */
		pq_sendint(&buf, natts, 2);
		for (i = 0; i < natts; i++)
			pq_sendint(&buf, format, 2);		/* per-column formats */
		pq_endmessage(&buf);
		cstate->copy_dest = COPY_NEW_FE;
		cstate->fe_msgbuf = makeStringInfo();
	}
	else if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 2)
	{
		/* old way */
		if (cstate->binary)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("COPY BINARY is not supported to stdout or from stdin")));
		pq_putemptymessage('G');
		cstate->copy_dest = COPY_OLD_FE;
	}
	else
	{
		/* very old way */
		if (cstate->binary)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("COPY BINARY is not supported to stdout or from stdin")));
		pq_putemptymessage('D');
		cstate->copy_dest = COPY_OLD_FE;
	}
	/* We *must* flush here to ensure FE knows it can send. */
	pq_flush();
}





/*
 * CopyGetData reads data from the source (file or frontend)
 *
 * We attempt to read at least minread, and at most maxread, bytes from
 * the source.	The actual number of bytes read is returned; if this is
 * less than minread, EOF was detected.
 *
 * Note: when copying from the frontend, we expect a proper EOF mark per
 * protocol; if the frontend simply drops the connection, we raise error.
 * It seems unwise to allow the COPY IN to complete normally in that case.
 *
 * NB: no data conversion is applied here.
 */
static int
CopyGetData(NoDBScanState_t cstate, void *databuf, int minread, int maxread)
{
	int			bytesread = 0;

	switch (cstate->copy_dest)
	{
		case COPY_FILE:
			bytesread = fread(databuf, 1, maxread, cstate->copy_file);
			if (ferror(cstate->copy_file))
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not read from COPY file: %m")));
			break;
		case COPY_OLD_FE:

			/*
			 * We cannot read more than minread bytes (which in practice is 1)
			 * because old protocol doesn't have any clear way of separating
			 * the COPY stream from following data.  This is slow, but not any
			 * slower than the code path was originally, and we don't care
			 * much anymore about the performance of old protocol.
			 */
			if (pq_getbytes((char *) databuf, minread))
			{
				/* Only a \. terminator is legal EOF in old protocol */
				ereport(ERROR,
						(errcode(ERRCODE_CONNECTION_FAILURE),
						 errmsg("unexpected EOF on client connection")));
			}
			bytesread = minread;
			break;
		case COPY_NEW_FE:
			while (maxread > 0 && bytesread < minread && !cstate->fe_eof)
			{
				int			avail;

				while (cstate->fe_msgbuf->cursor >= cstate->fe_msgbuf->len)
				{
					/* Try to receive another message */
					int			mtype;

			readmessage:
					mtype = pq_getbyte();
					if (mtype == EOF)
						ereport(ERROR,
								(errcode(ERRCODE_CONNECTION_FAILURE),
							 errmsg("unexpected EOF on client connection")));
					if (pq_getmessage(cstate->fe_msgbuf, 0))
						ereport(ERROR,
								(errcode(ERRCODE_CONNECTION_FAILURE),
							 errmsg("unexpected EOF on client connection")));
					switch (mtype)
					{
						case 'd':		/* CopyData */
							break;
						case 'c':		/* CopyDone */
							/* COPY IN correctly terminated by frontend */
							cstate->fe_eof = true;
							return bytesread;
						case 'f':		/* CopyFail */
							ereport(ERROR,
									(errcode(ERRCODE_QUERY_CANCELED),
									 errmsg("COPY from stdin failed: %s",
									   pq_getmsgstring(cstate->fe_msgbuf))));
							break;
						case 'H':		/* Flush */
						case 'S':		/* Sync */

							/*
							 * Ignore Flush/Sync for the convenience of client
							 * libraries (such as libpq) that may send those
							 * without noticing that the command they just
							 * sent was COPY.
							 */
							goto readmessage;
						default:
							ereport(ERROR,
									(errcode(ERRCODE_PROTOCOL_VIOLATION),
									 errmsg("unexpected message type 0x%02X during COPY from stdin",
											mtype)));
							break;
					}
				}
				avail = cstate->fe_msgbuf->len - cstate->fe_msgbuf->cursor;
				if (avail > maxread)
					avail = maxread;
				pq_copymsgbytes(cstate->fe_msgbuf, databuf, avail);
				databuf = (void *) ((char *) databuf + avail);
				maxread -= avail;
				bytesread += avail;
			}
			break;
	}

	return bytesread;
}


/*
 * Prepare struct with query execution status
 * *DP* Based on copyFrom(CopyState cstate) in src/backend/commands/copy.c
 */
static NoDBScanExecStatusStmt_t
GetScanExecStatusStmt(NoDBScanState_t cstate)
{
    NoDBScanExecStatusStmt_t status;
	bool pipe = (cstate->filename == NULL);
	int	attnum;

	cstate->estate = CreateExecutorState(); // for ExecConstraints()
	cstate->oldcontext = CurrentMemoryContext;
	status.hi_options = 0; // start with default heap_insert options


	/*
	*Check if input relation is actually a relation :
	* #define RELKIND_RELATION 'r'
	* #define RELKIND_SEQUENCE 'S'
	* #define RELKIND_VIEW	   'v'
	*/
	Assert(cstate->rel);
	if (cstate->rel->rd_rel->relkind != RELKIND_RELATION)
	{
		if (cstate->rel->rd_rel->relkind == RELKIND_VIEW)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy to view \"%s\"",
							RelationGetRelationName(cstate->rel))));
		else if (cstate->rel->rd_rel->relkind == RELKIND_SEQUENCE)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy to sequence \"%s\"",
							RelationGetRelationName(cstate->rel))));
		else
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy to non-table relation \"%s\"",
							RelationGetRelationName(cstate->rel))));
	}


	/*----------
	 * Check to see if we can avoid writing WAL
	 *
	 * If archive logging/streaming is not enabled *and* either
	 *	- table was created in same transaction as this COPY
	 *	- data is being written to relfilenode created in this transaction
	 * then we can skip writing WAL.  It's safe because if the transaction
	 * doesn't commit, we'll discard the table (or the new relfilenode file).
	 * If it does commit, we'll have done the heap_sync at the bottom of this
	 * routine first.
	 *
	 * As mentioned in comments in utils/rel.h, the in-same-transaction test
	 * is not completely reliable, since in rare cases rd_createSubid or
	 * rd_newRelfilenodeSubid can be cleared before the end of the transaction.
	 * However this is OK since at worst we will fail to make the optimization.
	 *
	 * Also, if the target file is new-in-transaction, we assume that checking
	 * FSM for free space is a waste of time, even if we must use WAL because
	 * of archiving.  This could possibly be wrong, but it's unlikely.
	 *
	 * The comments for heap_insert and RelationGetBufferForTuple specify that
	 * skipping WAL logging is only safe if we ensure that our tuples do not
	 * go into pages containing tuples from any other transactions --- but this
	 * must be the case if we have a new table or new relfilenode, so we need
	 * no additional work to enforce that.
	 *----------
	 */
	if (cstate->rel->rd_createSubid != InvalidSubTransactionId ||
		cstate->rel->rd_newRelfilenodeSubid != InvalidSubTransactionId)
	{
		status.hi_options |= HEAP_INSERT_SKIP_FSM;
		if (!XLogIsNeeded())
			status.hi_options |= HEAP_INSERT_SKIP_WAL;
	}

	if (pipe)
	{
		if (whereToSendOutput == DestRemote)
			ReceiveCopyBegin(cstate);
		else
			cstate->copy_file = stdin;
	}
	else
	{
		struct stat st;

		cstate->copy_file = AllocateFile(cstate->filename, PG_BINARY_R);

		if (cstate->copy_file == NULL)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not open file \"%s\" for reading: %m",
							cstate->filename)));

		fstat(fileno(cstate->copy_file), &st);
		if (S_ISDIR(st.st_mode))
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is a directory", cstate->filename)));
	}
	cstate->tupDesc = RelationGetDescr(cstate->rel);
	cstate->attr = cstate->tupDesc->attrs;
	status.num_phys_attrs = cstate->tupDesc->natts;
	status.attr_count = list_length(cstate->attnumlist);//?????
	status.num_defaults = 0;

	/*
	 * We need a ResultRelInfo so we can use the regular executor's
	 * index-entry-making machinery.  (There used to be a huge amount of code
	 * here that basically duplicated execUtils.c ...)
	 */
	status.resultRelInfo = makeNode(ResultRelInfo);
	status.resultRelInfo->ri_RangeTableIndex = 1;		// dummy
	status.resultRelInfo->ri_RelationDesc = cstate->rel;
	status.resultRelInfo->ri_TrigDesc = CopyTriggerDesc(cstate->rel->trigdesc);
	if (status.resultRelInfo->ri_TrigDesc)
	{
		status.resultRelInfo->ri_TrigFunctions = (FmgrInfo *)
			palloc0(status.resultRelInfo->ri_TrigDesc->numtriggers * sizeof(FmgrInfo));
		status.resultRelInfo->ri_TrigWhenExprs = (List **)
			palloc0(status.resultRelInfo->ri_TrigDesc->numtriggers * sizeof(List *));
	}
	status.resultRelInfo->ri_TrigInstrument = NULL;

	ExecOpenIndices(status.resultRelInfo);

	cstate->estate->es_result_relations = status.resultRelInfo;
	cstate->estate->es_num_result_relations = 1;
	cstate->estate->es_result_relation_info = status.resultRelInfo;

	/* Set up a tuple slot too */
	status.slot = ExecInitExtraTupleSlot(cstate->estate);
	ExecSetSlotDescriptor(status.slot, cstate->tupDesc);
	status.econtext = GetPerTupleExprContext(cstate->estate);

	/*
	 * Pick up the required catalog information for each attribute in the
	 * relation, including the input function, the element type (to pass to
	 * the input function), and info about defaults and constraints. (Which
	 * input function we use depends on text/binary format choice.)
	 */
	status.in_functions = (FmgrInfo *) palloc(status.num_phys_attrs * sizeof(FmgrInfo));
	cstate->fcinfo = (FunctionCallInfoData *) palloc(status.num_phys_attrs * sizeof(FunctionCallInfoData));

	cstate->attrlen = (int2 *) palloc(status.num_phys_attrs * sizeof(int2));
    for (attnum = 0; attnum < status.num_phys_attrs; attnum++) {
        cstate->attrlen[attnum] = cstate->attr[attnum]->attlen;
    }

	status.typioparams = (Oid *) palloc(status.num_phys_attrs * sizeof(Oid));
	status.defmap = (int *) palloc(status.num_phys_attrs * sizeof(int));
	status.defexprs = (ExprState **) palloc(status.num_phys_attrs * sizeof(ExprState *));

	for (attnum = 1; attnum <= status.num_phys_attrs; attnum++)
	{
		// We don't need info for dropped attributes //
		if (cstate->attr[attnum - 1]->attisdropped)
			continue;

		// Fetch the input function and typioparam info //
		if (cstate->binary)
			getTypeBinaryInputInfo(cstate->attr[attnum - 1]->atttypid,
								   &status.in_func_oid, &status.typioparams[attnum - 1]);
		else
			getTypeInputInfo(cstate->attr[attnum - 1]->atttypid,
							 &status.in_func_oid, &status.typioparams[attnum - 1]);
		fmgr_info(status.in_func_oid, &status.in_functions[attnum - 1]);


		//noDB: Add struct for each attribute
		InitFunctionCallInfoData(cstate->fcinfo[attnum - 1], &status.in_functions[attnum - 1], 3, NULL, NULL);

		//status.fcinfo[attnum - 1].arg[0] = CStringGetDatum(str); --> we need the str for it ;-)
		cstate->fcinfo[attnum - 1].arg[1] = ObjectIdGetDatum(status.typioparams[attnum - 1]);
		cstate->fcinfo[attnum - 1].arg[2] = Int32GetDatum(cstate->attr[attnum - 1]->atttypmod);
		cstate->fcinfo[attnum - 1].argnull[0] = false;
		cstate->fcinfo[attnum - 1].argnull[1] = false;
		cstate->fcinfo[attnum - 1].argnull[2] = false;


		// Get default info if needed //
		if (!list_member_int(cstate->attnumlist, attnum))
		{
			// attribute is NOT to be copied from input /
			// use default value if one exists //
			Node	   *defexpr = build_column_default(cstate->rel, attnum);

			if (defexpr != NULL)
			{
				status.defexprs[status.num_defaults] = ExecPrepareExpr((Expr *) defexpr,
						cstate->estate);
				status.defmap[status.num_defaults] = attnum - 1;
				status.num_defaults++;
			}
		}
	}
	/* Prepare to catch AFTER triggers. */
	AfterTriggerBeginQuery(); //SnoopDB: maybe not needed

	/*
	 * Check BEFORE STATEMENT insertion triggers. It's debateable whether we
	 * should do this for COPY, since it's not really an "INSERT" statement as
	 * such. However, executing these triggers maintains consistency with the
	 * EACH ROW triggers that we already fire on COPY.
	 */
	ExecBSInsertTriggers(cstate->estate, status.resultRelInfo);

	if (!cstate->binary)
		status.file_has_oids = false;
//		status.file_has_oids = cstate->oids;	// must rely on user to tell us...
	else
	{
		// Read and verify binary header
		char		readSig[11];
		int32		tmp;

		// Signature
		if (CopyGetData(cstate, readSig, 11, 11) != 11 ||
			memcmp(readSig, BinarySignature, 11) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("COPY file signature not recognized")));
		// Flags field
		if (!CopyGetInt32(cstate, &tmp))
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("invalid COPY file header (missing flags)")));
		status.file_has_oids = (tmp & (1 << 16)) != 0;
		tmp &= ~(1 << 16);
		if ((tmp >> 16) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("unrecognized critical flags in COPY file header")));
		// Header extension length
		if (!CopyGetInt32(cstate, &tmp) ||
			tmp < 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("invalid COPY file header (missing length)")));
		// Skip extension header, if present
		while (tmp-- > 0)
		{
			if (CopyGetData(cstate, readSig, 1, 1) != 1)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 errmsg("invalid COPY file header (wrong length)")));
		}
	}
	if (status.file_has_oids && cstate->binary)
	{
		getTypeBinaryInputInfo(OIDOID, &status.in_func_oid, &status.oid_typioparam);
		fmgr_info(status.in_func_oid, &status.oid_in_function);
	}
	/* Initialize state variables */
	cstate->fe_eof = false;
	cstate->eol_type = EOL_UNKNOWN;
//	cstate->cur_relname = RelationGetRelationName(cstate->rel);
	cstate->cur_lineno = 0; /* *DP* incremented in NoDBExecutor, so why not initialise it... */
//	cstate->cur_attname = NULL;
//	cstate->cur_attval = NULL;

	cstate->tupleStored = 0;
	cstate->tupleRead = 0 ;

//	bistate = GetBulkInsertState();

	/* Set up callback to identify error line number */
//	status.errcontext.callback = copy_in_error_callback;
	status.errcontext.arg = (void *) cstate;
	status.errcontext.previous = error_context_stack;
	error_context_stack = &status.errcontext;

	/* on input just throw the header line away
	 * *DP* For PostgresRAW : if file in actually opened only here and then read
	 * sequentially until EOF, this should be sufficient to ignore the header line */
	if (cstate->header_line)
	{
		cstate->cur_lineno++;
		NoDBCopyReadLineText(cstate);
	}

	/*Allocate memory*/
	cstate->values = (Datum *) palloc(status.num_phys_attrs * sizeof(Datum));
	cstate->nulls = (bool *) palloc(status.num_phys_attrs * sizeof(bool));

	//Added for testing
	MemSet(cstate->values, 0, status.num_phys_attrs * sizeof(Datum));
	MemSet(cstate->nulls, true, status.num_phys_attrs * sizeof(bool));

	/* create workspace for CopyReadAttributes results */
	cstate->nfields = status.file_has_oids ? (status.attr_count + 1) : status.attr_count;
	cstate->field_strings = (char **) palloc(cstate->nfields * sizeof(char *));

	return status;
}

static bool
CopyGetInt32(NoDBScanState_t cstate, int32 *val)
{
	uint32		buf;

	if (CopyGetData(cstate, &buf, sizeof(buf), sizeof(buf)) != sizeof(buf))
	{
		*val = 0;				/* suppress compiler warning */
		return false;
	}
	*val = (int32) ntohl(buf);
	return true;
}




/*
 * Auxilliary function for scan nodes, return the proper ScanState struct
 */
static ScanState *
getProperScanState(PlanState *planstate)
{
	ScanState *node;

	switch (nodeTag(planstate))
	{
			/*
			 * scan nodes
			 */
		case T_SeqScanState:
			node = (SeqScanState *) planstate;
			break;

		case T_IndexScanState:

			if (((IndexScanState *)planstate)->iss_NumRuntimeKeys != 0 && !((IndexScanState *)planstate)->iss_RuntimeKeysReady)
				node = NULL;//reScan will be called...
			else //&node->ss
				node = &((IndexScanState *) planstate)->ss;
			break;

		case T_BitmapHeapScanState:
			node = &((BitmapHeapScanState *) planstate)->ss;
			break;

		case T_TidScanState:
			node = &((TidScanState *) planstate)->ss;
			break;

		case T_SubqueryScanState:
			node = &((SubqueryScanState *) planstate)->ss;
			break;

		case T_FunctionScanState:
			node = &((FunctionScanState *) planstate)->ss;
			break;

		case T_ValuesScanState:
			node = &((ValuesScanState *) planstate)->ss;
			break;

		case T_CteScanState:
			node = &((CteScanState *) planstate)->ss;
			break;

		case T_WorkTableScanState://TODO:check if the statement below is valid...
			node = &((WorkTableScanState *) planstate)->ss;
			break;

		default:
//			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(planstate));
			node = NULL;
			break;
	}

	return node;
}


/*
 * Search through a PlanState tree for a scan nodes and update Tag to newTag.
 * Based on ExplainNode (commnad/explain.c)
 * Currently this aply only to SeqScan.
 * TODO: Change so as upadate other nodes as well (e.g. IndexSan if needed) + Scan to different table ==> different policy!!!
 */

List *
traversePlanTree(Plan *plan, PlanState *planstate, Plan *outer_plan, PlannedStmt *topPlan)
{
		List *planstateList = NIL;
		List *tmpList = NIL;
		ScanState *node;
		Assert(plan);

		//Check Plan Nodes (plannodes.h)
		switch (nodeTag(plan))
		{
			case T_IndexScan:
			case T_SeqScan:
			case T_BitmapHeapScan:
			case T_TidScan:
			case T_SubqueryScan:
			case T_FunctionScan:
			case T_ValuesScan:
			case T_CteScan:
			case T_WorkTableScan:
			{
				if (plan->qual)
				{
					changeParseStatus(PS_filterList);
					traverseQual(plan->qual, plan, NULL, topPlan);
					changeParseStatus(PS_idle);
				}

				node = getProperScanState(planstate);
				if ( node )
					planstateList = lcons(node, planstateList);
				break;
			}
			case T_BitmapIndexScan:
				break;
			case T_NestLoop:
			case T_MergeJoin:
			case T_HashJoin:
				break;
			case T_SetOp:
				break;
            case T_Limit:
            {
                LimitState *limitNode = (LimitState *) planstate;
                if (!plan->lefttree) {
                    if( nodeTag(plan->lefttree) == T_SeqScan ) {
                        recompute_limits(limitNode);
                        setQueryLimit(limitNode->count);
                    }
                }
                // Probably we don't need this
                if (!plan->righttree) {
                    if( nodeTag(plan->lefttree) == T_SeqScan ) {
                        recompute_limits(limitNode);
                        setQueryLimit(limitNode->count);
                    }
                }

                break;
            }
			default:
				break;
		}

		//check For Plan State Nodes (execnodes.h) --> No...
//		switch (nodeTag(planstate))
//		{
//		}

		/* initPlan-s */
		if (plan->initPlan) //PlanState here
		{
			tmpList = traverseSubPlans(planstate->initPlan, topPlan);
			planstateList = list_concat(planstateList, tmpList);
		}

		/* lefttree */
		if (outerPlan(plan)) {
			/*
			 * Ordinarily we don't pass down our own outer_plan value to our child
			 * nodes, but in bitmap scan trees we must, since the bottom
			 * BitmapIndexScan nodes may have outer references.
			 */
			tmpList = traversePlanTree(outerPlan(plan), outerPlanState(planstate), IsA(plan, BitmapHeapScan) ? outer_plan : NULL, topPlan);
			planstateList = list_concat(planstateList, tmpList);
		}

		/* righttree */
		if (innerPlan(plan)) {
			tmpList = traversePlanTree(innerPlan(plan), innerPlanState(planstate), outerPlan(plan), topPlan);
			planstateList = list_concat(planstateList, tmpList);
		}

		/* special child plans */
		switch (nodeTag(plan))
		{
			case T_ModifyTable:
				{
					tmpList = traverseMemberNodes(((ModifyTable *) plan)->plans,
												((ModifyTableState *) planstate)->mt_plans,
												outer_plan, topPlan);
					planstateList = list_concat(planstateList, tmpList);
					break;
				}
			case T_Append:
				{
					tmpList = traverseMemberNodes(((Append *) plan)->appendplans,
												((AppendState *) planstate)->appendplans,
												outer_plan, topPlan);
					planstateList = list_concat(planstateList, tmpList);
					break;
				}
			case T_BitmapAnd:
				{
					tmpList = traverseMemberNodes(((BitmapAnd *) plan)->bitmapplans,
												((BitmapAndState *) planstate)->bitmapplans,
												outer_plan, topPlan);
					planstateList = list_concat(planstateList, tmpList);
					break;
				}
			case T_BitmapOr:
				{
					tmpList = traverseMemberNodes(((BitmapOr *) plan)->bitmapplans,
												((BitmapOrState *) planstate)->bitmapplans,
												outer_plan, topPlan);
					planstateList = list_concat(planstateList, tmpList);
					break;
				}
			case T_SubqueryScan:
				{
					SubqueryScan *subqueryscan = (SubqueryScan *) plan;
					SubqueryScanState *subquerystate = (SubqueryScanState *) planstate;
					tmpList = traversePlanTree(subqueryscan->subplan, subquerystate->subplan, NULL, topPlan);
					planstateList = list_concat(planstateList, tmpList);
				}
				break;
			default:
				break;
		}

		/* subPlan-s */
		if (planstate->subPlan) //PlanState here
		{
			tmpList = traverseSubPlans(planstate->subPlan, topPlan);
			planstateList = list_concat(planstateList, tmpList);
		}

		return planstateList;
}


/*
 * Traverse a list of SubPlans (or initPlans, which also use SubPlan nodes).
 */
static List*
traverseSubPlans(List *plans, PlannedStmt *top)
{
	ListCell   *lst;
	List *planstateList = NIL;
	foreach(lst, plans)
	{
		SubPlanState *sps = (SubPlanState *) lfirst(lst);
		SubPlan    *sp = (SubPlan *) sps->xprstate.expr;
		List *tmpList = traversePlanTree(exec_subplan_get_plan(top, sp),sps->planstate, NULL, top);
		planstateList = list_concat(planstateList, tmpList);
	}
	return planstateList;
}


static List*
traverseMemberNodes(List *plans, PlanState **planstate, Plan *outer_plan, PlannedStmt *top)
{
	ListCell   *lst;
	List *planstateList = NIL;
	int			j = 0;
	foreach(lst, plans)
	{
		Plan	   *subnode = (Plan *) lfirst(lst);
		List *tmpList = traversePlanTree(subnode, planstate[j], outer_plan, top);
		planstateList = list_concat(planstateList, tmpList);
		j++;
	}
	return planstateList;
}


static void
traverseQual(List *qual, Plan *plan, Plan *outer_plan, PlannedStmt *topPlan)
{
	List	   *context;
	Node	   *node;
//	char	   *exprstr;

	ExplainState es;

	/* Initialize ExplainState. */
	ExplainInitState(&es);

	es.rtable = topPlan->rtable;
	es.pstmt = topPlan;

	/* No work if empty qual */
	if (qual == NIL)
		return;

	/* Convert AND list to explicit AND */
	node = (Node *) make_ands_explicit(qual);

	/* Set up deparsing context */
	context = deparse_context_for_plan((Node *) plan,
									   (Node *) outer_plan,
									   es.rtable,
									   es.pstmt->subplans);

	/* Deparse the expression */
//	exprstr = deparse_expression(node, context, false, false);
	deparse_expression(node, context, false, false);

}


// Copied from nodeLimit.c
static void
recompute_limits(LimitState *node)
{
    ExprContext *econtext = node->ps.ps_ExprContext;
    Datum       val;
    bool        isNull;

    if (node->limitOffset)
    {
        val = ExecEvalExprSwitchContext(node->limitOffset,
                                        econtext,
                                        &isNull,
                                        NULL);
        /* Interpret NULL offset as no offset */
        if (isNull)
            node->offset = 0;
        else
        {
            node->offset = DatumGetInt64(val);
            if (node->offset < 0)
                ereport(ERROR,
                 (errcode(ERRCODE_INVALID_ROW_COUNT_IN_RESULT_OFFSET_CLAUSE),
                  errmsg("OFFSET must not be negative")));
        }
    }
    else
    {
        /* No OFFSET supplied */
        node->offset = 0;
    }

    if (node->limitCount)
    {
        val = ExecEvalExprSwitchContext(node->limitCount,
                                        econtext,
                                        &isNull,
                                        NULL);
        /* Interpret NULL count as no count (LIMIT ALL) */
        if (isNull)
        {
            node->count = 0;
            node->noCount = true;
        }
        else
        {
            node->count = DatumGetInt64(val);
            if (node->count < 0)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_ROW_COUNT_IN_LIMIT_CLAUSE),
                         errmsg("LIMIT must not be negative")));
            node->noCount = false;
        }
    }
    else
    {
        /* No COUNT supplied */
        node->count = 0;
        node->noCount = true;
    }

    /* Reset position to start-of-scan */
    node->position = 0;
    node->subSlot = NULL;

    /* Set state-machine state */
    node->lstate = LIMIT_RESCAN;

    /*
     * If we have a COUNT, and our input is a Sort node, notify it that it can
     * use bounded sort.
     *
     * This is a bit of a kluge, but we don't have any more-abstract way of
     * communicating between the two nodes; and it doesn't seem worth trying
     * to invent one without some more examples of special communication
     * needs.
     *
     * Note: it is the responsibility of nodeSort.c to react properly to
     * changes of these parameters.  If we ever do redesign this, it'd be a
     * good idea to integrate this signaling with the parameter-change
     * mechanism.
     */
    if (IsA(outerPlanState(node), SortState))
    {
        SortState  *sortState = (SortState *) outerPlanState(node);
        int64       tuples_needed = node->count + node->offset;

        /* negative test checks for overflow */
        if (node->noCount || tuples_needed < 0)
        {
            /* make sure flag gets reset if needed upon rescan */
            sortState->bounded = false;
        }
        else
        {
            sortState->bounded = true;
            sortState->bound = tuples_needed;
        }
    }
}


