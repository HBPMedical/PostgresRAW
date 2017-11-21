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
 
/*-------------------------------------------------------------------------
 *
 * load.c
 *		Implements the Load operator based on COPY utility command
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/snooping/load.c$
 *
 *-------------------------------------------------------------------------
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

#include "snooping/load.h"
//#include "snooping/inputFunctions.h"
//#include "snooping/coordinator.h"


//#define ISOCTAL(c) (((c) >= '0') && ((c) <= '7'))
//#define OCTVALUE(c) ((c) - '0')
//
//
//
///*
// * These macros centralize code used to process line_buf and raw_buf buffers.
// * They are macros because they often do continue/break control and to avoid
// * function call overhead in tight COPY loops.
// *
// * We must use "if (1)" because the usual "do {...} while(0)" wrapper would
// * prevent the continue/break processing from working.	We end the "if (1)"
// * with "else ((void) 0)" to ensure the "if" does not unintentionally match
// * any "else" in the calling code, and to avoid any compiler warnings about
// * empty statements.  See http://www.cit.gu.edu.au/~anthony/info/C/C.macros.
// */
//
///*
// * This keeps the character read at the top of the loop in the buffer
// * even if there is more than one read-ahead.
// */
//#define IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(extralen) \
//if (1) \
//{ \
//	if (raw_buf_ptr + (extralen) >= copy_buf_len && !hit_eof) \
//	{ \
//		raw_buf_ptr = prev_raw_ptr; /* undo fetch */ \
//		need_data = true; \
//		continue; \
//	} \
//} else ((void) 0)
//
///* This consumes the remainder of the buffer and breaks */
//#define IF_NEED_REFILL_AND_EOF_BREAK(extralen) \
//if (1) \
//{ \
//	if (raw_buf_ptr + (extralen) >= copy_buf_len && hit_eof) \
//	{ \
//		if (extralen) \
//			raw_buf_ptr = copy_buf_len; /* consume the partial character */ \
//		/* backslash just before EOF, treat as data char */ \
//		result = true; \
//		break; \
//	} \
//} else ((void) 0)
//
///*
// * Transfer any approved data to line_buf; must do this to be sure
// * there is some room in raw_buf.
// */
//#define REFILL_LINEBUF \
//if (1) \
//{ \
//	if (raw_buf_ptr > cstate->raw_buf_index) \
//	{ \
//		appendBinaryStringInfo(&cstate->line_buf, \
//							 cstate->raw_buf + cstate->raw_buf_index, \
//							   raw_buf_ptr - cstate->raw_buf_index); \
//		cstate->raw_buf_index = raw_buf_ptr; \
//	} \
//} else ((void) 0)
//
///* Undo any read-ahead and jump out of the block. */
//#define NO_END_OF_COPY_GOTO \
//if (1) \
//{ \
//	raw_buf_ptr = prev_raw_ptr + 1; \
//	goto not_end_of_copy; \
//} else ((void) 0)
//
//
//
///* Low-level communications functions */
////static bool CopyReadLine(CopyState cstate);
//static bool CopyReadLine(CopyState cstate, Controller IntegrityCheck);
////static int CopyReadAttributesCSV(CopyState cstate, int maxfields, char **fieldvals);
////static int CopyReadAttributesText(CopyState cstate, int maxfields, char **fieldvals);
//static void CopyReadAttributesText(CopyState cstate);
//
//
//static int GetDecimalFromHex(char hex);
//static bool CopyLoadRawBuf(CopyState cstate);
//static bool CopyReadLineText(CopyState cstate);
//
//
///*Function defined for SnoopDB*/
//static void CopyReadAttributesTextPushDown(CopyState);
//static bool CopyReadLineTextUsingMetaPointers (CopyState cstate);
//static bool CopyReadLineUsingMetaPointers(CopyState cstate, Controller IntegrityCheck);
//
///* Used when qual = NULL */
//static void CopyReadAttributesTextUsingInternalMetapointers_V1(CopyState cstate);
///* Used when qual != NULL */
//static bool CopyReadAttributesTextUsingInternalMetapointers_V2(CopyState cstate, List *qual, ExprContext *econtext);
//
//static int parseLineForward(CopyState cstate, int* attributes, char *cur_ptr, int how_many, int attributeID);
//static int parseLineBackward(CopyState cstate, int* attributes, char *cur_ptr, int how_many, int attributeID);
//
//static void getSelectiveValues(CopyState cstate);
//static void getQualSelectiveValues(CopyState cstate);
//
//
//
//
///*
// * Only if (!qual && !projInfo)
// */
//TupleTableSlot*
//getNextTupleV2(TupleTableSlot *ss_ScanTupleSlot, bool *done, int pos)
//{
//	TupleTableSlot *slot = NULL;
//
//	CopyState cstate;
//	CopyStmtExecStatus* status;
//	Controller IntegrityCheck;
//
//	cstate = CopyExec[pos].cstate;
//	status = &CopyExec[pos].status;
//	IntegrityCheck = CopyExec[pos].IntegrityCheck;
//
//	while (!*done)
//	{
//		bool		skip_tuple;
//
//		CHECK_FOR_INTERRUPTS();
//
//		// Reset the per-tuple exprcontext
//		ResetPerTupleExprContext(status->estate);
//		// Switch into its memory context
//		MemoryContextSwitchTo(GetPerTupleMemoryContext(status->estate));
//
//		// Actually read the line into memory here
//		if( enable_tuple_metapointers ) //Metapointers in the end of each tuple accelerate future steps
//		{
//			if( FD[cstate->filePointers_ID].done )
//				*done = CopyReadLineUsingMetaPointers(cstate, IntegrityCheck);
//			else
//				*done = CopyReadLine(cstate, IntegrityCheck);
//		}
//		else //First time just collect the end-of-tuple pointers
//			*done = CopyReadLine(cstate, IntegrityCheck);
//
//
//		/*
//		 * EOF at start of line means we're done.  If we see EOF after
//		 * some characters, we act as though it was newline followed by
//		 * EOF, ie, process the line and then exit loop on next iteration.
//		 */
//		if (*done && cstate->line_buf.len == 0)
//			break;
//
//		/* Copy cached datum into cstate->values */
//		if( enable_caching ) {
//			getCacheDatum(cstate->processed, cstate->cache_ID, cstate->values);
//		}
//
//		/* Internal metapointers */
//		if( enable_internal_metapointers )
//		{	//There are available pointers to be used
//			if( isPositionalMapReady(cstate->internalFilePointers_ID) )	{
//				CopyReadAttributesTextUsingInternalMetapointers_V1(cstate);
//			}
//			else //Internal metapointers haven't been collected
//			{
//				CopyReadAttributesText(cstate);
//				addInternalMapMetaPointers(cstate->temp_positions, cstate->internalFilePointers_ID, cstate->processed);
//				getSelectiveValues(cstate);
//			}
//		}
////		else if( enable_pushdown_select ) //Internal metapointers haven't been enabled but pushdown_selection is enabled!
////			CopyReadAttributesTextPushDown(cstate);
//		else
//		{
//			CopyReadAttributesText(cstate);
//			getSelectiveValues(cstate);
//		}
//
//		/* Store datum into cache if needed */
//		if( enable_caching ) {
//			addCacheDatum(cstate->values, status->attr, cstate->cache_ID, cstate->processed);
//		}
//
//		/* Generate a TupleTableSlot */
//		slot = generateTupleTableSlot(ss_ScanTupleSlot, cstate->values, cstate->nulls);
//
//		/* Triggers and stuff need to be invoked in query context. */
//		MemoryContextSwitchTo(status->oldcontext);
//
//		skip_tuple = false;
//		if (!skip_tuple)
//		{
//			cstate->processed++;
//			return slot;
//		}
//	}
//	/* File was scanned: Re-initialize the file pointer in case we re-scan the relation ;-) */
//	//TODO: Change to access only if there is an update in the structures
//	reInitRelation(cstate, *status);
//	return slot;
//}
//
//
///*
// * Get next tuple when qual != NULL
// * Examine whether the tuple qualifies...
// */
//TupleTableSlot*
//getNextTuple_Qual(bool *done, int pos, List *qual, ExprContext *econtext, TupleTableSlot *ss_ScanTupleSlot, bool *pass)
//{
//	TupleTableSlot *ret_slot = NULL;
//	TupleTableSlot slot;
//
//	CopyState cstate;
//	CopyStmtExecStatus* status;
//	Controller IntegrityCheck;
//
//	cstate = CopyExec[pos].cstate;
//	status = &CopyExec[pos].status;
//	IntegrityCheck = CopyExec[pos].IntegrityCheck;
//	*pass = false;
//
//	while (!*done)
//	{
//		bool		skip_tuple;
//
//		CHECK_FOR_INTERRUPTS();
//
//		//Reset the per-tuple exprcontext
//		ResetPerTupleExprContext(status->estate);
//		//Switch into its memory context
//		MemoryContextSwitchTo(GetPerTupleMemoryContext(status->estate));
//
//		/*Prepare ExprContext for qual part*/
//		slot.tts_tupleDescriptor = ss_ScanTupleSlot->tts_tupleDescriptor;
//		econtext->ecxt_scantuple = &slot;
//
//		//Actually read the line into memory here
//		if( enable_tuple_metapointers ) //Metapointers in the end of each tuple accelerate future steps
//		{
//			if( FD[cstate->filePointers_ID].done )
//				*done = CopyReadLineUsingMetaPointers(cstate, IntegrityCheck);
//			else
//				*done = CopyReadLine(cstate, IntegrityCheck);
//		}
//		else //First time just collect the end-of-tuple pointers
//			*done = CopyReadLine(cstate, IntegrityCheck);
//
//
//		/*
//		 * EOF at start of line means we're done.  If we see EOF after
//		 * some characters, we act as though it was newline followed by
//		 * EOF, ie, process the line and then exit loop on next iteration.
//		 */
//		if (*done && cstate->line_buf.len == 0)
//			break;
//
//		if( enable_internal_metapointers )
//		{
//			//Internal metapointers have been collected
//			if( isPositionalMapReady(cstate->internalFilePointers_ID) )  {
//				*pass = CopyReadAttributesTextUsingInternalMetapointers_V2(cstate, qual, econtext);
//			}
//			else
//			{
//				CopyReadAttributesText(cstate);
//				addInternalMapMetaPointers(cstate->temp_positions, cstate->internalFilePointers_ID, cstate->processed);
//				getQualSelectiveValues(cstate);
//				/*Copy Cached datum into cstate->values for where clause*/
//				if( enable_caching )
//					get_whereCacheDatum(cstate->tupleReadMetapointers, cstate->cache_ID, cstate->values);
//
//				econtext->ecxt_scantuple->tts_values = cstate->values;
//				econtext->ecxt_scantuple->tts_isnull = cstate->nulls;
//				econtext->ecxt_scantuple->tts_nvalid = cstate->nfields;
//				if (!qual || ExecQual(qual, econtext, false))
//				{
//					*pass = true;
//					getSelectiveValues(cstate);
//					/*Copy Cached datum into cstate->values for the rest of the attributes*/
//					if( enable_caching )
//						getCacheDatum(cstate->tupleReadMetapointers, cstate->cache_ID, cstate->values);
//				}
//			}
//		}
////		else if( enable_pushdown_select ) //Internal metapointers haven't been enabled but pushdown_selection is enabled!
////		{
//////			fprintf(stderr,"\nNOT TESTED EXECUTION PATH\n");
////			CopyReadAttributesTextPushDown(cstate);
////		}
//		else //To check if we disable all features that we still run for all the attributes
//		{
//			CopyReadAttributesText(cstate);
//			getQualSelectiveValues(cstate);
//			/*Copy Cached datum into cstate->values for where clause*/
//			if( enable_caching )
//				get_whereCacheDatum(cstate->processed, cstate->cache_ID, cstate->values);
//
//			econtext->ecxt_scantuple->tts_values = cstate->values;
//			econtext->ecxt_scantuple->tts_isnull = cstate->nulls;
//			econtext->ecxt_scantuple->tts_nvalid = cstate->nfields;
//			if (!qual || ExecQual(qual, econtext, false))
//			{
//				*pass = true;
//				getSelectiveValues(cstate);
//				/*Copy Cached datum into cstate->values for the rest of the attributes*/
//				if( enable_caching )
//					getCacheDatum(cstate->processed, cstate->cache_ID, cstate->values);
//			}
//
//		}
//
//		/* Store datum into cache if needed */
//		if( enable_caching ) {
//			addCacheDatum(cstate->values, status->attr, cstate->cache_ID, cstate->processed);
//		}
//
//
//		/*****************************************************/
//		//If the where clause is true then generate the tuple//
//		/*****************************************************/
//		if(*pass) {
//			ret_slot = generateTupleTableSlot(ss_ScanTupleSlot, cstate->values, cstate->nulls);
//		}
//
//		MemoryContextSwitchTo(status->oldcontext);
//		skip_tuple = false;
//		if (!skip_tuple)
//		{
//			cstate->processed++;
//			return ret_slot;
//		}
//	}
//	/*File was scanned: Re-initialize the file pointer in case we re-scan the relation ;-)*/
//	reInitRelation(cstate, *status);
//	return ret_slot;
//}
//
///*
// * Use only the cache for the processing
// */
//TupleTableSlot*
//getNextTupleCacheOnly(TupleTableSlot *ss_ScanTupleSlot, bool *done, int pos)
//{
//	TupleTableSlot *slot = NULL;
//
//	CopyState cstate;
//	CopyStmtExecStatus* status;
//
//	cstate = CopyExec[pos].cstate;
//	status = &CopyExec[pos].status;
//
//	/*If we have processed all the cached columns then stop*/
//	if (cstate->processed < getNumberOfCachedTuples(pos))
//	{
//		CHECK_FOR_INTERRUPTS();
//		// Reset the per-tuple exprcontext
//		ResetPerTupleExprContext(status->estate);
//		// Switch into its memory context
//		MemoryContextSwitchTo(GetPerTupleMemoryContext(status->estate));
//
//		/*Copy Cached datum into cstate->values*/
//		getCacheDatum(cstate->processed, cstate->cache_ID, cstate->values);
//		tupleCacheProcessed(cstate->cache_ID);
//
//		/* Generate a TupleTableSlot */
//		slot = generateTupleTableSlot(ss_ScanTupleSlot, cstate->values, cstate->nulls);
//
//		/* Triggers and stuff need to be invoked in query context. */
//		MemoryContextSwitchTo(status->oldcontext);
//
//		*done = false;
//		cstate->processed++;
//		return slot;
//	}
//	*done = true;
//
//	/*File was scanned: Re-initialize the file pointer in case we re-scan the relation ;-)*/
//	reInitRelation(cstate, *status);
//	return slot;
//}
//
///*
// * Use only the cache for the processing
// */
//TupleTableSlot*
//getNextTuple_QualCacheOnly(bool *done, int pos, List *qual, ExprContext *econtext, TupleTableSlot *ss_ScanTupleSlot, bool *pass)
//{
//	TupleTableSlot *ret_slot = NULL;
//	TupleTableSlot slot;
//
//	CopyState cstate;
//	CopyStmtExecStatus* status;
//
//	cstate = CopyExec[pos].cstate;
//	status = &CopyExec[pos].status;
//	*pass = false;
//
//	if (cstate->processed < getNumberOfCachedTuples(pos))
//	{
//		CHECK_FOR_INTERRUPTS();
//		//Reset the per-tuple exprcontext
//		ResetPerTupleExprContext(status->estate);
//		//Switch into its memory context
//		MemoryContextSwitchTo(GetPerTupleMemoryContext(status->estate));
//
//		/*Prepare ExprContext for qual part*/
//		slot.tts_tupleDescriptor = ss_ScanTupleSlot->tts_tupleDescriptor;
//		econtext->ecxt_scantuple = &slot;
//
//		get_whereCacheDatum(cstate->processed, cstate->cache_ID, cstate->values);
//
//		econtext->ecxt_scantuple->tts_values = cstate->values;
//		econtext->ecxt_scantuple->tts_isnull = cstate->nulls;
//		econtext->ecxt_scantuple->tts_nvalid = cstate->nfields;
//
//		if (!qual || ExecQual(qual, econtext, false))
//		{
//			*pass = true;
//			getCacheDatum(cstate->processed, cstate->cache_ID, cstate->values);
//		}
//
//		if(*pass)
//			ret_slot = generateTupleTableSlot(ss_ScanTupleSlot, cstate->values, cstate->nulls);
//
//		tupleCacheProcessed(cstate->cache_ID);
//		MemoryContextSwitchTo(status->oldcontext);
//
//		*done = false;
//		cstate->processed++;
//		return ret_slot;
//	}
//	*done = true;
//
//	/*File was scanned: Re-initialize the file pointer in case we re-scan the relation ;-)*/
//	reInitRelation(cstate, *status);
//	return ret_slot;
//}
//
//
///*
// * It runs for count(*) queries...
// */
//TupleTableSlot*
//getNextTupleWithoutInterestingAtts(TupleTableSlot *ss_ScanTupleSlot, bool *done, int pos)
//{
//	TupleTableSlot *slot = NULL;
//
//	CopyState cstate;
//	CopyStmtExecStatus* status;
//
////	int pos = findExecutionInfo(relation);
////	Assert(pos	!= -1);
//
//	cstate = CopyExec[pos].cstate;
//	status = &CopyExec[pos].status;
//
//	if (cstate->processed < getNumberOfTuples(pos))
//	{
//		CHECK_FOR_INTERRUPTS();
//		// Reset the per-tuple exprcontext
//		ResetPerTupleExprContext(status->estate);
//		// Switch into its memory context
//		MemoryContextSwitchTo(GetPerTupleMemoryContext(status->estate));
//
//		/* Generate a TupleTableSlot */
//		slot = generateTupleTableSlot(ss_ScanTupleSlot, cstate->values, cstate->nulls);
//
//		/* Triggers and stuff need to be invoked in query context. */
//		MemoryContextSwitchTo(status->oldcontext);
//
//		*done = false;
//		cstate->processed++;
//		return slot;
//	}
//	*done = true;
//	/*File was scanned: Re-initialize the file pointer in case we re-scan the relation ;-)*/
//	reInitRelation(cstate, *status);
//	return slot;
//}
//
//
///*
// * CopyLoadRawBuf loads some more data into raw_buf
// *
// * Returns TRUE if able to obtain at least one more byte, else FALSE.
// *
// * If raw_buf_index < raw_buf_len, the unprocessed bytes are transferred
// * down to the start of the buffer and then we load more data after that.
// * This case is used only when a frontend multibyte character crosses a
// * bufferload boundary.
// */
//static bool
//CopyLoadRawBuf(CopyState cstate)
//{
//	int			nbytes;
//	int			inbytes;
//
//	if (cstate->raw_buf_index < cstate->raw_buf_len)
//	{
//		/* Copy down the unprocessed data */
//		nbytes = cstate->raw_buf_len - cstate->raw_buf_index;
//		memmove(cstate->raw_buf, cstate->raw_buf + cstate->raw_buf_index,
//				nbytes);
//	}
//	else
//		nbytes = 0;	/* no data need be saved */
//
//	inbytes = CopyGetData(cstate, cstate->raw_buf + nbytes,
//						  1, RAW_BUF_SIZE - nbytes);
//	nbytes += inbytes;
//	cstate->raw_buf[nbytes] = '\0';
//	cstate->raw_buf_index = 0;
//	cstate->raw_buf_len = nbytes;
//	return (inbytes > 0);
//}
//
//
///*
// * Read the next input line and stash it in line_buf, with conversion to
// * server encoding.
// *
// * Result is true if read was terminated by EOF, false if terminated
// * by newline.	The terminating newline or EOF marker is not included
// * in the final value of line_buf.
// */
//static bool
//CopyReadLine(CopyState cstate, Controller IntegrityCheck)
//{
//	bool result;
//	resetStringInfo(&cstate->line_buf);
//
//	/* Mark that encoding conversion hasn't occurred yet */
//	cstate->line_buf_converted = false;
//
//	/* Parse data and transfer into line_buf */
//	result = CopyReadLineText(cstate);
//
//	if (result)
//	{
//		/*
//		 * Reached EOF.  In protocol version 3, we should ignore anything
//		 * after \. up to the protocol end of copy data.  (XXX maybe better
//		 * not to treat \. as special?)
//		 */
//		if (cstate->copy_dest == COPY_NEW_FE)
//		{
//			do
//			{
//				cstate->raw_buf_index = cstate->raw_buf_len;
//			} while (CopyLoadRawBuf(cstate));
//		}
//		//HIT EOF and cstate->line_buf.len in order to stop reading from relation
//		if(cstate->line_buf.len == 0 )
//		{//EOL pointers are ready
//			if( enable_tuple_metapointers )
//				finalizeMetaPointer(cstate->filePointers_ID);
//			if( enable_internal_metapointers )
//				setPositionalMapReady(cstate->internalFilePointers_ID, true);
//		}
//	}
//	else
//	{
//		/*
//		 * If we didn't hit EOF, then we must have transferred the EOL marker
//		 * to line_buf along with the data.  Get rid of it.
//		 */
//		switch (cstate->eol_type)
//		{
//			case EOL_NL:
//				Assert(cstate->line_buf.len >= 1);
//				Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n');
//
//				cstate->line_buf.len--;
//				cstate->line_buf.data[cstate->line_buf.len] = '\0';
//				if ( enable_tuple_metapointers )
//					addMetaPointer(cstate->line_buf.len + 1, cstate->filePointers_ID);
//				break;
//			case EOL_CR:
//				Assert(cstate->line_buf.len >= 1);
//				Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\r');
//
//				cstate->line_buf.len--;
//				cstate->line_buf.data[cstate->line_buf.len] = '\0';
//				if ( enable_tuple_metapointers )
//					addMetaPointer(cstate->line_buf.len + 1, cstate->filePointers_ID);
//				break;
//			case EOL_CRNL:
//				Assert(cstate->line_buf.len >= 2);
//				Assert(cstate->line_buf.data[cstate->line_buf.len - 2] == '\r');
//				Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n');
//
//				cstate->line_buf.len -= 2;
//				cstate->line_buf.data[cstate->line_buf.len] = '\0';
//				if ( enable_tuple_metapointers )
//					addMetaPointer(cstate->line_buf.len + 2, cstate->filePointers_ID);
//				break;
//			case EOL_UNKNOWN:
//				/* shouldn't get here */
//				Assert(false);
//				break;
//		}
//	}
//
//	/* Done reading the line.  Convert it to server encoding. */
//	if (cstate->need_transcoding  && !IntegrityCheck.disable_need_transcoding)
//	{
//		char	   *cvt;
//
//		cvt = pg_client_to_server(cstate->line_buf.data,
//								  cstate->line_buf.len);
//		if (cvt != cstate->line_buf.data)
//		{
//			/* transfer converted data back to line_buf */
//			resetStringInfo(&cstate->line_buf);
//			appendBinaryStringInfo(&cstate->line_buf, cvt, strlen(cvt));
//			pfree(cvt);
//		}
//	}
//
//	/* Now it's safe to use the buffer in error messages */
//	cstate->line_buf_converted = true;
//	return result;
//}
//
///*
// * CopyReadLineTextUsingMetaPointers - Just read from the file and pass the values to the Copystate line_buf
// */
//static bool
//CopyReadLineTextUsingMetaPointers (CopyState cstate)
//{
//	/* */
//	bool  	result = false;
//	int		offset;
//
//	/*Go to file and read tuples!*/
//	if (cstate->tupleStored == 0)
//	{
//		int stored = 0;
//		int inbytes, toRead;
//		Assert(cstate->raw_buf_index >= cstate->raw_buf_len);
//		Assert(cstate->copy_dest == COPY_FILE);
//
//		//Assumption: a tuple must fit in RAW_BUF_SIZE <-- we can change this by resizing the the RAW_BUF_SIZE after the first
//		toRead = computeBytesToRead(cstate->tupleRead, RAW_BUF_SIZE, cstate->filePointers_ID, &stored);
//
//		if(toRead == 0) {
//			Assert(cstate->tupleRead == getNumberOfTuples(cstate->filePointers_ID));
//			return true;
//		}
//
//		inbytes = fread(cstate->raw_buf, 1, toRead, cstate->copy_file);
//		if (ferror(cstate->copy_file))
//			ereport(ERROR,(errcode_for_file_access(),
//				errmsg("could not read from COPY file: %m")));
//
//		Assert(toRead == inbytes);
//
////		cstate->cur_tuplePointer += toRead;
//		cstate->tupleStored = stored;
//
//		cstate->raw_buf[inbytes] = '\0';
//		cstate->raw_buf_index = 0;
//		cstate->raw_buf_len = inbytes;
//	}
//
//	offset = getEndOfTuple(cstate->tupleRead, cstate->filePointers_ID);
//
//	cstate->tupleRead++;
//	cstate->tupleStored--;
//
//	appendBinaryStringInfo(&cstate->line_buf, cstate->raw_buf + cstate->raw_buf_index, offset);
//	cstate->raw_buf_index += offset;
//
//	return result;
//}
//
///*
// * Read the next input line and stash it in line_buf, with conversion to
// * server encoding.
// *
// * Result is true if read was terminated by EOF, false if terminated
// * by newline.	The terminating newline or EOF marker is not included
// * in the final value of line_buf.
// */
//static bool
//CopyReadLineUsingMetaPointers(CopyState cstate, Controller IntegrityCheck)
//{
//	bool result;
//
//	resetStringInfo(&cstate->line_buf);
//
//	/* Mark that encoding conversion hasn't occurred yet */
//	cstate->line_buf_converted = false;
//
//	result = CopyReadLineTextUsingMetaPointers(cstate);
//
//	if(!result)
//	{
//		Assert(cstate->line_buf.len >= 1);
//		if(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n')
//		{
//			cstate->line_buf.len--;
//			cstate->line_buf.data[cstate->line_buf.len] = '\0';
//		}
//		else if(cstate->line_buf.data[cstate->line_buf.len - 1] == '\r')
//		{
//			cstate->line_buf.len--;
//			cstate->line_buf.data[cstate->line_buf.len] = '\0';
//		}
//		else if(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n' && cstate->line_buf.data[cstate->line_buf.len - 2] == '\r')
//		{
//			cstate->line_buf.len -= 2;
//			cstate->line_buf.data[cstate->line_buf.len] = '\0';
//		}
//		else
//			Assert(false);
//	}
//
//	/* Done reading the line.  Convert it to server encoding. */
//	if (cstate->need_transcoding && !IntegrityCheck.disable_need_transcoding)
//	{
//		char	   *cvt;
//		cvt = pg_client_to_server(cstate->line_buf.data,
//								  cstate->line_buf.len);
//		if (cvt != cstate->line_buf.data)
//		{
//			/* transfer converted data back to line_buf */
//			resetStringInfo(&cstate->line_buf);
//			appendBinaryStringInfo(&cstate->line_buf, cvt, strlen(cvt));
//			pfree(cvt);
//		}
//	}
//
//	/* Now it's safe to use the buffer in error messages */
//	cstate->line_buf_converted = true;
//
//	return result;
//}
//
///*
// * CopyReadLineText - inner loop of CopyReadLine for text mode
// */
//static bool
//CopyReadLineText(CopyState cstate)
//{
//	char	   *copy_raw_buf;
//	int			raw_buf_ptr;
//	int			copy_buf_len;
//	bool		need_data = false;
//	bool		hit_eof = false;
//	bool		result = false;
//	char		mblen_str[2];
//
//	/* CSV variables */
//	bool		first_char_in_line = true;
//	bool		in_quote = false;
////				last_was_esc = false;
////	char		quotec = '\0';
////	char		escapec = '\0';
//
////	if (cstate->csv_mode)
////	{
////		quotec = cstate->quote[0];
////		escapec = cstate->escape[0];
////		/* ignore special escape processing if it's the same as quotec */
////		if (quotec == escapec)
////			escapec = '\0';
////	}
//
//	mblen_str[1] = '\0';
//
//	/*
//	 * The objective of this loop is to transfer the entire next input line
//	 * into line_buf.  Hence, we only care for detecting newlines (\r and/or
//	 * \n) and the end-of-copy marker (\.).
//	 *
//	 * In CSV mode, \r and \n inside a quoted field are just part of the data
//	 * value and are put in line_buf.  We keep just enough state to know if we
//	 * are currently in a quoted field or not.
//	 *
//	 * These four characters, and the CSV escape and quote characters, are
//	 * assumed the same in frontend and backend encodings.
//	 *
//	 * For speed, we try to move data from raw_buf to line_buf in chunks
//	 * rather than one character at a time.  raw_buf_ptr points to the next
//	 * character to examine; any characters from raw_buf_index to raw_buf_ptr
//	 * have been determined to be part of the line, but not yet transferred to
//	 * line_buf.
//	 *
//	 * For a little extra speed within the loop, we copy raw_buf and
//	 * raw_buf_len into local variables.
//	 */
//	copy_raw_buf = cstate->raw_buf;
//	raw_buf_ptr = cstate->raw_buf_index;
//	copy_buf_len = cstate->raw_buf_len;
//
//	for (;;)
//	{
//		int			prev_raw_ptr;
//		char		c;
//
//		/*
//		 * Load more data if needed.  Ideally we would just force four bytes
//		 * of read-ahead and avoid the many calls to
//		 * IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(), but the COPY_OLD_FE protocol
//		 * does not allow us to read too far ahead or we might read into the
//		 * next data, so we read-ahead only as far we know we can.	One
//		 * optimization would be to read-ahead four byte here if
//		 * cstate->copy_dest != COPY_OLD_FE, but it hardly seems worth it,
//		 * considering the size of the buffer.
//		 */
//		if (raw_buf_ptr >= copy_buf_len || need_data)
//		{
//			REFILL_LINEBUF;
//
//			/*
//			 * Try to read some more data.	This will certainly reset
//			 * raw_buf_index to zero, and raw_buf_ptr must go with it.
//			 */
//			if (!CopyLoadRawBuf(cstate))
//				hit_eof = true;
//			raw_buf_ptr = 0;
//			copy_buf_len = cstate->raw_buf_len;
//
//			/*
//			 * If we are completely out of data, break out of the loop,
//			 * reporting EOF.
//			 */
//			if (copy_buf_len <= 0)
//			{
//				result = true;
//				break;
//			}
//			need_data = false;
//		}
//
//		/* OK to fetch a character */
//		prev_raw_ptr = raw_buf_ptr;
//		c = copy_raw_buf[raw_buf_ptr++];
//
//		/* Process \r */
//		if (c == '\r' && (!in_quote))
//		{
//			/* Check for \r\n on first line, _and_ handle \r\n. */
//			if (cstate->eol_type == EOL_UNKNOWN ||
//				cstate->eol_type == EOL_CRNL)
//			{
//				/*
//				 * If need more data, go back to loop top to load it.
//				 *
//				 * Note that if we are at EOF, c will wind up as '\0' because
//				 * of the guaranteed pad of raw_buf.
//				 */
//				IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
//
//				/* get next char */
//				c = copy_raw_buf[raw_buf_ptr];
//
//				if (c == '\n')
//				{
//					raw_buf_ptr++;		/* eat newline */
//					cstate->eol_type = EOL_CRNL;		/* in case not set yet */
//				}
//				else
//				{
//					/* found \r, but no \n */
//					if (cstate->eol_type == EOL_CRNL)
//						ereport(ERROR,
//								(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//							errmsg("literal carriage return found in data"),
//						errhint("Use \"\\r\" to represent carriage return.") ));
//
//					/*
//					 * if we got here, it is the first line and we didn't find
//					 * \n, so don't consume the peeked character
//					 */
//					cstate->eol_type = EOL_CR;
//				}
//			}
//			else if (cstate->eol_type == EOL_NL)
//				ereport(ERROR,
//						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//						 errmsg("literal carriage return found in data") ,
//					   errhint("Use \"\\r\" to represent carriage return.") ));
//			/* If reach here, we have found the line terminator */
//			break;
//		}
//
//		/* Process \n */
//		if (c == '\n' && (!in_quote))
//		{
//			if (cstate->eol_type == EOL_CR || cstate->eol_type == EOL_CRNL)
//				ereport(ERROR,
//						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//						 errmsg("literal newline found in data"),
//						 errhint("Use \"\\n\" to represent newline.") ));
//			cstate->eol_type = EOL_NL;	/* in case not set yet */
//			/* If reach here, we have found the line terminator */
//			break;
//		}
//
//		/*
//		 * In CSV mode, we only recognize \. alone on a line.  This is because
//		 * \. is a valid CSV data value.
//		 */
//		if (c == '\\' && (first_char_in_line))
//		{
//			char		c2;
//
//			IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
//			IF_NEED_REFILL_AND_EOF_BREAK(0);
//
//			/* -----
//			 * get next character
//			 * Note: we do not change c so if it isn't \., we can fall
//			 * through and continue processing for client encoding.
//			 * -----
//			 */
//			c2 = copy_raw_buf[raw_buf_ptr];
//
//			if (c2 == '.')
//			{
//				raw_buf_ptr++;	/* consume the '.' */
//
//				/*
//				 * Note: if we loop back for more data here, it does not
//				 * matter that the CSV state change checks are re-executed; we
//				 * will come back here with no important state changed.
//				 */
//				if (cstate->eol_type == EOL_CRNL)
//				{
//					/* Get the next character */
//					IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
//					/* if hit_eof, c2 will become '\0' */
//					c2 = copy_raw_buf[raw_buf_ptr++];
//
//					if (c2 == '\n')
//					{
////						if (!cstate->csv_mode)
//							ereport(ERROR,
//									(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//									 errmsg("end-of-copy marker does not match previous newline style")));
////						else
////							NO_END_OF_COPY_GOTO;
//					}
//					else if (c2 != '\r')
//					{
////						if (!cstate->csv_mode)
//							ereport(ERROR,
//									(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//									 errmsg("end-of-copy marker corrupt")));
////						else
////							NO_END_OF_COPY_GOTO;
//					}
//				}
//
//				/* Get the next character */
//				IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
//				/* if hit_eof, c2 will become '\0' */
//				c2 = copy_raw_buf[raw_buf_ptr++];
//
//				if (c2 != '\r' && c2 != '\n')
//				{
////					if (!cstate->csv_mode)
//						ereport(ERROR,
//								(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//								 errmsg("end-of-copy marker corrupt")));
////					else
////						NO_END_OF_COPY_GOTO;
//				}
//
//				if ((cstate->eol_type == EOL_NL && c2 != '\n') ||
//					(cstate->eol_type == EOL_CRNL && c2 != '\n') ||
//					(cstate->eol_type == EOL_CR && c2 != '\r'))
//				{
//					ereport(ERROR,
//							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//							 errmsg("end-of-copy marker does not match previous newline style")));
//				}
//
//				/*
//				 * Transfer only the data before the \. into line_buf, then
//				 * discard the data and the \. sequence.
//				 */
//				if (prev_raw_ptr > cstate->raw_buf_index)
//					appendBinaryStringInfo(&cstate->line_buf,
//									 cstate->raw_buf + cstate->raw_buf_index,
//									   prev_raw_ptr - cstate->raw_buf_index);
//				cstate->raw_buf_index = raw_buf_ptr;
//				result = true;	/* report EOF */
//				break;
//			}
//			else //if (!cstate->csv_mode)
//
//				/*
//				 * If we are here, it means we found a backslash followed by
//				 * something other than a period.  In non-CSV mode, anything
//				 * after a backslash is special, so we skip over that second
//				 * character too.  If we didn't do that \\. would be
//				 * considered an eof-of copy, while in non-CVS mode it is a
//				 * literal backslash followed by a period.	In CSV mode,
//				 * backslashes are not special, so we want to process the
//				 * character after the backslash just like a normal character,
//				 * so we don't increment in those cases.
//				 */
//				raw_buf_ptr++;
//		}
//
//		/*
//		 * This label is for CSV cases where \. appears at the start of a
//		 * line, but there is more text after it, meaning it was a data value.
//		 * We are more strict for \. in CSV mode because \. could be a data
//		 * value, while in non-CSV mode, \. cannot be a data value.
//		 */
/////not_end_of_copy:
//
//		/*
//		 * Process all bytes of a multi-byte character as a group.
//		 *
//		 * We only support multi-byte sequences where the first byte has the
//		 * high-bit set, so as an optimization we can avoid this block
//		 * entirely if it is not set.
//		 */
//		if (cstate->encoding_embeds_ascii && IS_HIGHBIT_SET(c))
//		{
//			int			mblen;
//
//			mblen_str[0] = c;
//			/* All our encodings only read the first byte to get the length */
//			mblen = pg_encoding_mblen(cstate->client_encoding, mblen_str);
//			IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(mblen - 1);
//			IF_NEED_REFILL_AND_EOF_BREAK(mblen - 1);
//			raw_buf_ptr += mblen - 1;
//		}
//		first_char_in_line = false;
//	}							/* end of outer loop */
//
//	/*
//	 * Transfer any still-uncopied data to line_buf.
//	 */
//	REFILL_LINEBUF;
//
//	return result;
//}
//
//
///*
// *	Return decimal value for a hexadecimal digit
// */
//static int
//GetDecimalFromHex(char hex)
//{
//	if (isdigit((unsigned char) hex))
//		return hex - '0';
//	else
//		return tolower((unsigned char) hex) - 'a' + 10;
//}
//
///*
// * Parse the current line into separate attributes (fields),
// * performing de-escaping as needed.
// *
// * The input is in line_buf.  We use attribute_buf to hold the result
// * strings.  fieldvals[k] is set to point to the k'th attribute string,
// * or NULL when the input matches the null marker string.  (Note that the
// * caller cannot check for nulls since the returned string would be the
// * post-de-escaping equivalent, which may look the same as some valid data
// * string.)
// *
// * delim is the column delimiter string (must be just one byte for now).
// * null_print is the null marker string.  Note that this is compared to
// * the pre-de-escaped input string.
// *
// * The return value is the number of fields actually read.	(We error out
// * if this would exceed maxfields, which is the length of fieldvals[].)
// */
////Modified to add metapointers for SnoopDB
////Parse only useful attributes
//static void
//CopyReadAttributesText(CopyState cstate)
//{
//	char		delimc = cstate->delim[0];
//	int			fieldno;
//	char	   *output_ptr;
//	char	   *cur_ptr;
//	char	   *line_end_ptr;
//
//
//	//Added for SnoopDB
//	int total_length = 0;
//	int maxfields = cstate->nfields;
//	char **fieldvals = cstate->field_strings;
//	int *attributes = cstate->interesting_attributes;
//	int interesting_attributes = cstate->numOfInterestingAtt + cstate->numOfQualAtt;
//
//	/*
//	 * We need a special case for zero-column tables: check that the input
//	 * line is empty, and return.
//	 */
//	if (maxfields <= 0)
//	{
//		if (cstate->line_buf.len != 0)
//			ereport(ERROR,
//					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//					 errmsg("extra data after last expected column")));
//		return;
//	}
//
//	/* We don't need any attributes so don't parse anything (for count(*) queries)*/
//	if (interesting_attributes == 0) {
//		return;
//	}
//
//	resetStringInfo(&cstate->attribute_buf);
//
//	/*
//	 * The de-escaped attributes will certainly not be longer than the input
//	 * data line, so we can just force attribute_buf to be large enough and
//	 * then transfer data without any checks for enough space.	We need to do
//	 * it this way because enlarging attribute_buf mid-stream would invalidate
//	 * pointers already stored into fieldvals[].
//	 */
//	if (cstate->attribute_buf.maxlen <= cstate->line_buf.len)
//		enlargeStringInfo(&cstate->attribute_buf, cstate->line_buf.len);
//	output_ptr = cstate->attribute_buf.data;
//
//	/* set pointer variables for loop */
//	cur_ptr = cstate->line_buf.data;
//	line_end_ptr = cstate->line_buf.data + cstate->line_buf.len;
//
//	/* Outer loop iterates over fields */
//	fieldno = 0;
//	for (;;)
//	{
//		bool		found_delim = false;
//		char	   *start_ptr;
//		char	   *end_ptr;
//		int			input_len;
//		bool		saw_non_ascii = false;
//
//		/* Make sure space remains in fieldvals[] */
//		if (fieldno >= maxfields)
//			ereport(ERROR,
//					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//					 errmsg("extra data after last expected column")));
//
//		/* Remember start of field on both input and output sides */
//		start_ptr = cur_ptr;
//		fieldvals[fieldno] = output_ptr;
//
//		/* Scan data for field */
//		for (;;)
//		{
//			char		c;
//
//			end_ptr = cur_ptr;
//			if (cur_ptr >= line_end_ptr)
//				break;
//			c = *cur_ptr++;
//			if (c == delimc)
//			{
//				found_delim = true;
//				break;
//			}
//			if (c == '\\')
//			{
//				if (cur_ptr >= line_end_ptr)
//					break;
//				c = *cur_ptr++;
//				switch (c)
//				{
//					case '0':
//					case '1':
//					case '2':
//					case '3':
//					case '4':
//					case '5':
//					case '6':
//					case '7':
//						{
//							/* handle \013 */
//							int			val;
//
//							val = OCTVALUE(c);
//							if (cur_ptr < line_end_ptr)
//							{
//								c = *cur_ptr;
//								if (ISOCTAL(c))
//								{
//									cur_ptr++;
//									val = (val << 3) + OCTVALUE(c);
//									if (cur_ptr < line_end_ptr)
//									{
//										c = *cur_ptr;
//										if (ISOCTAL(c))
//										{
//											cur_ptr++;
//											val = (val << 3) + OCTVALUE(c);
//										}
//									}
//								}
//							}
//							c = val & 0377;
//							if (c == '\0' || IS_HIGHBIT_SET(c))
//								saw_non_ascii = true;
//						}
//						break;
//					case 'x':
//						/* Handle \x3F */
//						if (cur_ptr < line_end_ptr)
//						{
//							char		hexchar = *cur_ptr;
//
//							if (isxdigit((unsigned char) hexchar))
//							{
//								int			val = GetDecimalFromHex(hexchar);
//
//								cur_ptr++;
//								if (cur_ptr < line_end_ptr)
//								{
//									hexchar = *cur_ptr;
//									if (isxdigit((unsigned char) hexchar))
//									{
//										cur_ptr++;
//										val = (val << 4) + GetDecimalFromHex(hexchar);
//									}
//								}
//								c = val & 0xff;
//								if (c == '\0' || IS_HIGHBIT_SET(c))
//									saw_non_ascii = true;
//							}
//						}
//						break;
//					case 'b':
//						c = '\b';
//						break;
//					case 'f':
//						c = '\f';
//						break;
//					case 'n':
//						c = '\n';
//						break;
//					case 'r':
//						c = '\r';
//						break;
//					case 't':
//						c = '\t';
//						break;
//					case 'v':
//						c = '\v';
//						break;
//
//						/*
//						 * in all other cases, take the char after '\'
//						 * literally
//						 */
//				}
//			}
//
//			/* Add c to output string */
//			*output_ptr++ = c;
//		}
//
//		/* Terminate attribute value in output area */
//		*output_ptr++ = '\0';
//
//		/*
//		 * If we de-escaped a non-7-bit-ASCII char, make sure we still have
//		 * valid data for the db encoding. Avoid calling strlen here for the
//		 * sake of efficiency.
//		 */
//		if (saw_non_ascii)
//		{
//			char	   *fld = fieldvals[fieldno];
//			pg_verifymbstr(fld, output_ptr - (fld + 1), false);
//		}
//
//		/* Check whether raw input matched null marker */
//		input_len = end_ptr - start_ptr;
//		if (input_len == cstate->null_print_len &&
//			strncmp(start_ptr, cstate->null_print, input_len) == 0)
//			fieldvals[fieldno] = NULL;
//
//		//if attribute is selected
//		//total_length
//		total_length += input_len;
//		cstate->temp_positions[fieldno] = total_length;
//
//		fieldno++;
//		if( attributes[fieldno - 1])
//		{
//			interesting_attributes--;
//			if (interesting_attributes == 0) //All interesting attributes have been collected
//				break;
//		}
//
//		total_length++;
//
//		/* Done if we hit EOL instead of a delim */
//		if (!found_delim)
//			break;
//	}
//	/* Clean up state of attribute_buf */
//	output_ptr--;
//	Assert(*output_ptr == '\0');
//}
//
//
//
////Modified to add metapointers for SnoopDB if needed + select only interesting attributes
////CopyExec[pos].cstate->interesting_attributes
//static void
//CopyReadAttributesTextPushDown(CopyState cstate)
//{
//	char		delimc = cstate->delim[0];
//	int			fieldno;
//	char	   *cur_ptr;
//	char	   *line_end_ptr;
//
//	int maxfields = cstate->nfields;
//	char **fieldvals = cstate->field_strings;
//	int *attributes = cstate->interesting_attributes;
//	int interesting_attributes = cstate->numOfInterestingAtt  + cstate->numOfQualAtt;
//	/*
//	 * We need a special case for zero-column tables: check that the input
//	 * line is empty, and return.
//	 */
//	if (maxfields <= 0)
//	{
//		if (cstate->line_buf.len != 0)
//			ereport(ERROR,
//					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//					 errmsg("extra data after last expected column")));
//		return;
//	}
//
//	/* We don't need any attributes so don't parse anything (for count(*) queries)*/
//	if (interesting_attributes == 0) {
//		return;
//	}
//
//	/* set pointer variables for loop */
//	cur_ptr = cstate->line_buf.data;
//	line_end_ptr = cstate->line_buf.data + cstate->line_buf.len;
//
//	/* Outer loop iterates over fields */
//	fieldno = 0;
//	for (;;)
//	{
//		bool		found_delim = false;
//		char	   *start_ptr;
//		char	   *end_ptr;
//		int			input_len;
//		bool		saw_non_ascii = false;
//
//		/* Make sure space remains in fieldvals[] */
//		if (fieldno >= maxfields)
//			ereport(ERROR,
//					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
//					 errmsg("extra data after last expected column")));
//
//		/* Remember start of field on both input and output sides */
//		start_ptr = cur_ptr;
//		fieldvals[fieldno] = start_ptr;
//
//		/* Scan data for field */
//		for (;;)
//		{
//			char		c;
//
//			end_ptr = cur_ptr;
//			if (cur_ptr >= line_end_ptr)
//				break;
//			c = *cur_ptr++;
//			if (c == delimc)
//			{
//				found_delim = true;
//				break;
//			}
//
//
//			if (c == '\\')
//			{
//				if (cur_ptr >= line_end_ptr)
//					break;
//				c = *cur_ptr++;
//				switch (c)
//				{
//					case '0':
//					case '1':
//					case '2':
//					case '3':
//					case '4':
//					case '5':
//					case '6':
//					case '7':
//						{
//							/* handle \013 */
//							int			val;
//
//							val = OCTVALUE(c);
//							if (cur_ptr < line_end_ptr)
//							{
//								c = *cur_ptr;
//								if (ISOCTAL(c))
//								{
//									cur_ptr++;
//									val = (val << 3) + OCTVALUE(c);
//									if (cur_ptr < line_end_ptr)
//									{
//										c = *cur_ptr;
//										if (ISOCTAL(c))
//										{
//											cur_ptr++;
//											val = (val << 3) + OCTVALUE(c);
//										}
//									}
//								}
//							}
//							c = val & 0377;
//							if (c == '\0' || IS_HIGHBIT_SET(c))
//								saw_non_ascii = true;
//						}
//						break;
//					case 'x':
//						/* Handle \x3F */
//						if (cur_ptr < line_end_ptr)
//						{
//							char		hexchar = *cur_ptr;
//
//							if (isxdigit((unsigned char) hexchar))
//							{
//								int			val = GetDecimalFromHex(hexchar);
//
//								cur_ptr++;
//								if (cur_ptr < line_end_ptr)
//								{
//									hexchar = *cur_ptr;
//									if (isxdigit((unsigned char) hexchar))
//									{
//										cur_ptr++;
//										val = (val << 4) + GetDecimalFromHex(hexchar);
//									}
//								}
//								c = val & 0xff;
//								if (c == '\0' || IS_HIGHBIT_SET(c))
//									saw_non_ascii = true;
//							}
//						}
//						break;
//					case 'b':
//						c = '\b';
//						break;
//					case 'f':
//						c = '\f';
//						break;
//					case 'n':
//						c = '\n';
//						break;
//					case 'r':
//						c = '\r';
//						break;
//					case 't':
//						c = '\t';
//						break;
//					case 'v':
//						c = '\v';
//						break;
//
//						/*
//						 * in all other cases, take the char after '\'
//						 * literally
//						 */
//				}
//			}
//
//		}
//
//		/*
//		 * If we de-escaped a non-7-bit-ASCII char, make sure we still have
//		 * valid data for the db encoding. Avoid calling strlen here for the
//		 * sake of efficiency.
//		 */
//		if (saw_non_ascii)
//		{
//			char	   *fld = fieldvals[fieldno];
//			pg_verifymbstr(fld, end_ptr - (fld + 1), false);
//		}
//
//		/* Check whether raw input matched null marker */
//		input_len = end_ptr - start_ptr;
//		if (input_len == cstate->null_print_len &&
//			strncmp(start_ptr, cstate->null_print, input_len) == 0)
//			fieldvals[fieldno] = NULL;
//
//		*end_ptr = '\0';
//
//		if( attributes[fieldno])
//		{
//			interesting_attributes--;
//			if (interesting_attributes == 0) //All interesting attributes have been collected
//				break;
//		}
//
//		fieldno++;
//		/* Done if we hit EOL instead of a delim */
//		if (!found_delim)
//			break;
//	}
//}
//
///* Used in case we don't have any qual */
//static void
//CopyReadAttributesTextUsingInternalMetapointers_V1(CopyState cstate)
//{
//	int i;
//	int j;
//	int start;
//	int end;
//	int bytesToRead = 0;
//	FunctionCallInfoData *fcinfo;
//	Datum *values;
////	bool *nulls;
//	char *string;
//	int maxfields;
//
//	int endOfTuple;
//	int ptr = 0;
//	char *cur_ptr;
//
//	ParsingParameters *parameters;
//	InputFunctionsData *temp;
//	long currentTuple;
//	int whichRelation;
//
//	if (cstate->numOfInterestingAtt == 0)
//	{
//		cstate->tupleReadMetapointers++;
//		return;
//	}
//
//	temp = &InputFunctions[cstate->pos];
//	currentTuple = cstate->tupleReadMetapointers;
//	whichRelation = cstate->internalFilePointers_ID;
//
//	fcinfo = cstate->fcinfo;
//	values = cstate->values;
//	maxfields = cstate->nfields;
//
//	getInternalNeededMapMetapointers(currentTuple, whichRelation, cstate->attributes, cstate->numOfneededMapPositions);
//	/* set pointer variables for loop */
//	cur_ptr = cstate->line_buf.data;
//	endOfTuple = cstate->line_buf.len;
//	cstate->attributes[maxfields] = endOfTuple;
//
//
//	for ( j = 0; j < cstate->numOftoBeParsed; j++ )
//	{
//		ptr = cstate->toBeParsed[j];
//		parameters =  cstate->parameters + ptr;
//		if (!parameters->direction)
//		{
//			start = cstate->attributes[parameters->attribute_id];
//			parseLineForward(cstate, cstate->attributes, cur_ptr + start + 1, parameters->how_many, parameters->attribute_id);
//		}
//		else
//		{
//			end =  cstate->attributes[parameters->attribute_id];
//			parseLineBackward(cstate, cstate->attributes, cur_ptr + end - 1, parameters->how_many, parameters->attribute_id - 1);
//		}
//	}
//
//	for ( j = 0; j < temp->numOf_int2in; j++)
//	{
//		i = temp->_int2in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_int2in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_int4in; j++)
//	{
//		i = temp->_int4in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_int4in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_int8in; j++)
//	{
//		i = temp->_int8in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_int8in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_float4in; j++)
//	{
//		i = temp->_float4in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_float4in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_float8in; j++)
//	{
//		i = temp->_float8in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_float8in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_bpcharin; j++)
//	{
//		i = temp->_bpcharin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_bpcharin(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_varcharin; j++)
//	{
//		i = temp->_varcharin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_varcharin(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_boolin; j++)
//	{
//		i = temp->_boolin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_boolin(string);
//	}
//
//	for ( j = 0; j < temp->numOf_byteain; j++)
//	{
//		i = temp->_byteain[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_byteain(string);
//	}
//
//	for ( j = 0; j < temp->numOf_charin; j++)
//	{
//		i = temp->_charin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_charin(string);
//	}
//
//	for ( j = 0; j < temp->numOf_bit_in; j++)
//	{
//		i = temp->_bit_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_bit_in(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_varbit_in; j++)
//	{
//		i = temp->_varbit_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_varbit_in(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_date_in; j++)
//	{
//		i = temp->_date_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_date_in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_numeric_in; j++)
//	{
//		i = temp->_numeric_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_numeric_in(string,fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	//New pointers have been collected
//	updateInternalMapMetaPointers(cstate->attributes, cstate->internalFilePointers_ID, cstate->tupleReadMetapointers);
//	cstate->tupleReadMetapointers++;
//}
//
//
///* Used in case we have qual*/
//static bool
//CopyReadAttributesTextUsingInternalMetapointers_V2(CopyState cstate, List *qual, ExprContext *econtext)
//{
//	int i;
//	int j;
//	int start;
//	int end;
//	int bytesToRead = 0;
//	FunctionCallInfoData *fcinfo;
//	Datum *values;
////	bool *nulls;
//	char *string;
//	int maxfields;
//
//	int endOfTuple;
//	int ptr = 0;
//	char *cur_ptr;
//
//	ParsingParameters *parameters;
//	InputFunctionsData *temp;
//	long currentTuple;
//	int whichRelation;
//
//	currentTuple = cstate->tupleReadMetapointers;
//	whichRelation = cstate->internalFilePointers_ID;
//
//	fcinfo = cstate->fcinfo;
//	values = cstate->values;
////	nulls = cstate->nulls;
//	maxfields = cstate->nfields;
//
//	/*Copy Cached datum into cstate->values for where clause*/
//	if( enable_caching )
//		get_whereCacheDatum(currentTuple, cstate->cache_ID, cstate->values);
//
//	/*Initially collect the default pointers*/
//	getInternaldefaultNeededMapMetapointers(currentTuple, whichRelation, cstate->attributes, cstate->numOfdefaultneededMapPositions);
//
//	/* set pointer variables for loop */
//	cur_ptr = cstate->line_buf.data;
//	endOfTuple = cstate->line_buf.len;
//	cstate->attributes[maxfields] = endOfTuple;
//
//	/* If pointers have not been collected we parse ;-). As a result the pointers will be available even if the tuples does not qualify */
//	for ( j = 0; j < cstate->numOftoBeParsed; j++ )
//	{
//		ptr = cstate->toBeParsed[j];
//		parameters =  cstate->parameters + ptr;
//		if (!parameters->direction)
//		{
//			start = cstate->attributes[parameters->attribute_id];
//			parseLineForward(cstate, cstate->attributes, cur_ptr + start + 1, parameters->how_many, parameters->attribute_id);
//		}
//		else
//		{
//			end =  cstate->attributes[parameters->attribute_id];
//			parseLineBackward(cstate, cstate->attributes, cur_ptr + end - 1, parameters->how_many, parameters->attribute_id - 1);
//		}
//	}
//
//	temp = &Qual_InputFunctions[cstate->pos];
//	for ( j = 0; j < temp->numOf_int2in; j++)
//	{
//		i = temp->_int2in[j];
//
//		bytesToRead = cstate->attributes[i+1] - cstate->attributes[i] - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + cstate->attributes[i] + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_int2in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_int4in; j++)
//	{
//		i = temp->_int4in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_int4in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_int8in; j++)
//	{
//		i = temp->_int8in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_int8in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_float4in; j++)
//	{
//		i = temp->_float4in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_float4in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_float8in; j++)
//	{
//		i = temp->_float8in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_float8in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_bpcharin; j++)
//	{
//		i = temp->_bpcharin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_bpcharin(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_varcharin; j++)
//	{
//		i = temp->_varcharin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_varcharin(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_boolin; j++)
//	{
//		i = temp->_boolin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_boolin(string);
//	}
//
//	for ( j = 0; j < temp->numOf_byteain; j++)
//	{
//		i = temp->_byteain[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_byteain(string);
//	}
//
//	for ( j = 0; j < temp->numOf_charin; j++)
//	{
//		i = temp->_charin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_charin(string);
//	}
//
//	for ( j = 0; j < temp->numOf_bit_in; j++)
//	{
//		i = temp->_bit_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_bit_in(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_varbit_in; j++)
//	{
//		i = temp->_varbit_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_varbit_in(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_date_in; j++)
//	{
//		i = temp->_date_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_date_in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_numeric_in; j++)
//	{
//		i = temp->_numeric_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_numeric_in(string,fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//
//	econtext->ecxt_scantuple->tts_values = cstate->values;
//	econtext->ecxt_scantuple->tts_isnull = cstate->nulls;
//	econtext->ecxt_scantuple->tts_nvalid = maxfields;
//	/*Examine if the tuple qualifies*/
//	if (!(!qual || ExecQual(qual, econtext, false)))
//	{
//		updateInternalMapMetaPointers(cstate->attributes, cstate->internalFilePointers_ID, cstate->tupleReadMetapointers);
//		cstate->tupleReadMetapointers++;
//		return false;
//	}
//
//	if( enable_caching )
//		getCacheDatum(cstate->tupleReadMetapointers, cstate->cache_ID, cstate->values);
//	/* The tuple qualifies so retrieve the rest of the attributes*/
//	getInternalNeededMapMetapointers(currentTuple, whichRelation, cstate->attributes, cstate->numOfneededMapPositions);
//
//	temp = &InputFunctions[cstate->pos];
//	for ( j = 0; j < temp->numOf_int2in; j++)
//	{
//		i = temp->_int2in[j];
//
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_int2in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_int4in; j++)
//	{
//		i = temp->_int4in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_int4in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_int8in; j++)
//	{
//		i = temp->_int8in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_int8in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_float4in; j++)
//	{
//		i = temp->_float4in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_float4in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_float8in; j++)
//	{
//		i = temp->_float8in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_float8in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_bpcharin; j++)
//	{
//		i = temp->_bpcharin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_bpcharin(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_varcharin; j++)
//	{
//		i = temp->_varcharin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_varcharin(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_boolin; j++)
//	{
//		i = temp->_boolin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_boolin(string);
//	}
//
//	for ( j = 0; j < temp->numOf_byteain; j++)
//	{
//		i = temp->_byteain[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_byteain(string);
//	}
//
//	for ( j = 0; j < temp->numOf_charin; j++)
//	{
//		i = temp->_charin[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_charin(string);
//	}
//
//	for ( j = 0; j < temp->numOf_bit_in; j++)
//	{
//		i = temp->_bit_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_bit_in(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_varbit_in; j++)
//	{
//		i = temp->_varbit_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_varbit_in(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_date_in; j++)
//	{
//		i = temp->_date_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_date_in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_numeric_in; j++)
//	{
//		i = temp->_numeric_in[j];
//		start = cstate->attributes[i];
//		end = cstate->attributes[i+1];
//		bytesToRead = end - start - 1;
//		Assert(bytesToRead > 0);
//		string = cur_ptr + start + 1;
//		string[bytesToRead] = '\0'; //Change with arithmetic calc ;-)
//		values[i] = noDB_numeric_in(string,fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	updateInternalMapMetaPointers(cstate->attributes, cstate->internalFilePointers_ID, cstate->tupleReadMetapointers);
//	cstate->tupleReadMetapointers++;
//	return true;
//}
//
//
//
//
///*
// * Parse forward to retrieve pointers
// */
//static int
//parseLineForward(CopyState cstate, int* attributes, char *cur_ptr, int how_many, int attributeID)
//{
//	char		delimc = cstate->delim[0];
//
//	char	   *line_end_ptr = cstate->line_buf.data + cstate->line_buf.len;
//	int			input_len = 0;
//
//	bool		found_delim = false;
//	char	   *start_ptr;
//	char	   *end_ptr;
//	bool		saw_non_ascii = false;
//
//	/* Remember start of field on both input and output sides */
//	start_ptr = cur_ptr;
//	/* Scan data for field */
//	for (;;)
//	{
//		char		c;
//		end_ptr = cur_ptr;
//		if (cur_ptr >= line_end_ptr)
//			break;
//		c = *cur_ptr++;
//		if (c == delimc)
//		{
//			found_delim = true;
//			attributeID++;
//			attributes[attributeID] = end_ptr - cstate->line_buf.data;
////			cstate->attributes[attributeID + 1] = end_ptr - cstate->line_buf.data;
//
//			if( how_many == 0 )
//				break;
//
//			start_ptr = cur_ptr;
//			how_many--;
////			attributeID++;
//		}
//		if (c == '\\')
//		{
//			if (cur_ptr >= line_end_ptr)
//				break;
//			c = *cur_ptr++;
//			switch (c)
//			{
//				case '0':
//				case '1':
//				case '2':
//				case '3':
//				case '4':
//				case '5':
//				case '6':
//				case '7':
//					{
//						/* handle \013 */
//						int			val;
//
//						val = OCTVALUE(c);
//						if (cur_ptr < line_end_ptr)
//						{
//							c = *cur_ptr;
//							if (ISOCTAL(c))
//							{
//								cur_ptr++;
//								val = (val << 3) + OCTVALUE(c);
//								if (cur_ptr < line_end_ptr)
//								{
//									c = *cur_ptr;
//									if (ISOCTAL(c))
//									{
//										cur_ptr++;
//										val = (val << 3) + OCTVALUE(c);
//									}
//								}
//							}
//						}
//						c = val & 0377;
//						if (c == '\0' || IS_HIGHBIT_SET(c))
//							saw_non_ascii = true;
//					}
//					break;
//				case 'x':
//					/* Handle \x3F */
//					if (cur_ptr < line_end_ptr)
//					{
//						char		hexchar = *cur_ptr;
//
//						if (isxdigit((unsigned char) hexchar))
//						{
//							int			val = GetDecimalFromHex(hexchar);
//
//							cur_ptr++;
//							if (cur_ptr < line_end_ptr)
//							{
//								hexchar = *cur_ptr;
//								if (isxdigit((unsigned char) hexchar))
//								{
//									cur_ptr++;
//									val = (val << 4) + GetDecimalFromHex(hexchar);
//								}
//							}
//							c = val & 0xff;
//							if (c == '\0' || IS_HIGHBIT_SET(c))
//								saw_non_ascii = true;
//						}
//					}
//					break;
//				case 'b':
//					c = '\b';
//					break;
//				case 'f':
//					c = '\f';
//					break;
//				case 'n':
//					c = '\n';
//					break;
//				case 'r':
//					c = '\r';
//					break;
//				case 't':
//					c = '\t';
//					break;
//				case 'v':
//					c = '\v';
//					break;
//
//					/*
//					 * in all other cases, take the char after '\'
//					 * literally
//					 */
//			}
//		}
//		/* Add c to output string */
////		if( how_many == 0 && c != delimc)
////			*output_ptr++ = c;
//	}
//
////	/* Terminate attribute value in output area */
////	*output_ptr++ = '\0';
//
//	/*
//	 * If we de-escaped a non-7-bit-ASCII char, make sure we still have
//	 * valid data for the db encoding. Avoid calling strlen here for the
//	 * sake of efficiency.
//	 */
//	//TODO: add outside the code
//	//TODO: check for NULL size
//	if (saw_non_ascii)
//	{
////			char	   *fld = fieldvals[fieldno];
////			pg_verifymbstr(fld, output_ptr - (fld + 1), false);
//	}
//
//	//We parsed the last attribute and we hit EOL
//	if(!found_delim)
//	{
//		attributeID++;
//		attributes[attributeID] = cstate->line_buf.len;
//	}
//
//	/* Check whether raw input matched null marker */
//	//TODO: put this check outside the code
//	input_len = end_ptr - start_ptr;
//	if (input_len == cstate->null_print_len &&
//		strncmp(start_ptr, cstate->null_print, input_len) == 0)
//		return 0;
//
//
//	return input_len;
//}
//
///*
// * Parse backward to retrieve pointers
// */
//static int
//parseLineBackward(CopyState cstate, int *attributes, char *cur_ptr, int how_many, int attributeID)
//{
//	char		delimc = cstate->delim[0];
//
//	char	   *line_end_ptr = cstate->line_buf.data;
//	int			input_len = 0;
//
//	bool		found_delim = false;
//	char	   *start_ptr;
//	char	   *end_ptr;
//	bool		saw_non_ascii = false;
//
//	/* Remember start of field on both input and output sides */
//	start_ptr = cur_ptr;
//	/* Scan data for field */
//	for (;;)
//	{
//		char		c;
//		end_ptr = cur_ptr;
//		if (cur_ptr < line_end_ptr)
//			break;
//		c = *cur_ptr--;
//
//		if (c == delimc)
//		{
//			found_delim = true;
//			attributes[attributeID] = end_ptr - cstate->line_buf.data;
//
//			if( how_many == 0 ) {
//				break;
//			}
//
//			how_many--;
//			attributeID--;
//		}
//
//		if (c == '\\')
//		{
//			if (cur_ptr < line_end_ptr)
//				break;
//			c = *cur_ptr--;
//			switch (c)
//			{
//				case '0':
//				case '1':
//				case '2':
//				case '3':
//				case '4':
//				case '5':
//				case '6':
//				case '7':
//					{
//						/* handle \013 */
//						int			val;
//
//						val = OCTVALUE(c);
//						if (cur_ptr >= line_end_ptr)
//						{
//							c = *cur_ptr;
//							if (ISOCTAL(c))
//							{
//								cur_ptr--;
//								val = (val << 3) + OCTVALUE(c);
//								if (cur_ptr < line_end_ptr)
//								{
//									c = *cur_ptr;
//									if (ISOCTAL(c))
//									{
//										cur_ptr--;
//										val = (val << 3) + OCTVALUE(c);
//									}
//								}
//							}
//						}
//						c = val & 0377;
//						if (c == '\0' || IS_HIGHBIT_SET(c))
//							saw_non_ascii = true;
//					}
//					break;
//				case 'x':
//					/* Handle \x3F */
//					if (cur_ptr >= line_end_ptr)
//					{
//						char		hexchar = *cur_ptr;
//
//						if (isxdigit((unsigned char) hexchar))
//						{
//							int			val = GetDecimalFromHex(hexchar);
//
//							cur_ptr--;
//							if (cur_ptr < line_end_ptr)
//							{
//								hexchar = *cur_ptr;
//								if (isxdigit((unsigned char) hexchar))
//								{
//									cur_ptr--;
//									val = (val << 4) + GetDecimalFromHex(hexchar);
//								}
//							}
//							c = val & 0xff;
//							if (c == '\0' || IS_HIGHBIT_SET(c))
//								saw_non_ascii = true;
//						}
//					}
//					break;
//				case 'b':
//					c = '\b';
//					break;
//				case 'f':
//					c = '\f';
//					break;
//				case 'n':
//					c = '\n';
//					break;
//				case 'r':
//					c = '\r';
//					break;
//				case 't':
//					c = '\t';
//					break;
//				case 'v':
//					c = '\v';
//					break;
//
//					/*
//					 * in all other cases, take the char after '\'
//					 * literally
//					 */
//			}
//		}
//		/* Add c to output string */
////		if( how_many == 0 && c != delimc)
////			*output_ptr-- = c;
////		else
////			output_ptr--;
//	}
//
////	/* Terminate attribute value in output area */
////	*output_ptr++ = '\0';
//
//	/*
//	 * If we de-escaped a non-7-bit-ASCII char, make sure we still have
//	 * valid data for the db encoding. Avoid calling strlen here for the
//	 * sake of efficiency.
//	 */
//	if (saw_non_ascii)
//	{
////			char	   *fld = fieldvals[fieldno];
////			pg_verifymbstr(fld, output_ptr - (fld + 1), false);
//	}
//
//	//We didn't find delimeter ==> First attribute
//	if(!found_delim)
//	{
//		attributes[attributeID] = 0;
//	}
//
//
//	/* Check whether raw input matched null marker */
//	input_len = start_ptr - end_ptr;
//	if (input_len == cstate->null_print_len &&
////		strncmp(start_ptr, cstate->null_print, input_len) == 0)
//		strncmp(end_ptr, cstate->null_print, input_len) == 0)
//		return 0;
//
//
//	return input_len;
//}
//
//
//
//
///*
// * Create new tuple descriptor (TupleDesc) containing only the attributes specified by the which
// */
//TupleDesc
//getSelectiveTupleDesc(TupleDesc originalTupDesc, int *which, int size)
//{
//	int  i, j;
//
//	TupleDesc newTupDesc = (TupleDesc) palloc(sizeof(struct tupleDesc));
//	newTupDesc->natts = size;
//	newTupDesc->attrs = (Form_pg_attribute *)palloc(size * sizeof(Form_pg_attribute));
//	for ( i = 0; i < size; i++)
//		newTupDesc->attrs[i] = (Form_pg_attribute)palloc(sizeof(FormData_pg_attribute));
//
//	j = 0;
//	for ( i = 0; i < originalTupDesc->natts; i++)
//	{
////		if (which[i] == 1)
//		{
//			memcpy(newTupDesc->attrs[j], originalTupDesc->attrs[i], sizeof(FormData_pg_attribute));
//			j++;
//		}
//	}
//
//	newTupDesc->constr = originalTupDesc->constr;
//	newTupDesc->tdhasoid = originalTupDesc->tdhasoid;
//	newTupDesc->tdtypeid = originalTupDesc->tdtypeid;
//	newTupDesc->tdtypmod = originalTupDesc->tdtypmod;
//	newTupDesc->tdrefcount = originalTupDesc->tdrefcount;
//
//	return newTupDesc;
//}
//
//
///*It should be removed and merged with CopyAttributesText*/
//static void
//getSelectiveValues(CopyState cstate)
//{
//	char *string;
//	int i,j;
//	FunctionCallInfoData *fcinfo;
//	Datum *values;
////	bool *nulls;
//	InputFunctionsData *temp;
//
//	//This might add unneccessary overhead in case numOfInterestingAtt = 0
//	if(cstate->numOfInterestingAtt == 0)
//		return;
//
//	temp = &InputFunctions[cstate->pos];
//	fcinfo = cstate->fcinfo;
//	values = cstate->values;
////	nulls = cstate->nulls;
//
//
//
//	for ( j = 0; j < temp->numOf_int2in; j++)
//	{
//		i = temp->_int2in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_int2in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_int4in; j++)
//	{
//		i = temp->_int4in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_int4in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_int8in; j++)
//	{
//		i = temp->_int8in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_int8in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_float4in; j++)
//	{
//		i = temp->_float4in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_float4in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_float8in; j++)
//	{
//		i = temp->_float8in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_float8in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_bpcharin; j++)
//	{
//		i = temp->_bpcharin[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_bpcharin(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_varcharin; j++)
//	{
//		i = temp->_varcharin[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_varcharin(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_boolin; j++)
//	{
//		i = temp->_boolin[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_boolin(string);
//	}
//
//	for ( j = 0; j < temp->numOf_byteain; j++)
//	{
//		i = temp->_byteain[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_byteain(string);
//	}
//
//	for ( j = 0; j < temp->numOf_charin; j++)
//	{
//		i = temp->_charin[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_charin(string);
//	}
//
//	for ( j = 0; j < temp->numOf_bit_in; j++)
//	{
//		i = temp->_bit_in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_bit_in(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_varbit_in; j++)
//	{
//		i = temp->_varbit_in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_varbit_in(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_date_in; j++)
//	{
//		i = temp->_date_in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_date_in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_numeric_in; j++)
//	{
//		i = temp->_numeric_in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_numeric_in(string,fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//
//	//Run only for the interesting attributes
////	for ( j = 0; j < attnum; j++ )
////	{
////		i = cstate->interesting_attrPositions[j];
////
////		string = cstate->field_strings[i];
////
////		//They are not used...
////		//activeExec.cstate->cur_attname = NameStr(activeExec.status.attr[i]->attname);
////		//activeExec.cstate->cur_attval = string;
////		//TODO: remove pointer to function since we are aware of the data type
//////		values[i] = InputFunctionCall(&activeExec.status.in_functions[i],
//////									  string,
//////									  activeExec.status.typioparams[i],
//////									  activeExec.status.attr[i]->atttypmod);
//////		values[i] = FunctionCallInvoke(&activeExec.status.fcinfo[i]);
////		/*	Replace the FunctionCallInvoke with a switch statement with the basic data types */
////		/*
////		 * List with useful functions:
////		 * 38    : int2in   		{ 38, "int2in", 1, true, false, int2in },
////		 * 42    : int4in   		{ 42, "int4in", 1, true, false, int4in },
////		 * 460  : int8in   			{ 460, "int8in", 1, true, false, int8in },
////		 * 200  : float4in   		{ 200, "float4in", 1, true, false, float4in },
////		 * 214  : float8in   		{ 214, "float8in", 1, true, false, float8in },
////		 * 1044: bpcharin   		{ 1044, "bpcharin", 3, true, false, bpcharin },
////		 * 1046: varcharin   		{ 1046, "varcharin", 3, true, false, varcharin },
////		 * 1242: boolin   			{ 1242, "boolin", 1, true, false, boolin },
////		 * 1244: byteain   			{ 1244, "byteain", 1, true, false, byteain },
////		 * 1245: charin   			{ 1245, "charin", 1, true, false, charin },
////		 * 1564: bit_in   			{ 1564, "bit_in", 3, true, false, bit_in },
////		 * 1579: varbit_in   		{ 1579, "varbit_in", 3, true, false, varbit_in },
////		 * 1701: numeric_in  		{ 1701, "numeric_in", 3, true, false, numeric_in },
////		 * 1084: date_in            { 1084, "date_in", 1, true, false, date_in },
////       * 1701: numeric_in         { 1701, "numeric_in", 3, true, false, numeric_in },
////		 */
//
//}
//
//static void
//getQualSelectiveValues(CopyState cstate)
//{
//	char *string;
//	int i,j;
//	FunctionCallInfoData *fcinfo;
//	Datum *values;
////	bool *nulls;
//	InputFunctionsData *temp;
//
//	//This might add unneccessary overhead in case numOfInterestingAtt = 0
//	if(cstate->numOfQualAtt == 0)
//		return;
//
//	temp = &Qual_InputFunctions[cstate->pos];
//	fcinfo = cstate->fcinfo;
//	values = cstate->values;
////	nulls = cstate->nulls;
//
//	for ( j = 0; j < temp->numOf_int2in; j++)
//	{
//		i = temp->_int2in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_int2in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_int4in; j++)
//	{
//		i = temp->_int4in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_int4in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_int8in; j++)
//	{
//		i = temp->_int8in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_int8in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_float4in; j++)
//	{
//		i = temp->_float4in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_float4in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_float8in; j++)
//	{
//		i = temp->_float8in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_float8in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_bpcharin; j++)
//	{
//		i = temp->_bpcharin[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_bpcharin(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_varcharin; j++)
//	{
//		i = temp->_varcharin[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_varcharin(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_boolin; j++)
//	{
//		i = temp->_boolin[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_boolin(string);
//	}
//
//	for ( j = 0; j < temp->numOf_byteain; j++)
//	{
//		i = temp->_byteain[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_byteain(string);
//	}
//
//	for ( j = 0; j < temp->numOf_charin; j++)
//	{
//		i = temp->_charin[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_charin(string);
//	}
//
//	for ( j = 0; j < temp->numOf_bit_in; j++)
//	{
//		i = temp->_bit_in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_bit_in(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_varbit_in; j++)
//	{
//		i = temp->_varbit_in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_varbit_in(string, fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//
//	for ( j = 0; j < temp->numOf_date_in; j++)
//	{
//		i = temp->_date_in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_date_in(string);
//	}
//
//	for ( j = 0; j < temp->numOf_numeric_in; j++)
//	{
//		i = temp->_numeric_in[j];
//		string = cstate->field_strings[i];
//		values[i] = noDB_numeric_in(string,fcinfo[i].arg[1], fcinfo[i].arg[2]);
//	}
//}
//
//
//
//
///**************************************************************************/
///*
// * Only if (!qual && !projInfo)
// */
//TupleTableSlot*
//getNextTuple_PM(TupleTableSlot *ss_ScanTupleSlot, bool *done, int pos)
//{
//        TupleTableSlot *slot = NULL;
//
//        CopyState cstate;
//        CopyStmtExecStatus* status;
//        Controller IntegrityCheck;
//
//        cstate = CopyExec[pos].cstate;
//        status = &CopyExec[pos].status;
//        IntegrityCheck = CopyExec[pos].IntegrityCheck;
//
//        while (!*done)
//        {
//                bool            skip_tuple;
//
//                CHECK_FOR_INTERRUPTS();
//
//                // Reset the per-tuple exprcontext
//                ResetPerTupleExprContext(status->estate);
//                // Switch into its memory context
//                MemoryContextSwitchTo(GetPerTupleMemoryContext(status->estate));
//
//                *done = CopyReadLineUsingMetaPointers(cstate, IntegrityCheck);
//
//                /*
//                 * EOF at start of line means we're done.  If we see EOF after
//                 * some characters, we act as though it was newline followed by
//                 * EOF, ie, process the line and then exit loop on next iteration.
//                 */
//                if (*done && cstate->line_buf.len == 0)
//                        break;
//                CopyReadAttributesTextUsingInternalMetapointers_V1(cstate);
//
//                /* Generate a TupleTableSlot */
//                slot = generateTupleTableSlot(ss_ScanTupleSlot, cstate->values, cstate->nulls);
//
//                /* Triggers and stuff need to be invoked in query context. */
//                MemoryContextSwitchTo(status->oldcontext);
//
//                skip_tuple = false;
//                if (!skip_tuple)
//                {
//                        cstate->processed++;
//                        return slot;
//                }
//        }
//        /* File was scanned: Re-initialize the file pointer in case we re-scan the relation ;-) */
//        //TODO: Change to access only if there is an update in the structures
//        reInitRelation(cstate, *status);
//        return slot;
//}
//
///*
// * Only if (!qual && !projInfo)
// */
//TupleTableSlot*
//getNextTupleFileScan(TupleTableSlot *ss_ScanTupleSlot, bool *done, int pos)
//{
//	TupleTableSlot *slot = NULL;
//
//	CopyState cstate;
//	CopyStmtExecStatus* status;
//	Controller IntegrityCheck;
//
//	cstate = CopyExec[pos].cstate;
//	status = &CopyExec[pos].status;
//	IntegrityCheck = CopyExec[pos].IntegrityCheck;
//
//	while (!*done)
//	{
//		bool		skip_tuple;
//
//		CHECK_FOR_INTERRUPTS();
//
//		// Reset the per-tuple exprcontext
//		ResetPerTupleExprContext(status->estate);
//		// Switch into its memory context
//		MemoryContextSwitchTo(GetPerTupleMemoryContext(status->estate));
//
//		// Actually read the line into memory here
//		if( enable_tuple_metapointers ) //Metapointers in the end of each tuple accelerate future steps
//		{
//			if( FD[cstate->filePointers_ID].done )
//				*done = CopyReadLineUsingMetaPointers(cstate, IntegrityCheck);
//			else
//				*done = CopyReadLine(cstate, IntegrityCheck);
//		}
//		else //First time just collect the end-of-tuple pointers
//			*done = CopyReadLine(cstate, IntegrityCheck);
//
//
//		/*
//		 * EOF at start of line means we're done.  If we see EOF after
//		 * some characters, we act as though it was newline followed by
//		 * EOF, ie, process the line and then exit loop on next iteration.
//		 */
//		if (*done && cstate->line_buf.len == 0)
//			break;
//
//		/* Copy cached datum into cstate->values */
//		if( enable_caching ) {
//			getCacheDatum(cstate->processed, cstate->cache_ID, cstate->values);
//		}
//
//		/* Internal metapointers */
//		if( enable_internal_metapointers )
//		{	//There are available pointers to be used
//			if( isPositionalMapReady(cstate->internalFilePointers_ID) )	{
//				CopyReadAttributesTextUsingInternalMetapointers_V1(cstate);
//			}
//			else //Internal metapointers haven't been collected
//			{
//				CopyReadAttributesText(cstate);
//				addInternalMapMetaPointers(cstate->temp_positions, cstate->internalFilePointers_ID, cstate->processed);
//				getSelectiveValues(cstate);
//			}
//		}
////		else if( enable_pushdown_select ) //Internal metapointers haven't been enabled but pushdown_selection is enabled!
////			CopyReadAttributesTextPushDown(cstate);
//		else
//		{
//			CopyReadAttributesText(cstate);
//			getSelectiveValues(cstate);
//		}
//
//		/* Store datum into cache if needed */
//		if( enable_caching ) {
//			addCacheDatum(cstate->values, status->attr, cstate->cache_ID, cstate->processed);
//		}
//
//		/* Generate a TupleTableSlot */
//		slot = generateTupleTableSlot(ss_ScanTupleSlot, cstate->values, cstate->nulls);
//
//		/* Triggers and stuff need to be invoked in query context. */
//		MemoryContextSwitchTo(status->oldcontext);
//
//		skip_tuple = false;
//		if (!skip_tuple)
//		{
//			cstate->processed++;
//			return slot;
//		}
//	}
//	/* File was scanned: Re-initialize the file pointer in case we re-scan the relation ;-) */
//	//TODO: Change to access only if there is an update in the structures
//	reInitRelation(cstate, *status);
//	return slot;
//}






