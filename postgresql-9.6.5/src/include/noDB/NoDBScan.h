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

#ifndef NODBSCAN_H_
#define NODBSCAN_H_

#include "postgres.h"

#include "nodes/execnodes.h"
#include "nodes/parsenodes.h"
#include "executor/execdesc.h"

#include "tcop/dest.h"

#include "utils/elog.h"


#include "noDB/NoDBCache.h"
#include "noDB/NoDBScanStrategy.h"
#include "noDB/NoDBExecInfo.h"

#include "noDB/auxiliary/NoDBCol.h"
#include "noDB/auxiliary/NoDBPM.h"
#include "noDB/auxiliary/NoDBTimer.h"

#include "commands/explain.h"


//#define NUMBER_OF_RELATIONS         20
//#define MAX_RELATION_NAME           128



/*********************************************************/
/*********************************************************/
/*                   NoDB version 2                      */
/*********************************************************/
/*********************************************************/


/*
 * Represents the different source/dest cases we need to worry about at
 * the bottom level
 */
typedef enum CopyDest
{
	COPY_FILE,					/* to/from file */
	COPY_OLD_FE,				/* to/from frontend (2.0 protocol) */
	COPY_NEW_FE					/* to/from frontend (3.0 protocol) */
} CopyDest;


/*
 *	Represents the end-of-line terminator type of the input
 */
typedef enum EolType
{
	EOL_UNKNOWN,
	EOL_NL,
	EOL_CR,
	EOL_CRNL
} EolType;

/*
 *
 * This struct contains all the state variables used throughout a SCAN
 * operation. For simplicity, we use the same struct for all variants of SCAN,
 * even though some fields are used in only some cases.
 *
 */
typedef struct NoDBScanStateData_t
{
	/* low-level state data */
	CopyDest    copy_dest;              /* type of copy source/destination */
	FILE        *copy_file;             /* used if copy_dest == COPY_FILE */
	StringInfo  fe_msgbuf;              /* used for all dests during COPY TO, only for
	                                     * dest == COPY_NEW_FE in COPY FROM */
	bool        fe_eof;                 /* true if detected end of copy data */
	EolType     eol_type;               /* EOL type of input */
	int	        client_encoding;        /* remote side's character encoding */
	bool        need_transcoding;       /* client encoding diff from server? */
	bool        encoding_embeds_ascii;  /* ASCII can be non-first byte? */
	uint64      processed;              /* # of tuples processed */

	/* parameters from the COPY command */
	Relation    rel;                    /* relation to copy to or from */
	QueryDesc   *queryDesc;             /* executable query to copy from */
	List        *attnumlist;            /* integer list of attnums to copy */

	char        *filename;              /* filename, or NULL for STDIN/STDOUT */
	bool        binary;                 /* binary format? */

	bool        csv_mode;               /* Comma Separated Value format? */
	bool        header_line;            /* CSV header line? */
	bool		read_header;			/* Denote that the header has been read */

	char        *null_print;            /* NULL marker string (server encoding!) */
	int         null_print_len;         /* length of same */
	char        *delim;                 /* column delimiter (must be 1 byte) */
	char        *quote;                 /* CSV quote char (must be 1 byte) */
	char        *escape;                /* CSV escape char (must be 1 byte) */
	bool        *force_quote_flags;      /* per-column CSV FQ flags */
	bool        *force_notnull_flags;    /* per-column CSV FNN flags */


	/* these are just for error messages, see copy_in_error_callback */
	const char *cur_relname;	/* table name for error messages */
	int			cur_lineno;		/* line number for error messages */
	const char *cur_attname;	/* current att for error messages */
	const char *cur_attval;	/* current att value for error messages */


	/* Added  for NoDB */
	//TODO: check what is used in each fucntion
	FunctionCallInfoData    *fcinfo;
	Form_pg_attribute       *attr;
	int2                    *attrlen;

	EState                  *estate;
	MemoryContext           oldcontext;
	TupleDesc               tupDesc;

	Datum                   *values;
	bool                    *nulls;

	int                     nfields;            /* Number of fields in the tuple*/
	int                     lastfield;          /* Last field of the tuple to be accessed by a query */
	char                    **field_strings;

	long                    tupleRead;          /* Number of tuples read from the file */
	int                     tupleStored;        /* Store the number of tuples read from the input file after creating the end-of-tuple pointers */

    NoDBColVector_t         readAllAtts;        /* Set of attributes accessed from the query */
	NoDBColVector_t         readRestAtts;       /* Set of attributes accessed from the query */
    NoDBColVector_t         readFilterAtts;     /* Set of attributes accessed from the query */
    NoDBColVector_t			writeDataCols;
    NoDBColVector_t 		writePositionCols;
    NoDBTimer_t             *timer;

    NoDBPMPair_t            *attributes;        /* Temporally store newly discovered metapointers */
//    NoDBPointer             *tempPositions;     /*Copy available Internal metapointers during initial search*/

	struct NoDBPlanState_t  *plan;
	NoDBExecInfo_t          *execInfo;

	// For generating the tuple
	TupleTableSlot          *ss_ScanTupleSlot;
	List                    *qual;
	ExprContext             *econtext;
	TupleTableSlot          *(*execNoDBPlan) (struct NoDBScanStateData_t *cstate, bool *pass);

	/*
	 * These variables are used to reduce overhead in textual COPY FROM.
	 *
	 * attribute_buf holds the separated, de-escaped text for each field of
	 * the current line.  The CopyReadAttributes functions return arrays of
	 * pointers into this buffer.  We avoid palloc/pfree overhead by re-using
	 * the buffer on each cycle.
	 */
	StringInfoData attribute_buf;

	/*
	 * Similarly, line_buf holds the whole input line being processed. The
	 * input cycle is first to read the whole line into line_buf, convert it
	 * to server encoding there, and then extract the individual attribute
	 * fields into attribute_buf.  line_buf is preserved unmodified so that we
	 * can display it in error messages if appropriate.
	 */
	StringInfoData  line_buf;
	bool            line_buf_converted;		/* converted to server encoding? */

	/*
	 * Finally, raw_buf holds raw data read from the data source (file or
	 * client connection).	CopyReadLine parses this data sufficiently to
	 * locate line boundaries, then transfers the data to line_buf and
	 * converts it.  Note: we guarantee that there is a \0 at
	 * raw_buf[raw_buf_len].
	 */
#define RAW_BUF_SIZE 32768		/* we palloc RAW_BUF_SIZE+1 bytes */
	char	   *raw_buf;
	int			raw_buf_index;	/* next byte to process */
	int			raw_buf_len;	/* total # of bytes stored */
} NoDBScanStateData_t;

typedef NoDBScanStateData_t *NoDBScanState_t;

/* DestReceiver for COPY (SELECT) TO */
typedef struct
{
	DestReceiver    pub;			/* publicly-known function pointers */
	NoDBScanState_t cstate;			/* CopyStateData for the command */
} DR_copy;


/*
 * Information needed for GetNextTuple...
 */
typedef struct NoDBScanExecStatusStmt_t
{
//	EState	   *estate;
	AttrNumber	num_phys_attrs;
	AttrNumber	num_defaults;
	AttrNumber	attr_count;

	bool		file_has_oids;
//	Form_pg_attribute *attr;
	FmgrInfo   *in_functions;
	FmgrInfo	oid_in_function;


	Oid		   *typioparams;
	Oid			oid_typioparam;
	Oid			in_func_oid;

	ExprContext *econtext;
	int		   *defmap;
	ExprState **defexprs;
//	TupleDesc	tupDesc;
	TupleTableSlot *slot;

	ResultRelInfo *resultRelInfo;

//	ErrorContextCallback errcontext;
	ErrorContextCallback noDBerrcontext;
	int	hi_options;
//	MemoryContext oldcontext;

} NoDBScanExecStatusStmt_t;




typedef struct NoDBScanOperator_t
{
    NoDBScanState_t             cstate;
    NoDBScanExecStatusStmt_t    status;
    CopyStmt                    *planCopyStmt;
} NoDBScanOperator_t;



typedef struct NoDBPlanState_t
{
    NoDBCache_t                 *eolCache;		/* EOL Cache for current relation */
    bool                        (*readFile) (NoDBScanState_t cstate);

    NoDBScanStrategyIterator_t  *iterator;
    NoDBScanStrategy_t          *curStrategy;	/* Read strategy */

    long int tuplesToRead;                      /* Number of rows to be read in cur strategy*/
    long int nRows;
    long int nEOL;
    long int nMin;

    bool    hasEOL;                    /* Has EOL for rows */
} NoDBPlanState_t;



//TODO: move to NoDBExecInfo.h (???)
extern bool enable_invisible_db;
extern bool enable_tuple_metapointers;
extern bool enable_internal_metapointers;
extern bool enable_pushdown_select;



NoDBScanOperator_t		*NoDBScanOperatorInit(ScanState *scanInfo);
void 					NoDBScanStateReInit(ScanState *scanInfo);
void                    NoDBScanOperatorDestroy(NoDBScanOperator_t *scanOper);

void                    NoDBUpdateStrategy(NoDBScanState_t cstate);
void                    NoDBResetStrategy(NoDBScanState_t cstate);
void                    NoDBFinalizeStrategy(NoDBScanState_t cstate);

//TODO: add function for re-init the file pointer and the reading strategy --> Needed for nested loops


//TODO: move to queryDescriptor
List *traversePlanTree(Plan *plan, PlanState *planstate, Plan *outer_plan, PlannedStmt *topPlan, List *ancestors, ExplainState *es);
bool ExplainPreScanNode(PlanState *planstate, Bitmapset **rels_used);




#endif /* NODBSCAN_H_ */
