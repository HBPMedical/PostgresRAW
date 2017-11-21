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
#include "utils/numeric.h"
#include "utils/datum.h"

#include <time.h>


#include "snooping/common.h"
#include "snooping/queryDescriptor.h"

#include "noDB/NoDBScanStrategy.h"
#include "noDB/NoDBCache.h"
#include "noDB/NoDBExecutorWithTimer.h"
#include "noDB/auxiliary/NoDBMalloc.h"
#include "noDB/auxiliary/NoDBTimer.h"

#define ISOCTAL(c) (((c) >= '0') && ((c) <= '7'))
#define OCTVALUE(c) ((c) - '0')


/*
 * These macros centralize code used to process line_buf and raw_buf buffers.
 * They are macros because they often do continue/break control and to avoid
 * function call overhead in tight COPY loops.
 *
 * We must use "if (1)" because the usual "do {...} while(0)" wrapper would
 * prevent the continue/break processing from working.	We end the "if (1)"
 * with "else ((void) 0)" to ensure the "if" does not unintentionally match
 * any "else" in the calling code, and to avoid any compiler warnings about
 * empty statements.  See http://www.cit.gu.edu.au/~anthony/info/C/C.macros.
 */

/*
 * This keeps the character read at the top of the loop in the buffer
 * even if there is more than one read-ahead.
 */
#define IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(extralen) \
if (1) \
{ \
	if (raw_buf_ptr + (extralen) >= copy_buf_len && !hit_eof) \
	{ \
		raw_buf_ptr = prev_raw_ptr; /* undo fetch */ \
		need_data = true; \
		continue; \
	} \
} else ((void) 0)

/* This consumes the remainder of the buffer and breaks */
#define IF_NEED_REFILL_AND_EOF_BREAK(extralen) \
if (1) \
{ \
	if (raw_buf_ptr + (extralen) >= copy_buf_len && hit_eof) \
	{ \
		if (extralen) \
			raw_buf_ptr = copy_buf_len; /* consume the partial character */ \
		/* backslash just before EOF, treat as data char */ \
		result = true; \
		break; \
	} \
} else ((void) 0)

/*
 * Transfer any approved data to line_buf; must do this to be sure
 * there is some room in raw_buf.
 */
#define REFILL_LINEBUF \
if (1) \
{ \
	if (raw_buf_ptr > cstate->raw_buf_index) \
	{ \
		appendBinaryStringInfo(&cstate->line_buf, \
							 cstate->raw_buf + cstate->raw_buf_index, \
							   raw_buf_ptr - cstate->raw_buf_index); \
		cstate->raw_buf_index = raw_buf_ptr; \
	} \
} else ((void) 0)

/* Undo any read-ahead and jump out of the block. */
#define NO_END_OF_COPY_GOTO \
if (1) \
{ \
	raw_buf_ptr = prev_raw_ptr + 1; \
	goto not_end_of_copy; \
} else ((void) 0)


static const char BinarySignature[11] = "PGCOPY\n\377\r\n\0";


//static void             getDatumFromCache(NoDBColumnsPerCache_t readFromCache, Datum *values, NoDBRow_t row);
//static void             NoDBGetNextTupleFromPM(NoDBColumnsPerCache_t readFromPM, NoDBPMPair_t *attributes, NoDBRow_t row);
static void             NoDBGetNextTupleViaPM(NoDBScanState_t cstate, NoDBReadViaPM_t readViaPM);

static void             convertToServerEncoding(NoDBScanState_t cstate);
static TupleTableSlot   *generateTupleTableSlot( TupleTableSlot *ss_ScanTupleSlot, Datum *values, bool *isnull);


static bool             NoDBCopyLoadRawBuf(NoDBScanState_t cstate);
static int              NoDBCopyGetData(NoDBScanState_t cstate, void *databuf, int minread, int maxread);
static bool             NoDBCopyReadLineText(NoDBScanState_t cstate);
static void             NoDBCopyReadAttributesText(NoDBScanState_t cstate);
static int              GetDecimalFromHex(char hex);

static bool             NoDBCopyReadLineTextWithEOL(NoDBScanState_t cstate);

static int              parseLineForward(NoDBScanState_t cstate, NoDBPMPair_t* attributes, char *cur_ptr, int how_many, int attributeID);
static int              parseLineBackward(NoDBScanState_t cstate, NoDBPMPair_t *attributes, char *cur_ptr, int how_many, int attributeID);


static void             NoDBGetValues(NoDBScanState_t cstate, NoDBColVector_t cols);
static void             NoDBGetValuesWithPM(NoDBScanState_t cstate, NoDBColVector_t cols);

static long int         NoDBComputeBytesToRead(NoDBCache_t *eolCache, long currentTuple, long availableBytes, int *tuplesRead);



TupleTableSlot *NoDBExecPlanWT(NoDBScanState_t cstate, bool *pass)
{
	int i;
	int j;
	NoDBScanStrategy_t *plan;


	// Read next tuple from file (Use EOL if available)
	if (cstate->plan->readFile(cstate))
		return NULL;

	plan = cstate->plan->curStrategy;

    for ( i = 0 ; i < plan->nreadPostFilterWithCache; i++)
        NoDBCacheGetDatumVector(plan->readPostFilterWithCache[i].cache, cstate->values, cstate->nulls, plan->readPostFilterWithCache[i].cols, cstate->processed);

    // Extract positions With PM
    for ( i = 0 ; i < plan->nreadPostFilterWithPM; i++)
        NoDBCacheGetPMPairVector(plan->readPostFilterWithPM[i].cache, cstate->attributes, plan->readPostFilterWithPM[i].cols, cstate->processed);

    // Read other positions via PM
	for ( i = 0 ; i < plan->nreadPostFilterViaPM; i++)
		NoDBGetNextTupleViaPM(cstate, plan->readPostFilterViaPM[i]);

	// Read attributes With file
	if ( NoDBColVectorSize(plan->readPostFilterWithFile) > 0 )
	{
	    NoDBTimerSetBegin(cstate->timer, TOKENIZING);
        NoDBCopyReadAttributesText(cstate);
        NoDBTimerSetEnd(cstate->timer, TOKENIZING);
        NoDBGetValues(cstate, plan->convertPostFilter);
	}
	else
		NoDBGetValuesWithPM(cstate,plan->convertPostFilter);


	// Cache data
    for (j = 0; j < plan->nwriteToCacheByValue; j++) {
        NoDBCacheSetDatumVector(plan->writeToCacheByValue[j].cache, cstate->values, cstate->nulls, plan->writeToCacheByValue[j].cols, cstate->processed);
    }

    for (j = 0; j < plan->nwriteToCacheByRef; j++) {
        NoDBCacheSetDatumRefVector(plan->writeToCacheByRef[j].cache, cstate->values, cstate->nulls, cstate->attrlen, plan->writeToCacheByRef[j].cols, cstate->processed);
    }

    // Cache positions (cache position + the length of the attribute, both should be short int)
    for (j = 0; j < plan->nwriteToPM; j++) {
        NoDBCacheSetPMPairVector(plan->writeToPM[j].cache, cstate->attributes, plan->writeToPM[j].cols, cstate->processed);
    }

    // Update strategy
    cstate->processed++;
	cstate->plan->nMin--;
    if ( cstate->plan->nMin == 0 )
        NoDBUpdateStrategy(cstate);

	return generateTupleTableSlot( cstate->ss_ScanTupleSlot, cstate->values, cstate->nulls);
}


TupleTableSlot *NoDBExecPlanWithFiltersWT(NoDBScanState_t cstate, bool *pass)
{
	TupleTableSlot *slot = NULL;
	int i;
	int j;
	NoDBScanStrategy_t *plan;

	*pass = false;

	if (cstate->plan->readFile(cstate))
		return NULL;

	plan = cstate->plan->curStrategy;

	// Use the Cache
    for ( i = 0 ; i < plan->nreadPreFilterWithCache; i++)
        NoDBCacheGetDatumVector(plan->readPreFilterWithCache[i].cache, cstate->values, cstate->nulls, plan->readPreFilterWithCache[i].cols, cstate->processed);

    // Extract positions With PM
    for ( i = 0 ; i < plan->nreadPreFilterWithPM; i++)
        NoDBCacheGetPMPairVector(plan->readPreFilterWithPM[i].cache, cstate->attributes, plan->readPreFilterWithPM[i].cols, cstate->processed);


	// Read other positions via PM
	for ( i = 0 ; i < plan->nreadPreFilterViaPM; i++)
		NoDBGetNextTupleViaPM(cstate, plan->readPreFilterViaPM[i]);


	// Read attributes With file
	if ( NoDBColVectorSize(plan->readPreFilterWithFile) > 0 )
	{
	    NoDBTimerSetBegin(cstate->timer, TOKENIZING);
        NoDBCopyReadAttributesText(cstate);
        NoDBTimerSetEnd(cstate->timer, TOKENIZING);
        NoDBGetValues(cstate, plan->convertPreFilter);
	}
	else
		NoDBGetValuesWithPM(cstate,plan->convertPreFilter);

	slot = generateTupleTableSlot( cstate->ss_ScanTupleSlot, cstate->values, cstate->nulls);
	/*
     * place the current tuple into the expr context
     */
	cstate->econtext->ecxt_scantuple = slot;

	if (ExecQual(cstate->qual, cstate->econtext, false))
	{
		// Read the rest of the attributes
		*pass = true;

		//Use With Cache
		for ( i = 0 ; i < plan->nreadPostFilterWithCache; i++)
		    NoDBCacheGetDatumVector(plan->readPostFilterWithCache[i].cache, cstate->values, cstate->nulls, plan->readPostFilterWithCache[i].cols, cstate->processed);

		// Extract positions With PM
		for ( i = 0 ; i < plan->nreadPostFilterWithPM; i++)
	        NoDBCacheGetPMPairVector(plan->readPostFilterWithPM[i].cache, cstate->attributes, plan->readPostFilterWithPM[i].cols, cstate->processed);

		// Read other positions via PM
		for ( i = 0 ; i < plan->nreadPostFilterViaPM; i++)
			NoDBGetNextTupleViaPM(cstate, plan->readPostFilterViaPM[i]);

		// Read attributes With file
	    if ( NoDBColVectorSize(plan->readPostFilterWithFile) > 0 )
	    {
		    NoDBTimerSetBegin(cstate->timer, TOKENIZING);
	        NoDBCopyReadAttributesText(cstate);
	        NoDBTimerSetEnd(cstate->timer, TOKENIZING);
	        NoDBGetValues(cstate, plan->readPostFilterWithFile);
	    }
	    else
			NoDBGetValuesWithPM(cstate,plan->convertPostFilter);
	}

    // Cache data
    for (j = 0; j < plan->nwriteToCacheByValue; j++) {
        NoDBCacheSetDatumVector(plan->writeToCacheByValue[j].cache, cstate->values, cstate->nulls, plan->writeToCacheByValue[j].cols, cstate->processed);
    }

    for (j = 0; j < plan->nwriteToCacheByRef; j++) {
        NoDBCacheSetDatumRefVector(plan->writeToCacheByRef[j].cache, cstate->values, cstate->nulls, cstate->attrlen, plan->writeToCacheByRef[j].cols, cstate->processed);
    }

    // Cache positions (cache position + the length of the attribute, both should be short int)
    for (j = 0; j < plan->nwriteToPM; j++) {
        NoDBCacheSetPMPairVector(plan->writeToPM[j].cache, cstate->attributes, plan->writeToPM[j].cols, cstate->processed);
    }


    // Update strategy
    cstate->processed++;
	cstate->plan->nMin--;
	if ( cstate->plan->nMin == 0 )
		NoDBUpdateStrategy(cstate);
    return slot;
}


// Read-only queries from caches
TupleTableSlot *NoDBExecPlanCacheOnlyWT(NoDBScanState_t cstate, bool *pass)
{
	int i;
	NoDBScanStrategy_t *plan;

	plan = cstate->plan->curStrategy;

	// Use the Cache
    for ( i = 0 ; i < plan->nreadPostFilterWithCache; i++)
        NoDBCacheGetDatumVector(plan->readPostFilterWithCache[i].cache, cstate->values, cstate->nulls, plan->readPostFilterWithCache[i].cols, cstate->processed);


    // Update strategy
    cstate->processed++;
	cstate->plan->nMin--;
    if ( cstate->plan->nMin == 0 )
        NoDBUpdateStrategy(cstate);

	return generateTupleTableSlot( cstate->ss_ScanTupleSlot, cstate->values, cstate->nulls);

}

// Read-only queries from caches with
TupleTableSlot *NoDBExecPlanWithFiltersCacheOnlyWT(NoDBScanState_t cstate, bool *pass)
{
	TupleTableSlot *slot = NULL;
	int i;
	NoDBScanStrategy_t *plan;

	*pass = false;
	plan = cstate->plan->curStrategy;

	// Use the Cache
    for ( i = 0 ; i < plan->nreadPreFilterWithCache; i++)
        NoDBCacheGetDatumVector(plan->readPreFilterWithCache[i].cache, cstate->values, cstate->nulls, plan->readPreFilterWithCache[i].cols, cstate->processed);

	slot = generateTupleTableSlot( cstate->ss_ScanTupleSlot, cstate->values, cstate->nulls);
	cstate->econtext->ecxt_scantuple = slot;

	if (ExecQual(cstate->qual, cstate->econtext, false))
	{
		// Read the rest of the attributes
		*pass = true;

		//Use With Cache
	    for ( i = 0 ; i < plan->nreadPostFilterWithCache; i++)
	        NoDBCacheGetDatumVector(plan->readPostFilterWithCache[i].cache, cstate->values, cstate->nulls, plan->readPostFilterWithCache[i].cols, cstate->processed);
	}

    // Update strategy
    cstate->processed++;
	cstate->plan->nMin--;
	if ( cstate->plan->nMin == 0 )
		NoDBUpdateStrategy(cstate);
    return slot;
}


/*
//Read from Cache
static
void getDatumFromCache(NoDBColumnsPerCache_t readFromCache, Datum *values, NoDBRow_t row)
{
	int i;
	for ( i = 0 ; i < NoDBColVectorSize(readFromCache.cols); i++)
	{
		NoDBCol_t column = NoDBColVectorGet(readFromCache.cols, i);
		values[column] = NoDBCacheGetDatum(readFromCache.cache, column, row);
	}
}

//Read from PM
static void
NoDBGetNextTupleFromPM(NoDBColumnsPerCache_t readFromPM,  NoDBPMPair_t *attributes, NoDBRow_t row)
{
	int i;
	for ( i = 0 ; i < NoDBColVectorSize(readFromPM.cols); i++)
	{
		NoDBCol_t column = NoDBColVectorGet(readFromPM.cols, i);
		attributes[column] = NoDBCacheGetPMPair(readFromPM.cache, column, row);
	}
}
*/

static void
NoDBGetNextTupleViaPM(NoDBScanState_t cstate, NoDBReadViaPM_t readViaPM)
{
	int i;
	int start;
	int end;
//	int size;
//	int leave = 0;


	//Read the first and the last
	NoDBTimerSetBegin(cstate->timer, TOKENIZING);
	if (readViaPM.forward > 0)
	{
        int how_many = readViaPM.forward;
        int which = readViaPM.col;

        start = cstate->attributes[which].pointer + cstate->attributes[which].width;
        cstate->attributes[which + 1].pointer = start + 1;
        parseLineForward(cstate, cstate->attributes, cstate->line_buf.data + start + 1, how_many, which + 1);
        //Update width
        int j;
        for ( j = which + 1; j <= (which + how_many); j++)
            cstate->attributes[j].width = cstate->attributes[j + 1].pointer - cstate->attributes[j].pointer - 1;
	}

	if (readViaPM.backward < 0)
	{
		int how_many = readViaPM.backward;
		int which = readViaPM.col;
		end =  cstate->attributes[which].pointer - 1;
		parseLineBackward(cstate, cstate->attributes, cstate->line_buf.data + end - 1, abs(how_many), which - 1);
		//Update width
		int j;
		for ( j = which - 1; j >= (which + how_many); j--)
			cstate->attributes[j].width = cstate->attributes[j + 1].pointer - cstate->attributes[j].pointer - 1;
	}
    NoDBTimerSetEnd(cstate->timer, TOKENIZING);

//	if ( (size = NoDBColVectorSize(readViaPM.cols)) <= 0 ) {
//	    return;
//	}
//
//	//Read the first and the last
//	for ( i = 0 ; i < size; i+= (size - 1))
//	{
//		int j;
//		NoDBCol_t column = NoDBColVectorGet(readViaPM.cols, i);
//		int how_many = readViaPM.distances[i];
//		int which;
//		if ( how_many > 0)
//		{
//		    //If the distance of the first is positive then use the seciond distance to parse
//		    if (i == 0 && readViaPM.distances[size - 1 ] > 0) {
//		        column = NoDBColVectorGet(readViaPM.cols, size - 1);
//		        how_many = readViaPM.distances[size - 1];
//		        leave = 1;
//		    }
//			which = column - how_many;
//			start = cstate->attributes[which].pointer + cstate->attributes[which].width;
//			cstate->attributes[which + 1].pointer = start + 1;
//			parseLineForward(cstate, cstate->attributes, cstate->line_buf.data + start + 1, how_many, which + 1);
//
//			//Update width
//			for ( j = which + 1; j <= column; j++)
//				cstate->attributes[j].width = cstate->attributes[j + 1].pointer - cstate->attributes[j].pointer - 1;
//		}
//		else
//		{
//            if (size > 1 && i == 0) {
//                column = NoDBColVectorGet(readViaPM.cols, size - 1);
//                how_many = readViaPM.distances[size - 1];
//                leave = 1;
//            }
//			which = column - how_many;
//			end =  cstate->attributes[which].pointer - 1;
//			parseLineBackward(cstate, cstate->attributes, cstate->line_buf.data + end - 1, abs(how_many), which - 1);
//			//Update width
//			for ( j = which - 1; j >= column; j--)
//				cstate->attributes[j].width = cstate->attributes[j + 1].pointer - cstate->attributes[j].pointer - 1;
//
//		}
//		if( size == 1 || leave)
//		    break;
//	}
}




/*
 * NoDBGetNextTupleFromFile()
 */

bool
NoDBGetNextTupleFromFileWT(NoDBScanState_t cstate)
{
	bool done = false;


	NoDBTimerSetBegin(cstate->timer, PARSING);
//	TIMESPEC_SET_CURRENT(cstate->timer.begin[PARSING]);
	//Read next tuple from file
	resetStringInfo(&cstate->line_buf);

	/* Parse data and transfer into cstate->line_buf */
	done = NoDBCopyReadLineText(cstate);

	if (!done)
	{
		/*
		 * If we didn't hit EOF, then we must have transferred the EOL marker
		 * to line_buf along with the data.  Get rid of it.
		 */
		switch (cstate->eol_type)
		{
			case EOL_NL:
				Assert(cstate->line_buf.len >= 1);
				Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n');

				cstate->line_buf.len--;
				cstate->line_buf.data[cstate->line_buf.len] = '\0';
				NoDBCacheSetShortInt(cstate->plan->eolCache, 0, cstate->processed, cstate->line_buf.len + 1);
				break;

			case EOL_CR:
				Assert(cstate->line_buf.len >= 1);
				Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\r');

				cstate->line_buf.len--;
				cstate->line_buf.data[cstate->line_buf.len] = '\0';
				NoDBCacheSetShortInt(cstate->plan->eolCache, 0, cstate->processed, cstate->line_buf.len + 1);
				break;

			case EOL_CRNL:
				Assert(cstate->line_buf.len >= 2);
				Assert(cstate->line_buf.data[cstate->line_buf.len - 2] == '\r');
				Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n');

				cstate->line_buf.len -= 2;
				cstate->line_buf.data[cstate->line_buf.len] = '\0';
				NoDBCacheSetShortInt(cstate->plan->eolCache, 0, cstate->processed, cstate->line_buf.len + 2);
				break;

			case EOL_UNKNOWN:
				/* shouldn't get here */
				Assert(false);
				break;
		}
		NoDBCacheSetUsedRows(cstate->plan->eolCache, cstate->processed + 1);
		cstate->tupleRead++;
	    convertToServerEncoding(cstate);
	}

	NoDBTimerSetEnd(cstate->timer, PARSING);


	if (done && cstate->line_buf.len == 0)
		return true;


	return done;
}

bool
NoDBGetNextTupleFromFileWithEOLWT(NoDBScanState_t cstate)
{
	bool done = false;

	NoDBTimerSetBegin(cstate->timer, PARSING);
	//Read next tuple from file
	resetStringInfo(&cstate->line_buf);

	/* Parse data and transfer into cstate->line_buf */
	done = NoDBCopyReadLineTextWithEOL(cstate);

	if (!done)
	{
		Assert(cstate->line_buf.len >= 1);
		if(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n')
		{
			cstate->line_buf.len--;
			cstate->line_buf.data[cstate->line_buf.len] = '\0';
		}
		else if(cstate->line_buf.data[cstate->line_buf.len - 1] == '\r')
		{
			cstate->line_buf.len--;
			cstate->line_buf.data[cstate->line_buf.len] = '\0';
		}
		else if(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n' && cstate->line_buf.data[cstate->line_buf.len - 2] == '\r')
		{
			cstate->line_buf.len -= 2;
			cstate->line_buf.data[cstate->line_buf.len] = '\0';
		}
		else
			Assert(false);
		cstate->tupleRead++;
	    convertToServerEncoding(cstate);
	}
	NoDBTimerSetEnd(cstate->timer, PARSING);


	if (done && cstate->line_buf.len == 0)
		return true;


	return done;
}




static
void convertToServerEncoding(NoDBScanState_t cstate)
{
	/* Mark that encoding conversion hasn't occurred yet */
	cstate->line_buf_converted = false;

	/* Done reading the line.  Convert it to server encoding. */
	if (cstate->need_transcoding  && ! NoDBGetNeedTranscoding(cstate->execInfo))
	{
		char	   *cvt;

		cvt = pg_client_to_server(cstate->line_buf.data,
								  cstate->line_buf.len);
		if (cvt != cstate->line_buf.data)
		{
			/* transfer converted data back to line_buf */
			resetStringInfo(&cstate->line_buf);
			appendBinaryStringInfo(&cstate->line_buf, cvt, strlen(cvt));
			pfree(cvt);
		}
	}

	/* Now it's safe to use the buffer in error messages */
	cstate->line_buf_converted = true;
}


static TupleTableSlot *
generateTupleTableSlot( TupleTableSlot *ss_ScanTupleSlot, Datum *values, bool *isnull)
{
	TupleTableSlot *slot;

	slot = ss_ScanTupleSlot;
	slot->tts_isempty = false;
	slot->tts_shouldFree = false;
	slot->tts_shouldFreeMin = false;
//	slot->tts_tuple = tuple;
	slot->tts_mintuple = NULL;

	slot->tts_tupleDescriptor = ss_ScanTupleSlot->tts_tupleDescriptor;

	slot->tts_nvalid = ss_ScanTupleSlot->tts_tupleDescriptor->natts;
	slot->tts_values = values;
	slot->tts_isnull = isnull;

	return slot;
}


bool
NoDBTryReFillRawBufWT(NoDBScanState_t cstate)
{
	bool		hit_eof = false;
	bool		result = false;
	int raw_buf_ptr = cstate->raw_buf_index;

	if (raw_buf_ptr >= cstate->raw_buf_len)
	{
		REFILL_LINEBUF;
		/*
		 * Try to read some more data.	This will certainly reset
		 * raw_buf_index to zero, and raw_buf_ptr must go with it.
		 */
		if (!NoDBCopyLoadRawBuf(cstate))
			hit_eof = true;
		if (cstate->raw_buf_len <= 0 || hit_eof)
			return true;
	}
	return result;
}


/*
 * CopyLoadRawBuf loads some more data into raw_buf
 *
 * Returns TRUE if able to obtain at least one more byte, else FALSE.
 *
 * If raw_buf_index < raw_buf_len, the unprocessed bytes are transferred
 * down to the start of the buffer and then we load more data after that.
 * This case is used only when a frontend multibyte character crosses a
 * bufferload boundary.
 */
static bool
NoDBCopyLoadRawBuf(NoDBScanState_t cstate)
{
	int			nbytes;
	int			inbytes;

    NoDBTimerSetBegin(cstate->timer, IO);
	if (cstate->raw_buf_index < cstate->raw_buf_len)
	{
		/* Copy down the unprocessed data */
		nbytes = cstate->raw_buf_len - cstate->raw_buf_index;
		memmove(cstate->raw_buf, cstate->raw_buf + cstate->raw_buf_index,
				nbytes);
	}
	else
		nbytes = 0;	/* no data need be saved */

	inbytes = NoDBCopyGetData(cstate, cstate->raw_buf + nbytes,
						  1, RAW_BUF_SIZE - nbytes);
	nbytes += inbytes;
	cstate->raw_buf[nbytes] = '\0';
	cstate->raw_buf_index = 0;
	cstate->raw_buf_len = nbytes;

    NoDBTimerSetEnd(cstate->timer, IO);

	return (inbytes > 0);
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
NoDBCopyGetData(NoDBScanState_t cstate, void *databuf, int minread, int maxread)
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
//		case COPY_OLD_FE:
//			break;
//		case COPY_NEW_FE:
//			break;
		default:
			ereport(ERROR,
					(errmsg("Unknown copy_dest\n")));

	}

	return bytesread;
}

/*
 * CopyReadLineText - inner loop of CopyReadLine for text mode
 */
//TODO: In the previous version I had removed the csv_mode field
static bool
NoDBCopyReadLineText(NoDBScanState_t cstate)
{
	char	   *copy_raw_buf;
	int			raw_buf_ptr;
	int			copy_buf_len;
	bool		need_data = false;
	bool		hit_eof = false;
	bool		result = false;
	char		mblen_str[2];

	/* CSV variables */
	bool		first_char_in_line = true;
	bool		in_quote = false,
				last_was_esc = false;
	char		quotec = '\0';
	char		escapec = '\0';

	if (cstate->csv_mode)
	{
		quotec = cstate->quote[0];
		escapec = cstate->escape[0];
		/* ignore special escape processing if it's the same as quotec */
		if (quotec == escapec)
			escapec = '\0';
	}

	mblen_str[1] = '\0';

	/*
	 * The objective of this loop is to transfer the entire next input line
	 * into line_buf.  Hence, we only care for detecting newlines (\r and/or
	 * \n) and the end-of-copy marker (\.).
	 *
	 * In CSV mode, \r and \n inside a quoted field are just part of the data
	 * value and are put in line_buf.  We keep just enough state to know if we
	 * are currently in a quoted field or not.
	 *
	 * These four characters, and the CSV escape and quote characters, are
	 * assumed the same in frontend and backend encodings.
	 *
	 * For speed, we try to move data from raw_buf to line_buf in chunks
	 * rather than one character at a time.  raw_buf_ptr points to the next
	 * character to examine; any characters from raw_buf_index to raw_buf_ptr
	 * have been determined to be part of the line, but not yet transferred to
	 * line_buf.
	 *
	 * For a little extra speed within the loop, we copy raw_buf and
	 * raw_buf_len into local variables.
	 */
	copy_raw_buf = cstate->raw_buf;
	raw_buf_ptr = cstate->raw_buf_index;
	copy_buf_len = cstate->raw_buf_len;

	for (;;)
	{
		int			prev_raw_ptr;
		char		c;

		/*
		 * Load more data if needed.  Ideally we would just force four bytes
		 * of read-ahead and avoid the many calls to
		 * IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(), but the COPY_OLD_FE protocol
		 * does not allow us to read too far ahead or we might read into the
		 * next data, so we read-ahead only as far we know we can.	One
		 * optimization would be to read-ahead four byte here if
		 * cstate->copy_dest != COPY_OLD_FE, but it hardly seems worth it,
		 * considering the size of the buffer.
		 */
		if (raw_buf_ptr >= copy_buf_len || need_data)
		{
			REFILL_LINEBUF;

			/*
			 * Try to read some more data.	This will certainly reset
			 * raw_buf_index to zero, and raw_buf_ptr must go with it.
			 */
			if (!NoDBCopyLoadRawBuf(cstate))
				hit_eof = true;
			raw_buf_ptr = 0;
			copy_buf_len = cstate->raw_buf_len;

			/*
			 * If we are completely out of data, break out of the loop,
			 * reporting EOF.
			 */
			if (copy_buf_len <= 0)
			{
				result = true;
				break;
			}
			need_data = false;
		}

		/* OK to fetch a character */
		prev_raw_ptr = raw_buf_ptr;
		c = copy_raw_buf[raw_buf_ptr++];

		if (cstate->csv_mode)
		{
			/*
			 * If character is '\\' or '\r', we may need to look ahead below.
			 * Force fetch of the next character if we don't already have it.
			 * We need to do this before changing CSV state, in case one of
			 * these characters is also the quote or escape character.
			 *
			 * Note: old-protocol does not like forced prefetch, but it's OK
			 * here since we cannot validly be at EOF.
			 */
			if (c == '\\' || c == '\r')
			{
				IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
			}

			/*
			 * Dealing with quotes and escapes here is mildly tricky. If the
			 * quote char is also the escape char, there's no problem - we
			 * just use the char as a toggle. If they are different, we need
			 * to ensure that we only take account of an escape inside a
			 * quoted field and immediately preceding a quote char, and not
			 * the second in a escape-escape sequence.
			 */
			if (in_quote && c == escapec)
				last_was_esc = !last_was_esc;
			if (c == quotec && !last_was_esc)
				in_quote = !in_quote;
			if (c != escapec)
				last_was_esc = false;

			/*
			 * Updating the line count for embedded CR and/or LF chars is
			 * necessarily a little fragile - this test is probably about the
			 * best we can do.	(XXX it's arguable whether we should do this
			 * at all --- is cur_lineno a physical or logical count?)
			 */
			if (in_quote && c == (cstate->eol_type == EOL_NL ? '\n' : '\r'))
				cstate->cur_lineno++;
		}

		/* Process \r */
		if (c == '\r' && (!cstate->csv_mode || !in_quote))
		{
			/* Check for \r\n on first line, _and_ handle \r\n. */
			if (cstate->eol_type == EOL_UNKNOWN ||
				cstate->eol_type == EOL_CRNL)
			{
				/*
				 * If need more data, go back to loop top to load it.
				 *
				 * Note that if we are at EOF, c will wind up as '\0' because
				 * of the guaranteed pad of raw_buf.
				 */
				IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);

				/* get next char */
				c = copy_raw_buf[raw_buf_ptr];

				if (c == '\n')
				{
					raw_buf_ptr++;		/* eat newline */
					cstate->eol_type = EOL_CRNL;		/* in case not set yet */
				}
				else
				{
					/* found \r, but no \n */
					if (cstate->eol_type == EOL_CRNL)
						ereport(ERROR,
								(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
								 !cstate->csv_mode ?
							errmsg("literal carriage return found in data") :
							errmsg("unquoted carriage return found in data"),
								 !cstate->csv_mode ?
						errhint("Use \"\\r\" to represent carriage return.") :
								 errhint("Use quoted CSV field to represent carriage return.")));

					/*
					 * if we got here, it is the first line and we didn't find
					 * \n, so don't consume the peeked character
					 */
					cstate->eol_type = EOL_CR;
				}
			}
			else if (cstate->eol_type == EOL_NL)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 !cstate->csv_mode ?
						 errmsg("literal carriage return found in data") :
						 errmsg("unquoted carriage return found in data"),
						 !cstate->csv_mode ?
					   errhint("Use \"\\r\" to represent carriage return.") :
						 errhint("Use quoted CSV field to represent carriage return.")));
			/* If reach here, we have found the line terminator */
			break;
		}

		/* Process \n */
		if (c == '\n' && (!cstate->csv_mode || !in_quote))
		{
			if (cstate->eol_type == EOL_CR || cstate->eol_type == EOL_CRNL)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 !cstate->csv_mode ?
						 errmsg("literal newline found in data") :
						 errmsg("unquoted newline found in data"),
						 !cstate->csv_mode ?
						 errhint("Use \"\\n\" to represent newline.") :
					 errhint("Use quoted CSV field to represent newline.")));
			cstate->eol_type = EOL_NL;	/* in case not set yet */
			/* If reach here, we have found the line terminator */
			break;
		}

		/*
		 * In CSV mode, we only recognize \. alone on a line.  This is because
		 * \. is a valid CSV data value.
		 */
		if (c == '\\' && (!cstate->csv_mode || first_char_in_line))
		{
			char		c2;

			IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
			IF_NEED_REFILL_AND_EOF_BREAK(0);

			/* -----
			 * get next character
			 * Note: we do not change c so if it isn't \., we can fall
			 * through and continue processing for client encoding.
			 * -----
			 */
			c2 = copy_raw_buf[raw_buf_ptr];

			if (c2 == '.')
			{
				raw_buf_ptr++;	/* consume the '.' */

				/*
				 * Note: if we loop back for more data here, it does not
				 * matter that the CSV state change checks are re-executed; we
				 * will come back here with no important state changed.
				 */
				if (cstate->eol_type == EOL_CRNL)
				{
					/* Get the next character */
					IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
					/* if hit_eof, c2 will become '\0' */
					c2 = copy_raw_buf[raw_buf_ptr++];

					if (c2 == '\n')
					{
						if (!cstate->csv_mode)
							ereport(ERROR,
									(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
									 errmsg("end-of-copy marker does not match previous newline style")));
						else
							NO_END_OF_COPY_GOTO;
					}
					else if (c2 != '\r')
					{
						if (!cstate->csv_mode)
							ereport(ERROR,
									(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
									 errmsg("end-of-copy marker corrupt")));
						else
							NO_END_OF_COPY_GOTO;
					}
				}

				/* Get the next character */
				IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
				/* if hit_eof, c2 will become '\0' */
				c2 = copy_raw_buf[raw_buf_ptr++];

				if (c2 != '\r' && c2 != '\n')
				{
					if (!cstate->csv_mode)
						ereport(ERROR,
								(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
								 errmsg("end-of-copy marker corrupt")));
					else
						NO_END_OF_COPY_GOTO;
				}

				if ((cstate->eol_type == EOL_NL && c2 != '\n') ||
					(cstate->eol_type == EOL_CRNL && c2 != '\n') ||
					(cstate->eol_type == EOL_CR && c2 != '\r'))
				{
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("end-of-copy marker does not match previous newline style")));
				}

				/*
				 * Transfer only the data before the \. into line_buf, then
				 * discard the data and the \. sequence.
				 */
				if (prev_raw_ptr > cstate->raw_buf_index)
					appendBinaryStringInfo(&cstate->line_buf,
									 cstate->raw_buf + cstate->raw_buf_index,
									   prev_raw_ptr - cstate->raw_buf_index);
				cstate->raw_buf_index = raw_buf_ptr;
				result = true;	/* report EOF */
				break;
			}
			else if (!cstate->csv_mode)

				/*
				 * If we are here, it means we found a backslash followed by
				 * something other than a period.  In non-CSV mode, anything
				 * after a backslash is special, so we skip over that second
				 * character too.  If we didn't do that \\. would be
				 * considered an eof-of copy, while in non-CVS mode it is a
				 * literal backslash followed by a period.	In CSV mode,
				 * backslashes are not special, so we want to process the
				 * character after the backslash just like a normal character,
				 * so we don't increment in those cases.
				 */
				raw_buf_ptr++;
		}

		/*
		 * This label is for CSV cases where \. appears at the start of a
		 * line, but there is more text after it, meaning it was a data value.
		 * We are more strict for \. in CSV mode because \. could be a data
		 * value, while in non-CSV mode, \. cannot be a data value.
		 */
not_end_of_copy:

		/*
		 * Process all bytes of a multi-byte character as a group.
		 *
		 * We only support multi-byte sequences where the first byte has the
		 * high-bit set, so as an optimization we can avoid this block
		 * entirely if it is not set.
		 */
		if (cstate->encoding_embeds_ascii && IS_HIGHBIT_SET(c))
		{
			int			mblen;

			mblen_str[0] = c;
			/* All our encodings only read the first byte to get the length */
			mblen = pg_encoding_mblen(cstate->client_encoding, mblen_str);
			IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(mblen - 1);
			IF_NEED_REFILL_AND_EOF_BREAK(mblen - 1);
			raw_buf_ptr += mblen - 1;
		}
		first_char_in_line = false;
	}							/* end of outer loop */

	/*
	 * Transfer any still-uncopied data to line_buf.
	 */
	REFILL_LINEBUF;

	return result;
}


/*
 * Parse the current line into separate attributes (fields),
 * performing de-escaping as needed.
 *
 * The input is in line_buf.  We use attribute_buf to hold the result
 * strings.  fieldvals[k] is set to point to the k'th attribute string,
 * or NULL when the input matches the null marker string.  (Note that the
 * caller cannot check for nulls since the returned string would be the
 * post-de-escaping equivalent, which may look the same as some valid data
 * string.)
 *
 * delim is the column delimiter string (must be just one byte for now).
 * null_print is the null marker string.  Note that this is compared to
 * the pre-de-escaped input string.
 *
 * The return value is the number of fields actually read.	(We error out
 * if this would exceed maxfields, which is the length of fieldvals[].)
 */
//Modified to collect pointers for NoDB
//Parse only useful attributes (up to lastfield)
static void
NoDBCopyReadAttributesText(NoDBScanState_t cstate)
{
	char		delimc;
	int		fieldno;
	char	   *output_ptr;
	char	   *cur_ptr;
	char	   *line_end_ptr;


	//Added for SnoopDB
	int total_length;
	int maxfields;
	int lastfield;
	char **fieldvals;
//	int *attributes = cstate->interesting_attributes;
//	int interesting_attributes = cstate->numOfInterestingAtt + cstate->numOfQualAtt;
//	if (lastfield == -1) {
//		return;
//	}

	delimc = cstate->delim[0];
	total_length = 0;
	maxfields = cstate->nfields;
	lastfield = cstate->lastfield;
	fieldvals = cstate->field_strings;

	/*
	 * We need a special case for zero-column tables: check that the input
	 * line is empty, and return.
	 */
	if (maxfields <= 0)
	{
		if (cstate->line_buf.len != 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("extra data after last expected column")));
		return;
	}

	/* We don't need any attributes so don't parse anything (for count(*) queries)*/
//	if (interesting_attributes == 0) {
//		return;
//	}

	resetStringInfo(&cstate->attribute_buf);

	/*
	 * The de-escaped attributes will certainly not be longer than the input
	 * data line, so we can just force attribute_buf to be large enough and
	 * then transfer data without any checks for enough space.	We need to do
	 * it this way because enlarging attribute_buf mid-stream would invalidate
	 * pointers already stored into fieldvals[].
	 */
	if (cstate->attribute_buf.maxlen <= cstate->line_buf.len)
		enlargeStringInfo(&cstate->attribute_buf, cstate->line_buf.len);
	output_ptr = cstate->attribute_buf.data;

	/* set pointer variables for loop */
	cur_ptr = cstate->line_buf.data;
	line_end_ptr = cstate->line_buf.data + cstate->line_buf.len;

	/* Outer loop iterates over fields */
	fieldno = 0;
	for (;;)
	{
		bool		found_delim = false;
		char	   *start_ptr;
		char	   *end_ptr;
		int			input_len;
		bool		saw_non_ascii = false;

		/* Make sure space remains in fieldvals[] */
		if (fieldno >= maxfields)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("extra data after last expected column")));

		/* Remember start of field on both input and output sides */
		start_ptr = cur_ptr;
		fieldvals[fieldno] = output_ptr;

		/* Scan data for field */
		for (;;)
		{
			char		c;

			end_ptr = cur_ptr;
			if (cur_ptr >= line_end_ptr)
				break;
			c = *cur_ptr++;
			if (c == delimc)
			{
				found_delim = true;
				break;
			}
			if (c == '\\')
			{
				if (cur_ptr >= line_end_ptr)
					break;
				c = *cur_ptr++;
				switch (c)
				{
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
						{
							/* handle \013 */
							int			val;

							val = OCTVALUE(c);
							if (cur_ptr < line_end_ptr)
							{
								c = *cur_ptr;
								if (ISOCTAL(c))
								{
									cur_ptr++;
									val = (val << 3) + OCTVALUE(c);
									if (cur_ptr < line_end_ptr)
									{
										c = *cur_ptr;
										if (ISOCTAL(c))
										{
											cur_ptr++;
											val = (val << 3) + OCTVALUE(c);
										}
									}
								}
							}
							c = val & 0377;
							if (c == '\0' || IS_HIGHBIT_SET(c))
								saw_non_ascii = true;
						}
						break;
					case 'x':
						/* Handle \x3F */
						if (cur_ptr < line_end_ptr)
						{
							char		hexchar = *cur_ptr;

							if (isxdigit((unsigned char) hexchar))
							{
								int			val = GetDecimalFromHex(hexchar);

								cur_ptr++;
								if (cur_ptr < line_end_ptr)
								{
									hexchar = *cur_ptr;
									if (isxdigit((unsigned char) hexchar))
									{
										cur_ptr++;
										val = (val << 4) + GetDecimalFromHex(hexchar);
									}
								}
								c = val & 0xff;
								if (c == '\0' || IS_HIGHBIT_SET(c))
									saw_non_ascii = true;
							}
						}
						break;
					case 'b':
						c = '\b';
						break;
					case 'f':
						c = '\f';
						break;
					case 'n':
						c = '\n';
						break;
					case 'r':
						c = '\r';
						break;
					case 't':
						c = '\t';
						break;
					case 'v':
						c = '\v';
						break;

						/*
						 * in all other cases, take the char after '\'
						 * literally
						 */
				}
			}

			/* Add c to output string */
			*output_ptr++ = c;
		}

		/* Terminate attribute value in output area */
		*output_ptr++ = '\0';

		/*
		 * If we de-escaped a non-7-bit-ASCII char, make sure we still have
		 * valid data for the db encoding. Avoid calling strlen here for the
		 * sake of efficiency.
		 */
		if (saw_non_ascii)
		{
			char	   *fld = fieldvals[fieldno];
			pg_verifymbstr(fld, output_ptr - (fld + 1), false);
		}

		/* Check whether raw input matched null marker */
		input_len = end_ptr - start_ptr;
		if (input_len == cstate->null_print_len &&
			strncmp(start_ptr, cstate->null_print, input_len) == 0)
			fieldvals[fieldno] = NULL;

		//if attribute is selected
		//total_length
		total_length += input_len;
//		cstate->tempPositions[fieldno + 1] = total_length + 1;
		cstate->attributes[fieldno + 1].pointer = total_length + 1;
		cstate->attributes[fieldno].width = cstate->attributes[fieldno + 1].pointer - cstate->attributes[fieldno].pointer - 1;
//        cstate->attributes[fieldno + 1].width = total_length + 1;

//        cstate->tempPositions[fieldno + 1] = total_length + 1;

		if ( fieldno == lastfield)
			break;
		fieldno++;
//		if( attributes[fieldno - 1])
//		{
//			interesting_attributes--;
//			if (interesting_attributes == 0) //All interesting attributes have been collected
//				break;
//		}

		total_length++;

		/* Done if we hit EOL instead of a delim */
		if (!found_delim)
			break;
	}
	/* Clean up state of attribute_buf */
	output_ptr--;
	Assert(*output_ptr == '\0');
}

/*
 *	Return decimal value for a hexadecimal digit
 */
static int
GetDecimalFromHex(char hex)
{
	if (isdigit((unsigned char) hex))
		return hex - '0';
	else
		return tolower((unsigned char) hex) - 'a' + 10;
}


/*
 * CopyReadLineTextUsingMetaPointers - Just read from the file and pass the values to the Copystate line_buf
 */
static bool
NoDBCopyReadLineTextWithEOL(NoDBScanState_t cstate)
{
	/* */
	bool  	result = false;
	int		offset;

	/*Go to file and read tuples!*/
	if (cstate->tupleStored == 0)
	{
		int stored = 0;
		int inbytes, toRead = 0;
		Assert(cstate->raw_buf_index >= cstate->raw_buf_len);
		Assert(cstate->copy_dest == COPY_FILE);

		toRead = NoDBComputeBytesToRead(cstate->plan->eolCache, cstate->processed,RAW_BUF_SIZE, &stored);

		if(toRead == 0) {
//			Assert(cstate->tupleRead == NoDBCacheGetEnd(cstate->plan.eolCache));
//			Assert(cstate->processed == NoDBCacheGetEnd(cstate->plan.eolCache));
			return true;
		}
	    NoDBTimerSetBegin(cstate->timer, IO);

		inbytes = fread(cstate->raw_buf, 1, toRead, cstate->copy_file);
		if (ferror(cstate->copy_file))
			ereport(ERROR,(errcode_for_file_access(),
				errmsg("could not read from COPY file: %m")));

	    NoDBTimerSetEnd(cstate->timer, IO);

		Assert(toRead == inbytes);

		cstate->tupleStored = stored;

		cstate->raw_buf[inbytes] = '\0';
		cstate->raw_buf_index = 0;
		cstate->raw_buf_len = inbytes;
	}

	offset = NoDBCacheGetShortInt(cstate->plan->eolCache, 0, cstate->processed);

	cstate->tupleStored--;

	appendBinaryStringInfo(&cstate->line_buf, cstate->raw_buf + cstate->raw_buf_index, offset);
	cstate->raw_buf_index += offset;

	return result;
}


/*
 * Parse forward to retrieve pointers
 */
static int
parseLineForward(NoDBScanState_t cstate, NoDBPMPair_t* attributes, char *cur_ptr, int how_many, int attributeID)
{
	char		delimc = cstate->delim[0];

	char	   *line_end_ptr = cstate->line_buf.data + cstate->line_buf.len;
	int			input_len = 0;

	char	   *start_ptr;
	char	   *end_ptr;
	bool		saw_non_ascii;

	/* Remember start of field on both input and output sides */
	start_ptr = cur_ptr;
	/* Scan data for field */
	for (;;)
	{
		char		c;
		end_ptr = cur_ptr;
		if (cur_ptr >= line_end_ptr)
			break;
		c = *cur_ptr++;
		if (c == delimc)
		{
			attributeID++;
			attributes[attributeID].pointer = end_ptr - cstate->line_buf.data + 1;
			how_many--;

			if( how_many == 0 )
				break;

			start_ptr = cur_ptr;
		}
		if (c == '\\')
		{
			if (cur_ptr >= line_end_ptr)
				break;
			c = *cur_ptr++;
			switch (c)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					{
						/* handle \013 */
						int			val;

						val = OCTVALUE(c);
						if (cur_ptr < line_end_ptr)
						{
							c = *cur_ptr;
							if (ISOCTAL(c))
							{
								cur_ptr++;
								val = (val << 3) + OCTVALUE(c);
								if (cur_ptr < line_end_ptr)
								{
									c = *cur_ptr;
									if (ISOCTAL(c))
									{
										cur_ptr++;
										val = (val << 3) + OCTVALUE(c);
									}
								}
							}
						}
						c = val & 0377;
						if (c == '\0' || IS_HIGHBIT_SET(c))
							saw_non_ascii = true;
					}
					break;
				case 'x':
					/* Handle \x3F */
					if (cur_ptr < line_end_ptr)
					{
						char		hexchar = *cur_ptr;

						if (isxdigit((unsigned char) hexchar))
						{
							int			val = GetDecimalFromHex(hexchar);

							cur_ptr++;
							if (cur_ptr < line_end_ptr)
							{
								hexchar = *cur_ptr;
								if (isxdigit((unsigned char) hexchar))
								{
									cur_ptr++;
									val = (val << 4) + GetDecimalFromHex(hexchar);
								}
							}
							c = val & 0xff;
							if (c == '\0' || IS_HIGHBIT_SET(c))
								saw_non_ascii = true;
						}
					}
					break;
				case 'b':
					c = '\b';
					break;
				case 'f':
					c = '\f';
					break;
				case 'n':
					c = '\n';
					break;
				case 'r':
					c = '\r';
					break;
				case 't':
					c = '\t';
					break;
				case 'v':
					c = '\v';
					break;

					/*
					 * in all other cases, take the char after '\'
					 * literally
					 */
			}
		}
		/* Add c to output string */
//		if( how_many == 0 && c != delimc)
//			*output_ptr++ = c;
	}

//	/* Terminate attribute value in output area */
//	*output_ptr++ = '\0';

	/*
	 * If we de-escaped a non-7-bit-ASCII char, make sure we still have
	 * valid data for the db encoding. Avoid calling strlen here for the
	 * sake of efficiency.
	 */
	//TODO: add outside the code
	//TODO: check for NULL size
//	if (saw_non_ascii)
//	{
//			char	   *fld = fieldvals[fieldno];
//			pg_verifymbstr(fld, output_ptr - (fld + 1), false);
//	}

	//We parsed the last attribute and we hit EOL
	if( how_many > 0 )
	{
		attributeID++;
		attributes[attributeID].pointer = cstate->line_buf.len + 1;
	}

	/* Check whether raw input matched null marker */
	//TODO: put this check outside the code
	input_len = end_ptr - start_ptr;
	if (input_len == cstate->null_print_len &&
		strncmp(start_ptr, cstate->null_print, input_len) == 0)
		return 0;


	return input_len;
}

/*
 * Parse backward to retrieve pointers
 */
static int
parseLineBackward(NoDBScanState_t cstate, NoDBPMPair_t *attributes, char *cur_ptr, int how_many, int attributeID)
{
	char		delimc = cstate->delim[0];

	char	   *line_end_ptr = cstate->line_buf.data;
	int			input_len = 0;

	char	   *start_ptr;
	char	   *end_ptr;
	bool		saw_non_ascii = false;

	/* Remember start of field on both input and output sides */
	start_ptr = cur_ptr;
	/* Scan data for field */
	for (;;)
	{
		char		c;
		end_ptr = cur_ptr;
		if (cur_ptr < line_end_ptr)
			break;
		c = *cur_ptr--;

		if (c == delimc)
		{
			attributes[attributeID].pointer = end_ptr - cstate->line_buf.data + 1;

			how_many--;
			if( how_many == 0 ) {
				break;
			}

			attributeID--;
		}

		if (c == '\\')
		{
			if (cur_ptr < line_end_ptr)
				break;
			c = *cur_ptr--;
			switch (c)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					{
						/* handle \013 */
						int			val;

						val = OCTVALUE(c);
						if (cur_ptr >= line_end_ptr)
						{
							c = *cur_ptr;
							if (ISOCTAL(c))
							{
								cur_ptr--;
								val = (val << 3) + OCTVALUE(c);
								if (cur_ptr < line_end_ptr)
								{
									c = *cur_ptr;
									if (ISOCTAL(c))
									{
										cur_ptr--;
										val = (val << 3) + OCTVALUE(c);
									}
								}
							}
						}
						c = val & 0377;
						if (c == '\0' || IS_HIGHBIT_SET(c))
							saw_non_ascii = true;
					}
					break;
				case 'x':
					/* Handle \x3F */
					if (cur_ptr >= line_end_ptr)
					{
						char		hexchar = *cur_ptr;

						if (isxdigit((unsigned char) hexchar))
						{
							int			val = GetDecimalFromHex(hexchar);

							cur_ptr--;
							if (cur_ptr < line_end_ptr)
							{
								hexchar = *cur_ptr;
								if (isxdigit((unsigned char) hexchar))
								{
									cur_ptr--;
									val = (val << 4) + GetDecimalFromHex(hexchar);
								}
							}
							c = val & 0xff;
							if (c == '\0' || IS_HIGHBIT_SET(c))
								saw_non_ascii = true;
						}
					}
					break;
				case 'b':
					c = '\b';
					break;
				case 'f':
					c = '\f';
					break;
				case 'n':
					c = '\n';
					break;
				case 'r':
					c = '\r';
					break;
				case 't':
					c = '\t';
					break;
				case 'v':
					c = '\v';
					break;

					/*
					 * in all other cases, take the char after '\'
					 * literally
					 */
			}
		}
		/* Add c to output string */
//		if( how_many == 0 && c != delimc)
//			*output_ptr-- = c;
//		else
//			output_ptr--;
	}

//	/* Terminate attribute value in output area */
//	*output_ptr++ = '\0';

	/*
	 * If we de-escaped a non-7-bit-ASCII char, make sure we still have
	 * valid data for the db encoding. Avoid calling strlen here for the
	 * sake of efficiency.
	 */
//	if (saw_non_ascii)
//	{
//			char	   *fld = fieldvals[fieldno];
//			pg_verifymbstr(fld, output_ptr - (fld + 1), false);
//	}

	//We didn't find delimeter ==> First attribute
	if( how_many > 0)
	{
		attributes[attributeID].pointer = 0;
	}


	/* Check whether raw input matched null marker */
	input_len = start_ptr - end_ptr;
	if (input_len == cstate->null_print_len &&
		strncmp(end_ptr, cstate->null_print, input_len) == 0)
		return 0;


	return input_len;
}

static void
NoDBGetValues(NoDBScanState_t cstate, NoDBColVector_t cols)
{
    char *string;
    int i,j;

    NoDBTimerSetBegin(cstate->timer, CONVERSION);
    for ( j = 0; j < NoDBColVectorSize(cols); j++)
    {
        i = NoDBColVectorGet(cols,j);
        string = cstate->field_strings[i];
        if( string == NULL)
        {
            cstate->values[i] = (Datum)0;
            cstate->nulls[i] = true;
        }
        else
        {
            cstate->fcinfo[i].arg[0] = CStringGetDatum(string);
            cstate->fcinfo[i].argnull[0] = false;
            cstate->values[i] = FunctionCallInvoke(&cstate->fcinfo[i]);
            cstate->nulls[i] = false;
        }
    }
    NoDBTimerSetEnd(cstate->timer, CONVERSION);
}

static void
NoDBGetValuesWithPM(NoDBScanState_t cstate, NoDBColVector_t cols)
{
    int i,j;
    int bytesToRead;
    char *string;
    NoDBPMPair_t pair;

    NoDBTimerSetBegin(cstate->timer, CONVERSION);
    for ( j = 0; j < NoDBColVectorSize(cols); j++)
    {
        i = NoDBColVectorGet(cols,j);
        pair = cstate->attributes[i];
        bytesToRead = pair.width;
        Assert(bytesToRead > 0);
        string = cstate->line_buf.data + pair.pointer;
        string[bytesToRead] = '\0';
        if( strcmp(string,"\\N") == 0)
        {
            cstate->values[i] = (Datum)0;
            cstate->nulls[i] = true;
        }
        else
        {
            cstate->fcinfo[i].arg[0] = CStringGetDatum(string);
            cstate->fcinfo[i].argnull[0] = false;
            cstate->values[i] = FunctionCallInvoke(&cstate->fcinfo[i]);
            cstate->nulls[i] = false;
        }
    }
    NoDBTimerSetEnd(cstate->timer, CONVERSION);
}


/* Compute bytes to be read in the next block (used after the EOL pointers have been collected) */
//TODO use EOL cache for that
static long int
NoDBComputeBytesToRead(NoDBCache_t *eolCache, long currentTuple, long availableBytes, int *tuplesRead)
{
	long int result = 0 ;
	int i;
	*tuplesRead = 0;

	for( i = currentTuple; i < NoDBCacheGetUsedRows(eolCache); i++)
	{
			result += NoDBCacheGetShortInt(eolCache, 0, i);;
			(*tuplesRead)++;
			if(result > availableBytes) {
				result -= NoDBCacheGetShortInt(eolCache, 0, i);;
				(*tuplesRead)--;
				break;
			}
	}
	return result;
}








