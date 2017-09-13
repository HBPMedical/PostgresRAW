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

//
//
//#include "postgres.h"
//
//#include "access/heapam.h"
//#include "access/transam.h"
//#include "access/tupconvert.h"
//#include "access/tuptoaster.h"
//#include "access/xact.h"
//#include "catalog/index.h"
//#include "catalog/indexing.h"
//#include "catalog/namespace.h"
//#include "catalog/pg_inherits_fn.h"
//#include "catalog/pg_namespace.h"
//#include "commands/dbcommands.h"
//#include "commands/vacuum.h"
//#include "executor/executor.h"
//#include "miscadmin.h"
//#include "nodes/nodeFuncs.h"
//#include "parser/parse_oper.h"
//#include "parser/parse_relation.h"
//#include "pgstat.h"
//#include "postmaster/autovacuum.h"
//#include "storage/bufmgr.h"
//#include "storage/proc.h"
//#include "storage/procarray.h"
//#include "utils/acl.h"
//#include "utils/attoptcache.h"
//#include "utils/datum.h"
//#include "utils/guc.h"
//#include "utils/lsyscache.h"
//#include "utils/memutils.h"
//#include "utils/pg_rusage.h"
//#include "utils/syscache.h"
//#include "utils/tuplesort.h"
//#include "utils/tqual.h"
//
//
//#include "noDB/NoDBStatistics.h"
//#include "noDB/NoDBScanStrategy.h"
//
//
//
//
///* Data structure for Algorithm S from Knuth 3.4.2 */
//typedef struct
//{
//	BlockNumber N;				/* number of blocks, known in advance */
//	int			n;				/* desired sample size */
//	BlockNumber t;				/* current block number */
//	int			m;				/* blocks selected so far */
//} BlockSamplerData;
//
//typedef BlockSamplerData *BlockSampler;
//
//
//#define WIDTH_THRESHOLD  1024
//
//#define swapInt(a,b)	do {int _tmp; _tmp=a; a=b; b=_tmp;} while(0)
//#define swapDatum(a,b)	do {Datum _tmp; _tmp=a; a=b; b=_tmp;} while(0)
//
///*
// * Extra information used by the default analysis routines
// */
//typedef struct
//{
//	Oid			eqopr;			/* '=' operator for datatype, if any */
//	Oid			eqfunc;			/* and associated function */
//	Oid			ltopr;			/* '<' operator for datatype, if any */
//} StdAnalyzeData;
//
//
//typedef struct
//{
//	int			count;			/* # of duplicates */
//	int			first;			/* values[] index of first occurrence */
//} ScalarMCVItem;
//
//typedef struct
//{
//	FmgrInfo   *cmpFn;
//	int			cmpFlags;
//	int		   *tupnoLink;
//} CompareScalarsContext;
//
//
//
//static VacAttrStats *NoDB_examine_attribute(Relation onerel, int attnum, Node *index_expr);
//static bool NoDB_std_typanalyze(VacAttrStats *stats);
//static void NoDB_compute_minimal_stats(int whichRelation, int whichAttribute, VacAttrStatsP stats, int *rows, int samplerows, double totalrows);
//static void NoDB_compute_scalar_stats(int whichRelation, int whichAttribute, VacAttrStatsP stats, int *rows, int samplerows, double totalrows);
//static int NoDB_compare_scalars(const void *a, const void *b, void *arg);
//static int NoDB_compare_mcvs(const void *a, const void *b);
//static int NoDB_acquire_sample_rows(int *rows, int targrows, long totalrows);
//
//
///*don't try this at home....*/
//static void compute_scalar_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows);
//static void compute_minimal_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows);
//static void update_attstats(Oid relid, bool inh, int natts, VacAttrStats **vacattrstats);
//
//static MemoryContext anl_context2 = NULL;
//
//
//
////void
////initializeStatistics(int pos, Relation rel,  int numberOfAttributes, int *which)
////{
////	int i;
////	int id;
////	Form_pg_attribute *attr;
////	char *relation = rel->rd_rel->relname.data;
////
////	relation = rel->rd_rel->relname.data;
////	attr = rel->rd_att->attrs;
////
////	if( ! RawStatisticalMap[pos].initialized)
////	{
////		strcpy(RawStatisticalMap[pos].relation, relation);
////		RawStatisticalMap[pos].available = createBitMap(numberOfAttributes);
////		for (i = 0 ; i < numberOfAttributes; i++)
////			setBitValue(RawStatisticalMap[pos].available, i, 0);
////		RawStatisticalMap[pos].toBeProcessed = (int*) malloc ( numberOfAttributes  * sizeof(int));
////		RawStatisticalMap[pos].numOftoBeProcessed = 0;
////		for (i = 0 ; i < numberOfAttributes; i++)
////			RawStatisticalMap[pos].toBeProcessed[i] = 0;
////
////		statisticalMap_usedFiles++;
////		RawStatisticalMap[pos].initialized = true;
////
////		/*
////		 * attstattarget:
////		 * "0" : do  not collect any stats about this column.
////		 * "-1": use default.
////		 */
////		RawStatisticalMap[pos].attstattarget = (int*) malloc ( numberOfAttributes  * sizeof(int));
////		for (i = 0 ; i < numberOfAttributes; i++)
////			RawStatisticalMap[pos].attstattarget[i] = attr[i]->attstattarget;
////
////		/* if the caching is not enabled */
////		if ( !enable_caching )
////		{
////
////		}
////	}
////
////	id = 0;
////	for (i = 0 ; i < numberOfAttributes; i++)
////	{
////		if( which[i] && RawStatisticalMap[pos].attstattarget[i] != 0)
////		{
////			if ( !getBit(RawStatisticalMap[pos].available, i) )
////			{
////				RawStatisticalMap[pos].toBeProcessed[id++] = i;
////				setBitValue(RawStatisticalMap[pos].available, i, 1);
////			}
////		}
////	}
////
////	RawStatisticalMap[pos].numOftoBeProcessed = id;
////
////	/* if the caching is not enabled */
////	if ( !enable_caching )
////	{
////		fprintf(stderr,"\n\nEnable caching to collect statistics\n\n");
////
////	}
////
////}
//
//
//
//void
//NoDB_analyze_rel(NoDBScanState_t cstate, Relation onerel)
//{
//	int			attr_cnt = 0,
//				tcnt,
//				i;
//	int whichRelation;
//	VacAttrStats **vacattrstats;
//
//	int			targrows,
//				numrows;
//	double		totalrows,
//				totaldeadrows = 0.0;
//	int  *rows;
//	MemoryContext caller_context;
//
//
//	bool update_reltuples = true;
//	bool inh = false;
//
//	int* toCollect;
//	int nToCollect = 0;
//
//	NoDBExecInfo_t *execinfo = cstate->execInfo;
//
//
//	toCollect = (int*)palloc(NoDBColVectorSize(cstate->writeDataCols) * sizeof(int));
//
//	for (i = 0; i < NoDBColVectorSize(cstate->writeDataCols); i++)
//	{
//		NoDBCol_t column =  NoDBColVectorGet(cstate->writeDataCols, i);
//		if ( !NoDBBitmapIsSet(execinfo->relStats.statsCollected, column) && onerel->rd_att->attrs[column]->attstattarget != 0)
//		{
//			toCollect[nToCollect++] = column;
//			NoDBBitmapSet(execinfo->relStats.statsCollected, column);
//		}
//	}
//
//	if ( nToCollect == 0)
//	{
//		pfree(toCollect);
//		return;
//	}
//
//	//Get list with caches per attributes
//	NoDBList_t **listsForCollect = (NoDBList_t**)palloc(nToCollect * sizeof(NoDBList_t*));
//	for (i=0 ;i < nToCollect; i++)
//	{
//		NoDBList_t  *caches = NULL;
//		NoDBList_t  *allCaches;
//		NoDBCol_t column = toCollect[i];
//
//		caches = NULL;
//		for (allCaches = NoDBCaches; allCaches; allCaches = allCaches->next) {
//			NoDBCache_t *cache;
//			cache = allCaches->ptr;
//			if (NoDBCacheGetType(cache) == NODB_DATA_CACHE && NoDBCacheHasColumn(cache, column)) {
//				caches =  NoDBListAdd(caches, cache);
//			}
//		}
//		listsForCollect[i] = caches;
//	}
//
//
//	/*
//	 * Set up a working context so that we can easily free whatever junk gets
//	 * created.
//	 */
//	anl_context2 = AllocSetContextCreate(CurrentMemoryContext,
//										"Analyze",
//										ALLOCSET_DEFAULT_MINSIZE,
//										ALLOCSET_DEFAULT_INITSIZE,
//										ALLOCSET_DEFAULT_MAXSIZE);
//	caller_context = MemoryContextSwitchTo(anl_context2);
//
//
//	/*
//	 * Determine which columns to analyze
//	 *
//	 * Note that system attributes are never analyzed.
//	 */
//	vacattrstats = (VacAttrStats **) palloc( nToCollect * sizeof(VacAttrStats *));
//	tcnt = 0;
//	for (i = 0; i < nToCollect; i++)
//	{
//		vacattrstats[tcnt] = NoDB_examine_attribute(onerel, toCollect[i] + 1, NULL);
//		if (vacattrstats[tcnt] != NULL)
//			tcnt++;
//	}
//	attr_cnt = tcnt;
//
//
//	if (attr_cnt <= 0 )
//		goto cleanup;
//
//
//	targrows = 100;
//	for (i = 0; i < attr_cnt; i++)
//	{
//		if (targrows < vacattrstats[i]->minrows)
//			targrows = vacattrstats[i]->minrows;
//	}
//
//	/*
//	 * Acquire the sample rows
//	 */
//	totalrows = NoDBExecGetNumberOfRows(execinfo);
//
//	rows = (int *) palloc(targrows * sizeof(int));
//	numrows = NoDB_acquire_sample_rows(rows, targrows, totalrows);
//
//
//	/*
//	 * Compute the statistics.	Temporary results during the calculations for
//	 * each column are stored in a child context.  The calc routines are
//	 * responsible to make sure that whatever they store into the VacAttrStats
//	 * structure is allocated in anl_context.
//	 */
//	if (numrows > 0)
//	{
//		MemoryContext col_context,
//					old_context;
//
//		col_context = AllocSetContextCreate(anl_context2,
//											"Analyze Column",
//											ALLOCSET_DEFAULT_MINSIZE,
//											ALLOCSET_DEFAULT_INITSIZE,
//											ALLOCSET_DEFAULT_MAXSIZE);
//		old_context = MemoryContextSwitchTo(col_context);
//
//		for (i = 0; i < attr_cnt; i++)
//		{
//			VacAttrStats *stats = vacattrstats[i];
//			AttributeOpts *aopt = get_attribute_options(onerel->rd_id, stats->attr->attnum);
//
//			stats->tupDesc = onerel->rd_att;
//
//			if (stats->compute_stats == (void*) compute_scalar_stats)
//				NoDB_compute_scalar_stats(whichRelation, toCollect[i], stats, rows, numrows, totalrows);
//			else if(stats->compute_stats == (void*) compute_minimal_stats)
//				NoDB_compute_minimal_stats(whichRelation, toCollect[i], stats, rows, numrows, totalrows);
//			else
//				fprintf(stderr,"\nError in sampling\n");
//
//
//			/*
//			 * If the appropriate flavor of the n_distinct option is
//			 * specified, override with the corresponding value.
//			 */
//			if (aopt != NULL)
//			{
//				float8		n_distinct =
//				inh ? aopt->n_distinct_inherited : aopt->n_distinct;
//
//				if (n_distinct != 0.0)
//					stats->stadistinct = n_distinct;
//			}
//
//			MemoryContextResetAndDeleteChildren(col_context);
//		}
//
//		MemoryContextSwitchTo(old_context);
//		MemoryContextDelete(col_context);
//
//		/*
//		 * Emit the completed stats rows into pg_statistic, replacing any
//		 * previous statistics for the target columns.	(If there are stats in
//		 * pg_statistic for columns we didn't process, we leave them alone.)
//		 */
//		update_attstats(RelationGetRelid(onerel), inh,
//						attr_cnt, vacattrstats);
//
//	}
//
//	/*
//	 * Update pages/tuples stats in pg_class, but not if we're inside a VACUUM
//	 * that got a more precise number.
//	 */
//	if (update_reltuples)
//	{
//		vac_update_relstats(onerel,
//							RelationGetNumberOfBlocks(onerel),
//							totalrows, false, InvalidTransactionId);
//	}
//	/*
//	 * Report ANALYZE to the stats collector, too; likewise, tell it to adopt
//	 * these numbers only if we're not inside a VACUUM that got a better
//	 * number.	However, a call with inh = true shouldn't reset the stats.
//	 */
//	if (!inh)
//		pgstat_report_analyze(onerel, update_reltuples,
//							  totalrows, totaldeadrows);
//
//	/* We skip to here if there were no analyzable columns */
//cleanup:
//
//	/* Restore current context and release memory */
//	MemoryContextSwitchTo(caller_context);
//	MemoryContextDelete(anl_context2);
//	anl_context2 = NULL;
//}
//
//
//
//
//static VacAttrStats *
//NoDB_examine_attribute(Relation onerel, int attnum, Node *index_expr)
//{
//	Form_pg_attribute attr = onerel->rd_att->attrs[attnum - 1];
//	HeapTuple	typtuple;
//	VacAttrStats *stats;
//	int			i;
//	bool		ok;
//
//	/* Never analyze dropped columns */
//	if (attr->attisdropped)
//		return NULL;
//
//	/* Don't analyze column if user has specified not to */
//	if (attr->attstattarget == 0)
//		return NULL;
//
//	/*
//	 * Create the VacAttrStats struct.	Note that we only have a copy of the
//	 * fixed fields of the pg_attribute tuple.
//	 */
//	stats = (VacAttrStats *) palloc0(sizeof(VacAttrStats));
//	stats->attr = (Form_pg_attribute) palloc(ATTRIBUTE_FIXED_PART_SIZE);
//	memcpy(stats->attr, attr, ATTRIBUTE_FIXED_PART_SIZE);
//
//
//	/*
//	 * When analyzing an expression index, believe the expression tree's type
//	 * not the column datatype --- the latter might be the opckeytype storage
//	 * type of the opclass, which is not interesting for our purposes.  (Note:
//	 * if we did anything with non-expression index columns, we'd need to
//	 * figure out where to get the correct type info from, but for now that's
//	 * not a problem.)  It's not clear whether anyone will care about the
//	 * typmod, but we store that too just in case.
//	 */
//	if (index_expr)
//	{
//		stats->attrtypid = exprType(index_expr);
//		stats->attrtypmod = exprTypmod(index_expr);
//	}
//	else
//	{
//		stats->attrtypid = attr->atttypid;
//		stats->attrtypmod = attr->atttypmod;
//	}
//
//	typtuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(stats->attrtypid));
//	if (!HeapTupleIsValid(typtuple))
//		elog(ERROR, "cache lookup failed for type %u", stats->attrtypid);
//	stats->attrtype = (Form_pg_type) palloc(sizeof(FormData_pg_type));
//	memcpy(stats->attrtype, GETSTRUCT(typtuple), sizeof(FormData_pg_type));
//	ReleaseSysCache(typtuple);
//	stats->anl_context = anl_context2;
//	stats->tupattnum = attnum;
//
//	/*
//	 * The fields describing the stats->stavalues[n] element types default to
//	 * the type of the data being analyzed, but the type-specific typanalyze
//	 * function can change them if it wants to store something else.
//	 */
//	for (i = 0; i < STATISTIC_NUM_SLOTS; i++)
//	{
//		stats->statypid[i] = stats->attrtypid;
//		stats->statyplen[i] = stats->attrtype->typlen;
//		stats->statypbyval[i] = stats->attrtype->typbyval;
//		stats->statypalign[i] = stats->attrtype->typalign;
//	}
//
//	/*
//	 * Call the type-specific typanalyze function.	If none is specified, use
//	 * std_typanalyze().
//	 */
//	if (OidIsValid(stats->attrtype->typanalyze))
//		ok = DatumGetBool(OidFunctionCall1(stats->attrtype->typanalyze,
//										   PointerGetDatum(stats)));
//	else
//		ok = NoDB_std_typanalyze(stats);
//
//	if (!ok || stats->compute_stats == NULL || stats->minrows <= 0)
//	{
//		pfree(stats->attrtype);
//		pfree(stats->attr);
//		pfree(stats);
//		return NULL;
//	}
//
//	return stats;
//}
//
//
//
///*
// * std_typanalyze -- the default type-specific typanalyze function
// */
//static bool
//NoDB_std_typanalyze(VacAttrStats *stats)
//{
//	Form_pg_attribute attr = stats->attr;
//	Oid			ltopr;
//	Oid			eqopr;
//	StdAnalyzeData *mystats;
//
//	/* If the attstattarget column is negative, use the default value */
//	/* NB: it is okay to scribble on stats->attr since it's a copy */
//	if (attr->attstattarget < 0)
//		attr->attstattarget = default_statistics_target;
//
//	/* Look for default "<" and "=" operators for column's type */
//	get_sort_group_operators(stats->attrtypid,
//							 false, false, false,
//							 &ltopr, &eqopr, NULL);
//
//	/* If column has no "=" operator, we can't do much of anything */
//	if (!OidIsValid(eqopr))
//		return false;
//
//	/* Save the operator info for compute_stats routines */
//	mystats = (StdAnalyzeData *) palloc(sizeof(StdAnalyzeData));
//	mystats->eqopr = eqopr;
//	mystats->eqfunc = get_opcode(eqopr);
//	mystats->ltopr = ltopr;
//	stats->extra_data = mystats;
//
//	/*
//	 * Determine which standard statistics algorithm to use
//	 */
//	if (OidIsValid(ltopr))
//	{
//		/* Seems to be a scalar datatype */
//		stats->compute_stats = compute_scalar_stats;
//		/*--------------------
//		 * The following choice of minrows is based on the paper
//		 * "Random sampling for histogram construction: how much is enough?"
//		 * by Surajit Chaudhuri, Rajeev Motwani and Vivek Narasayya, in
//		 * Proceedings of ACM SIGMOD International Conference on Management
//		 * of Data, 1998, Pages 436-447.  Their Corollary 1 to Theorem 5
//		 * says that for table size n, histogram size k, maximum relative
//		 * error in bin size f, and error probability gamma, the minimum
//		 * random sample size is
//		 *		r = 4 * k * ln(2*n/gamma) / f^2
//		 * Taking f = 0.5, gamma = 0.01, n = 10^6 rows, we obtain
//		 *		r = 305.82 * k
//		 * Note that because of the log function, the dependence on n is
//		 * quite weak; even at n = 10^12, a 300*k sample gives <= 0.66
//		 * bin size error with probability 0.99.  So there's no real need to
//		 * scale for n, which is a good thing because we don't necessarily
//		 * know it at this point.
//		 *--------------------
//		 */
//		stats->minrows = 300 * attr->attstattarget;
//	}
//	else
//	{
//		/* Can't do much but the minimal stuff */
//		stats->compute_stats = compute_minimal_stats;
//		/* Might as well use the same minrows as above */
//		stats->minrows = 300 * attr->attstattarget;
//	}
//
//	return true;
//}
//
//
//static void compute_scalar_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows)
//{
//
//}
//
//static void compute_minimal_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows)
//{
//
//}
//
//
//static void
//NoDB_compute_minimal_stats(int whichRelation, int whichAttribute, VacAttrStatsP stats,
//		 int *rows,
//		 int samplerows,
//		 double totalrows)
//{
//	int			i;
//	int			null_cnt = 0;
//	int			nonnull_cnt = 0;
//	int			toowide_cnt = 0;
//	double		total_width = 0;
//	bool		is_varlena = (!stats->attrtype->typbyval &&
//							  stats->attrtype->typlen == -1);
//	bool		is_varwidth = (!stats->attrtype->typbyval &&
//							   stats->attrtype->typlen < 0);
//	FmgrInfo	f_cmpeq;
//	TrackItem  *track;
//	int			track_cnt,
//				track_max;
//	int			num_mcv = stats->attr->attstattarget;
//	StdAnalyzeData *mystats = (StdAnalyzeData *) stats->extra_data;
//
//	/*
//	 * We track up to 2*n values for an n-element MCV list; but at least 10
//	 */
//	track_max = 2 * num_mcv;
//	if (track_max < 10)
//		track_max = 10;
//	track = (TrackItem *) palloc(track_max * sizeof(TrackItem));
//	track_cnt = 0;
//
//	fmgr_info(mystats->eqfunc, &f_cmpeq);
//
//
//
//	/*if the cache is enabled then retrieve the value from the cache*/
//	if ( enable_caching ) {
//		getCacheDatumSample2(whichRelation, whichAttribute, rows, samplerows, track);
//	}
//	else
//	{
//		/* If the cache is disabled then use the temp cache ;-) with the store data */
//
//	}
//
//
//	/* Initial scan to find sortable values */
//	for (i = 0; i < samplerows; i++)
//	{
//		Datum		value;
////		bool		isnull;
//		bool		match;
//		int			firstcount1,
//					j;
//
////		vacuum_delay_point();
//
//		/*Replace with access from the cache or the temp elements*/
////		value = fetchfunc(stats, i, &isnull);
//
//		/* Check for null/nonnull */
////		if (isnull)
////		{
////			null_cnt++;
////			continue;
////		}
//		nonnull_cnt++;
//		value = track[i].value;
//
//		/*
//		 * If it's a variable-width field, add up widths for average width
//		 * calculation.  Note that if the value is toasted, we use the toasted
//		 * width.  We don't bother with this calculation if it's a fixed-width
//		 * type.
//		 */
//		if (is_varlena)
//		{
//			total_width += VARSIZE_ANY(DatumGetPointer(value));
//
//			/*
//			 * If the value is toasted, we want to detoast it just once to
//			 * avoid repeated detoastings and resultant excess memory usage
//			 * during the comparisons.	Also, check to see if the value is
//			 * excessively wide, and if so don't detoast at all --- just
//			 * ignore the value.
//			 */
//			if (toast_raw_datum_size(value) > WIDTH_THRESHOLD)
//			{
//				toowide_cnt++;
//				continue;
//			}
//			value = PointerGetDatum(PG_DETOAST_DATUM(value));
//		}
//		else if (is_varwidth)
//		{
//			/* must be cstring */
//			total_width += strlen(DatumGetCString(value)) + 1;
//		}
//
//		/*
//		 * See if the value matches anything we're already tracking.
//		 */
//		match = false;
//		firstcount1 = track_cnt;
//		for (j = 0; j < track_cnt; j++)
//		{
//			if (DatumGetBool(FunctionCall2(&f_cmpeq, value, track[j].value)))
//			{
//				match = true;
//				break;
//			}
//			if (j < firstcount1 && track[j].count == 1)
//				firstcount1 = j;
//		}
//
//		if (match)
//		{
//			/* Found a match */
//			track[j].count++;
//			/* This value may now need to "bubble up" in the track list */
//			while (j > 0 && track[j].count > track[j - 1].count)
//			{
//				swapDatum(track[j].value, track[j - 1].value);
//				swapInt(track[j].count, track[j - 1].count);
//				j--;
//			}
//		}
//		else
//		{
//			/* No match.  Insert at head of count-1 list */
//			if (track_cnt < track_max)
//				track_cnt++;
//			for (j = track_cnt - 1; j > firstcount1; j--)
//			{
//				track[j].value = track[j - 1].value;
//				track[j].count = track[j - 1].count;
//			}
//			if (firstcount1 < track_cnt)
//			{
//				track[firstcount1].value = value;
//				track[firstcount1].count = 1;
//			}
//		}
//	}
//
//	/* We can only compute real stats if we found some non-null values. */
//	if (nonnull_cnt > 0)
//	{
//		int			nmultiple,
//					summultiple;
//
//		stats->stats_valid = true;
//		/* Do the simple null-frac and width stats */
//		stats->stanullfrac = (double) null_cnt / (double) samplerows;
//		if (is_varwidth)
//			stats->stawidth = total_width / (double) nonnull_cnt;
//		else
//			stats->stawidth = stats->attrtype->typlen;
//
//		/* Count the number of values we found multiple times */
//		summultiple = 0;
//		for (nmultiple = 0; nmultiple < track_cnt; nmultiple++)
//		{
//			if (track[nmultiple].count == 1)
//				break;
//			summultiple += track[nmultiple].count;
//		}
//
//		if (nmultiple == 0)
//		{
//			/* If we found no repeated values, assume it's a unique column */
//			stats->stadistinct = -1.0;
//		}
//		else if (track_cnt < track_max && toowide_cnt == 0 &&
//				 nmultiple == track_cnt)
//		{
//			/*
//			 * Our track list includes every value in the sample, and every
//			 * value appeared more than once.  Assume the column has just
//			 * these values.
//			 */
//			stats->stadistinct = track_cnt;
//		}
//		else
//		{
//			/*----------
//			 * Estimate the number of distinct values using the estimator
//			 * proposed by Haas and Stokes in IBM Research Report RJ 10025:
//			 *		n*d / (n - f1 + f1*n/N)
//			 * where f1 is the number of distinct values that occurred
//			 * exactly once in our sample of n rows (from a total of N),
//			 * and d is the total number of distinct values in the sample.
//			 * This is their Duj1 estimator; the other estimators they
//			 * recommend are considerably more complex, and are numerically
//			 * very unstable when n is much smaller than N.
//			 *
//			 * We assume (not very reliably!) that all the multiply-occurring
//			 * values are reflected in the final track[] list, and the other
//			 * nonnull values all appeared but once.  (XXX this usually
//			 * results in a drastic overestimate of ndistinct.	Can we do
//			 * any better?)
//			 *----------
//			 */
//			int			f1 = nonnull_cnt - summultiple;
//			int			d = f1 + nmultiple;
//			double		numer,
//						denom,
//						stadistinct;
//
//			numer = (double) samplerows *(double) d;
//
//			denom = (double) (samplerows - f1) +
//				(double) f1 *(double) samplerows / totalrows;
//
//			stadistinct = numer / denom;
//			/* Clamp to sane range in case of roundoff error */
//			if (stadistinct < (double) d)
//				stadistinct = (double) d;
//			if (stadistinct > totalrows)
//				stadistinct = totalrows;
//			stats->stadistinct = floor(stadistinct + 0.5);
//		}
//
//		/*
//		 * If we estimated the number of distinct values at more than 10% of
//		 * the total row count (a very arbitrary limit), then assume that
//		 * stadistinct should scale with the row count rather than be a fixed
//		 * value.
//		 */
//		if (stats->stadistinct > 0.1 * totalrows)
//			stats->stadistinct = -(stats->stadistinct / totalrows);
//
//		/*
//		 * Decide how many values are worth storing as most-common values. If
//		 * we are able to generate a complete MCV list (all the values in the
//		 * sample will fit, and we think these are all the ones in the table),
//		 * then do so.	Otherwise, store only those values that are
//		 * significantly more common than the (estimated) average. We set the
//		 * threshold rather arbitrarily at 25% more than average, with at
//		 * least 2 instances in the sample.
//		 */
//		if (track_cnt < track_max && toowide_cnt == 0 &&
//			stats->stadistinct > 0 &&
//			track_cnt <= num_mcv)
//		{
//			/* Track list includes all values seen, and all will fit */
//			num_mcv = track_cnt;
//		}
//		else
//		{
//			double		ndistinct = stats->stadistinct;
//			double		avgcount,
//						mincount;
//
//			if (ndistinct < 0)
//				ndistinct = -ndistinct * totalrows;
//			/* estimate # of occurrences in sample of a typical value */
//			avgcount = (double) samplerows / ndistinct;
//			/* set minimum threshold count to store a value */
//			mincount = avgcount * 1.25;
//			if (mincount < 2)
//				mincount = 2;
//			if (num_mcv > track_cnt)
//				num_mcv = track_cnt;
//			for (i = 0; i < num_mcv; i++)
//			{
//				if (track[i].count < mincount)
//				{
//					num_mcv = i;
//					break;
//				}
//			}
//		}
//
//		/* Generate MCV slot entry */
//		if (num_mcv > 0)
//		{
//			MemoryContext old_context;
//			Datum	   *mcv_values;
//			float4	   *mcv_freqs;
//
//			/* Must copy the target values into anl_context */
//			old_context = MemoryContextSwitchTo(stats->anl_context);
//			mcv_values = (Datum *) palloc(num_mcv * sizeof(Datum));
//			mcv_freqs = (float4 *) palloc(num_mcv * sizeof(float4));
//			for (i = 0; i < num_mcv; i++)
//			{
//				mcv_values[i] = datumCopy(track[i].value,
//										  stats->attrtype->typbyval,
//										  stats->attrtype->typlen);
//				mcv_freqs[i] = (double) track[i].count / (double) samplerows;
//			}
//			MemoryContextSwitchTo(old_context);
//
//			stats->stakind[0] = STATISTIC_KIND_MCV;
//			stats->staop[0] = mystats->eqopr;
//			stats->stanumbers[0] = mcv_freqs;
//			stats->numnumbers[0] = num_mcv;
//			stats->stavalues[0] = mcv_values;
//			stats->numvalues[0] = num_mcv;
//
//			/*
//			 * Accept the defaults for stats->statypid and others. They have
//			 * been set before we were called (see vacuum.h)
//			 */
//		}
//	}
//	else if (null_cnt > 0)
//	{
//		/* We found only nulls; assume the column is entirely null */
//		stats->stats_valid = true;
//		stats->stanullfrac = 1.0;
//		if (is_varwidth)
//			stats->stawidth = 0;	/* "unknown" */
//		else
//			stats->stawidth = stats->attrtype->typlen;
//		stats->stadistinct = 0.0;		/* "unknown" */
//	}
//
//	/* We don't need to bother cleaning up any of our temporary palloc's */
//}
//
//
//static void
//NoDB_compute_scalar_stats(int whichRelation, int whichAttribute, VacAttrStatsP stats,
//					 int *rows,
//					 int samplerows,
//					 double totalrows)
//{
//	int			i;
//	int			null_cnt = 0;
//	int			nonnull_cnt = 0;
//	int			toowide_cnt = 0;
//	double		total_width = 0;
//	bool		is_varlena = (!stats->attrtype->typbyval &&
//							  stats->attrtype->typlen == -1);
//	bool		is_varwidth = (!stats->attrtype->typbyval &&
//							   stats->attrtype->typlen < 0);
//	double		corr_xysum;
//	Oid			cmpFn;
//	int			cmpFlags;
//	FmgrInfo	f_cmpfn;
//	ScalarItem *values;
//	int			values_cnt = 0;
//	int		   *tupnoLink;
//	ScalarMCVItem *track;
//	int			track_cnt = 0;
//	int			num_mcv = stats->attr->attstattarget;
//	int			num_bins = stats->attr->attstattarget;
//	StdAnalyzeData *mystats = (StdAnalyzeData *) stats->extra_data;
//
//	values = (ScalarItem *) palloc(samplerows * sizeof(ScalarItem));
//	tupnoLink = (int *) palloc(samplerows * sizeof(int));
//	track = (ScalarMCVItem *) palloc(num_mcv * sizeof(ScalarMCVItem));
//
//	SelectSortFunction(mystats->ltopr, false, &cmpFn, &cmpFlags);
//	fmgr_info(cmpFn, &f_cmpfn);
//
//
//	/*if the cache is enabled then retrieve the value from the cache*/
//	if ( enable_caching ) {
//		getCacheDatumSample(whichRelation, whichAttribute, rows, samplerows, values);
//	}
//	else
//	{
//		/* If the cache is disabled then use the temp cache ;-) with the store data */
//
//	}
//
//
//	/* Initial scan to find sortable values */
//	for (i = 0; i < samplerows; i++)
//	{
//		Datum		value;
//		//no NULL values in this step;
////		bool		isnull;
//
//		/* Check for null/nonnull */
////		if (isnull)
////		{
////			null_cnt++;
////			continue;
////		}
//		nonnull_cnt++;
//		value = values[i].value;
//		/*
//		 * If it's a variable-width field, add up widths for average width
//		 * calculation.  Note that if the value is toasted, we use the toasted
//		 * width.  We don't bother with this calculation if it's a fixed-width
//		 * type.
//		 */
//		if (is_varlena)
//		{
//			total_width += VARSIZE_ANY(DatumGetPointer(value));
//			//noDB specific ;-)
//			total_width -= 3;
//			/*
//			 * If the value is toasted, we want to detoast it just once to
//			 * avoid repeated detoastings and resultant excess memory usage
//			 * during the comparisons.	Also, check to see if the value is
//			 * excessively wide, and if so don't detoast at all --- just
//			 * ignore the value.
//			 */
//			if (toast_raw_datum_size(value) > WIDTH_THRESHOLD)
//			{
//				toowide_cnt++;
//				continue;
//			}
//			value = PointerGetDatum(PG_DETOAST_DATUM(value));
//		}
//		else if (is_varwidth)
//		{
//			/* must be cstring */
//			total_width += strlen(DatumGetCString(value)) + 1;
//		}
//
//		/* Add it to the list to be sorted */
//		values[values_cnt].value = value;
//		values[values_cnt].tupno = values_cnt;
//		tupnoLink[values_cnt] = values_cnt;
//		values_cnt++;
//	}
//
//	/* We can only compute real stats if we found some sortable values. */
//	if (values_cnt > 0)
//	{
//		int			ndistinct,	/* # distinct values in sample */
//					nmultiple,	/* # that appear multiple times */
//					num_hist,
//					dups_cnt;
//		int			slot_idx = 0;
//		CompareScalarsContext cxt;
//
//		/* Sort the collected values */
//		cxt.cmpFn = &f_cmpfn;
//		cxt.cmpFlags = cmpFlags;
//		cxt.tupnoLink = tupnoLink;
//		qsort_arg((void *) values, values_cnt, sizeof(ScalarItem),
//				  NoDB_compare_scalars, (void *) &cxt);
//
//		/*
//		 * Now scan the values in order, find the most common ones, and also
//		 * accumulate ordering-correlation statistics.
//		 *
//		 * To determine which are most common, we first have to count the
//		 * number of duplicates of each value.	The duplicates are adjacent in
//		 * the sorted list, so a brute-force approach is to compare successive
//		 * datum values until we find two that are not equal. However, that
//		 * requires N-1 invocations of the datum comparison routine, which are
//		 * completely redundant with work that was done during the sort.  (The
//		 * sort algorithm must at some point have compared each pair of items
//		 * that are adjacent in the sorted order; otherwise it could not know
//		 * that it's ordered the pair correctly.) We exploit this by having
//		 * compare_scalars remember the highest tupno index that each
//		 * ScalarItem has been found equal to.	At the end of the sort, a
//		 * ScalarItem's tupnoLink will still point to itself if and only if it
//		 * is the last item of its group of duplicates (since the group will
//		 * be ordered by tupno).
//		 */
//		corr_xysum = 0;
//		ndistinct = 0;
//		nmultiple = 0;
//		dups_cnt = 0;
//		for (i = 0; i < values_cnt; i++)
//		{
//			int			tupno = values[i].tupno;
//
//			corr_xysum += ((double) i) * ((double) tupno);
//			dups_cnt++;
//			if (tupnoLink[tupno] == tupno)
//			{
//				/* Reached end of duplicates of this value */
//				ndistinct++;
//				if (dups_cnt > 1)
//				{
//					nmultiple++;
//					if (track_cnt < num_mcv ||
//						dups_cnt > track[track_cnt - 1].count)
//					{
//						/*
//						 * Found a new item for the mcv list; find its
//						 * position, bubbling down old items if needed. Loop
//						 * invariant is that j points at an empty/ replaceable
//						 * slot.
//						 */
//						int			j;
//
//						if (track_cnt < num_mcv)
//							track_cnt++;
//						for (j = track_cnt - 1; j > 0; j--)
//						{
//							if (dups_cnt <= track[j - 1].count)
//								break;
//							track[j].count = track[j - 1].count;
//							track[j].first = track[j - 1].first;
//						}
//						track[j].count = dups_cnt;
//						track[j].first = i + 1 - dups_cnt;
//					}
//				}
//				dups_cnt = 0;
//			}
//		}
//
//		stats->stats_valid = true;
//		/* Do the simple null-frac and width stats */
//		stats->stanullfrac = (double) null_cnt / (double) samplerows;
//		if (is_varwidth)
//			stats->stawidth = total_width / (double) nonnull_cnt;
//		else
//			stats->stawidth = stats->attrtype->typlen;
//
//		if (nmultiple == 0)
//		{
//			/* If we found no repeated values, assume it's a unique column */
//			stats->stadistinct = -1.0;
//		}
//		else if (toowide_cnt == 0 && nmultiple == ndistinct)
//		{
//			/*
//			 * Every value in the sample appeared more than once.  Assume the
//			 * column has just these values.
//			 */
//			stats->stadistinct = ndistinct;
//		}
//		else
//		{
//			/*----------
//			 * Estimate the number of distinct values using the estimator
//			 * proposed by Haas and Stokes in IBM Research Report RJ 10025:
//			 *		n*d / (n - f1 + f1*n/N)
//			 * where f1 is the number of distinct values that occurred
//			 * exactly once in our sample of n rows (from a total of N),
//			 * and d is the total number of distinct values in the sample.
//			 * This is their Duj1 estimator; the other estimators they
//			 * recommend are considerably more complex, and are numerically
//			 * very unstable when n is much smaller than N.
//			 *
//			 * Overwidth values are assumed to have been distinct.
//			 *----------
//			 */
//			int			f1 = ndistinct - nmultiple + toowide_cnt;
//			int			d = f1 + nmultiple;
//			double		numer,
//						denom,
//						stadistinct;
//
//			numer = (double) samplerows *(double) d;
//
//			denom = (double) (samplerows - f1) +
//				(double) f1 *(double) samplerows / totalrows;
//
//			stadistinct = numer / denom;
//			/* Clamp to sane range in case of roundoff error */
//			if (stadistinct < (double) d)
//				stadistinct = (double) d;
//			if (stadistinct > totalrows)
//				stadistinct = totalrows;
//			stats->stadistinct = floor(stadistinct + 0.5);
//		}
//
//		/*
//		 * If we estimated the number of distinct values at more than 10% of
//		 * the total row count (a very arbitrary limit), then assume that
//		 * stadistinct should scale with the row count rather than be a fixed
//		 * value.
//		 */
//		if (stats->stadistinct > 0.1 * totalrows)
//			stats->stadistinct = -(stats->stadistinct / totalrows);
//
//		/*
//		 * Decide how many values are worth storing as most-common values. If
//		 * we are able to generate a complete MCV list (all the values in the
//		 * sample will fit, and we think these are all the ones in the table),
//		 * then do so.	Otherwise, store only those values that are
//		 * significantly more common than the (estimated) average. We set the
//		 * threshold rather arbitrarily at 25% more than average, with at
//		 * least 2 instances in the sample.  Also, we won't suppress values
//		 * that have a frequency of at least 1/K where K is the intended
//		 * number of histogram bins; such values might otherwise cause us to
//		 * emit duplicate histogram bin boundaries.  (We might end up with
//		 * duplicate histogram entries anyway, if the distribution is skewed;
//		 * but we prefer to treat such values as MCVs if at all possible.)
//		 */
//		if (track_cnt == ndistinct && toowide_cnt == 0 &&
//			stats->stadistinct > 0 &&
//			track_cnt <= num_mcv)
//		{
//			/* Track list includes all values seen, and all will fit */
//			num_mcv = track_cnt;
//		}
//		else
//		{
//			double		ndistinct = stats->stadistinct;
//			double		avgcount,
//						mincount,
//						maxmincount;
//
//			if (ndistinct < 0)
//				ndistinct = -ndistinct * totalrows;
//			/* estimate # of occurrences in sample of a typical value */
//			avgcount = (double) samplerows / ndistinct;
//			/* set minimum threshold count to store a value */
//			mincount = avgcount * 1.25;
//			if (mincount < 2)
//				mincount = 2;
//			/* don't let threshold exceed 1/K, however */
//			maxmincount = (double) samplerows / (double) num_bins;
//			if (mincount > maxmincount)
//				mincount = maxmincount;
//			if (num_mcv > track_cnt)
//				num_mcv = track_cnt;
//			for (i = 0; i < num_mcv; i++)
//			{
//				if (track[i].count < mincount)
//				{
//					num_mcv = i;
//					break;
//				}
//			}
//		}
//
//		/* Generate MCV slot entry */
//		if (num_mcv > 0)
//		{
//			MemoryContext old_context;
//			Datum	   *mcv_values;
//			float4	   *mcv_freqs;
//
//			/* Must copy the target values into anl_context */
//			old_context = MemoryContextSwitchTo(stats->anl_context);
//			mcv_values = (Datum *) palloc(num_mcv * sizeof(Datum));
//			mcv_freqs = (float4 *) palloc(num_mcv * sizeof(float4));
//			for (i = 0; i < num_mcv; i++)
//			{
//				mcv_values[i] = datumCopy(values[track[i].first].value,
//										  stats->attrtype->typbyval,
//										  stats->attrtype->typlen);
//				mcv_freqs[i] = (double) track[i].count / (double) samplerows;
//			}
//			MemoryContextSwitchTo(old_context);
//
//			stats->stakind[slot_idx] = STATISTIC_KIND_MCV;
//			stats->staop[slot_idx] = mystats->eqopr;
//			stats->stanumbers[slot_idx] = mcv_freqs;
//			stats->numnumbers[slot_idx] = num_mcv;
//			stats->stavalues[slot_idx] = mcv_values;
//			stats->numvalues[slot_idx] = num_mcv;
//
//			/*
//			 * Accept the defaults for stats->statypid and others. They have
//			 * been set before we were called (see vacuum.h)
//			 */
//			slot_idx++;
//		}
//
//		/*
//		 * Generate a histogram slot entry if there are at least two distinct
//		 * values not accounted for in the MCV list.  (This ensures the
//		 * histogram won't collapse to empty or a singleton.)
//		 */
//		num_hist = ndistinct - num_mcv;
//		if (num_hist > num_bins)
//			num_hist = num_bins + 1;
//		if (num_hist >= 2)
//		{
//			MemoryContext old_context;
//			Datum	   *hist_values;
//			int			nvals;
//			int			pos,
//						posfrac,
//						delta,
//						deltafrac;
//
//			/* Sort the MCV items into position order to speed next loop */
//			qsort((void *) track, num_mcv,
//				  sizeof(ScalarMCVItem), NoDB_compare_mcvs);
//
//			/*
//			 * Collapse out the MCV items from the values[] array.
//			 *
//			 * Note we destroy the values[] array here... but we don't need it
//			 * for anything more.  We do, however, still need values_cnt.
//			 * nvals will be the number of remaining entries in values[].
//			 */
//			if (num_mcv > 0)
//			{
//				int			src,
//							dest;
//				int			j;
//
//				src = dest = 0;
//				j = 0;			/* index of next interesting MCV item */
//				while (src < values_cnt)
//				{
//					int			ncopy;
//
//					if (j < num_mcv)
//					{
//						int			first = track[j].first;
//
//						if (src >= first)
//						{
//							/* advance past this MCV item */
//							src = first + track[j].count;
//							j++;
//							continue;
//						}
//						ncopy = first - src;
//					}
//					else
//						ncopy = values_cnt - src;
//					memmove(&values[dest], &values[src],
//							ncopy * sizeof(ScalarItem));
//					src += ncopy;
//					dest += ncopy;
//				}
//				nvals = dest;
//			}
//			else
//				nvals = values_cnt;
//			Assert(nvals >= num_hist);
//
//			/* Must copy the target values into anl_context */
//			old_context = MemoryContextSwitchTo(stats->anl_context);
//			hist_values = (Datum *) palloc(num_hist * sizeof(Datum));
//
//			/*
//			 * The object of this loop is to copy the first and last values[]
//			 * entries along with evenly-spaced values in between.	So the
//			 * i'th value is values[(i * (nvals - 1)) / (num_hist - 1)].  But
//			 * computing that subscript directly risks integer overflow when
//			 * the stats target is more than a couple thousand.  Instead we
//			 * add (nvals - 1) / (num_hist - 1) to pos at each step, tracking
//			 * the integral and fractional parts of the sum separately.
//			 */
//			delta = (nvals - 1) / (num_hist - 1);
//			deltafrac = (nvals - 1) % (num_hist - 1);
//			pos = posfrac = 0;
//
//			for (i = 0; i < num_hist; i++)
//			{
//				hist_values[i] = datumCopy(values[pos].value,
//										   stats->attrtype->typbyval,
//										   stats->attrtype->typlen);
//				pos += delta;
//				posfrac += deltafrac;
//				if (posfrac >= (num_hist - 1))
//				{
//					/* fractional part exceeds 1, carry to integer part */
//					pos++;
//					posfrac -= (num_hist - 1);
//				}
//			}
//
//			MemoryContextSwitchTo(old_context);
//
//			stats->stakind[slot_idx] = STATISTIC_KIND_HISTOGRAM;
//			stats->staop[slot_idx] = mystats->ltopr;
//			stats->stavalues[slot_idx] = hist_values;
//			stats->numvalues[slot_idx] = num_hist;
//
//			/*
//			 * Accept the defaults for stats->statypid and others. They have
//			 * been set before we were called (see vacuum.h)
//			 */
//			slot_idx++;
//		}
//
//		/* Generate a correlation entry if there are multiple values */
//		if (values_cnt > 1)
//		{
//			MemoryContext old_context;
//			float4	   *corrs;
//			double		corr_xsum,
//						corr_x2sum;
//
//			/* Must copy the target values into anl_context */
//			old_context = MemoryContextSwitchTo(stats->anl_context);
//			corrs = (float4 *) palloc(sizeof(float4));
//			MemoryContextSwitchTo(old_context);
//
//			/*----------
//			 * Since we know the x and y value sets are both
//			 *		0, 1, ..., values_cnt-1
//			 * we have sum(x) = sum(y) =
//			 *		(values_cnt-1)*values_cnt / 2
//			 * and sum(x^2) = sum(y^2) =
//			 *		(values_cnt-1)*values_cnt*(2*values_cnt-1) / 6.
//			 *----------
//			 */
//			corr_xsum = ((double) (values_cnt - 1)) *
//				((double) values_cnt) / 2.0;
//			corr_x2sum = ((double) (values_cnt - 1)) *
//				((double) values_cnt) * (double) (2 * values_cnt - 1) / 6.0;
//
//			/* And the correlation coefficient reduces to */
//			corrs[0] = (values_cnt * corr_xysum - corr_xsum * corr_xsum) /
//				(values_cnt * corr_x2sum - corr_xsum * corr_xsum);
//
//			stats->stakind[slot_idx] = STATISTIC_KIND_CORRELATION;
//			stats->staop[slot_idx] = mystats->ltopr;
//			stats->stanumbers[slot_idx] = corrs;
//			stats->numnumbers[slot_idx] = 1;
//			slot_idx++;
//		}
//	}
//	else if (nonnull_cnt == 0 && null_cnt > 0)
//	{
//		/* We found only nulls; assume the column is entirely null */
//		stats->stats_valid = true;
//		stats->stanullfrac = 1.0;
//		if (is_varwidth)
//			stats->stawidth = 0;	/* "unknown" */
//		else
//			stats->stawidth = stats->attrtype->typlen;
//		stats->stadistinct = 0.0;		/* "unknown" */
//	}
//
//	/* We don't need to bother cleaning up any of our temporary palloc's */
//}
//
//
//static int
//NoDB_compare_scalars(const void *a, const void *b, void *arg)
//{
//	Datum		da = ((ScalarItem *) a)->value;
//	int			ta = ((ScalarItem *) a)->tupno;
//	Datum		db = ((ScalarItem *) b)->value;
//	int			tb = ((ScalarItem *) b)->tupno;
//	CompareScalarsContext *cxt = (CompareScalarsContext *) arg;
//	int32		compare;
//
//	compare = ApplySortFunction(cxt->cmpFn, cxt->cmpFlags,
//								da, false, db, false);
//	if (compare != 0)
//		return compare;
//
//	/*
//	 * The two datums are equal, so update cxt->tupnoLink[].
//	 */
//	if (cxt->tupnoLink[ta] < tb)
//		cxt->tupnoLink[ta] = tb;
//	if (cxt->tupnoLink[tb] < ta)
//		cxt->tupnoLink[tb] = ta;
//
//	/*
//	 * For equal datums, sort by tupno
//	 */
//	return ta - tb;
//}
//
///*
// * qsort comparator for sorting ScalarMCVItems by position
// */
//static int
//NoDB_compare_mcvs(const void *a, const void *b)
//{
//	int			da = ((ScalarMCVItem *) a)->first;
//	int			db = ((ScalarMCVItem *) b)->first;
//
//	return da - db;
//}
//
//
//
///* Get the size sample and return how many positions should be sampled and which positions*/
//static int
//NoDB_acquire_sample_rows(int *rows, int targrows, long totalrows)
//{
//	int i;
//
//	int			numrows = 0;	/* # rows now in reservoir */
//
//	Assert(targrows > 0);
//
//	/*We don't need sampling, just take everything*/
//	if (totalrows < targrows)
//	{
//		numrows = totalrows;
//		for (i = 0; i < totalrows; i++ )
//			rows[i] = i;
//	}
//	else
//	{
//		numrows = 0;
//
//        for (i = 0; i < totalrows && numrows < targrows; ++i)
//        {
//        	int temp_n = totalrows - i;
//        	int temp_m = targrows - numrows;
//        	if (random() % temp_n < temp_m)
//        		rows[numrows++] = i + 1;
//        }
//	}
//	return numrows;
//}
//
//
//
//static void
//update_attstats(Oid relid, bool inh, int natts, VacAttrStats **vacattrstats)
//{
//	Relation	sd;
//	int			attno;
//
//	if (natts <= 0)
//		return;					/* nothing to do */
//
//	sd = heap_open(StatisticRelationId, RowExclusiveLock);
//
//	for (attno = 0; attno < natts; attno++)
//	{
//		VacAttrStats *stats = vacattrstats[attno];
//		HeapTuple	stup,
//					oldtup;
//		int			i,
//					k,
//					n;
//		Datum		values[Natts_pg_statistic];
//		bool		nulls[Natts_pg_statistic];
//		bool		replaces[Natts_pg_statistic];
//
//		/* Ignore attr if we weren't able to collect stats */
//		if (!stats->stats_valid)
//			continue;
//
//		/*
//		 * Construct a new pg_statistic tuple
//		 */
//		for (i = 0; i < Natts_pg_statistic; ++i)
//		{
//			nulls[i] = false;
//			replaces[i] = true;
//		}
//
//		i = 0;
//		values[i++] = ObjectIdGetDatum(relid);	/* starelid */
//		values[i++] = Int16GetDatum(stats->attr->attnum);		/* staattnum */
//		values[i++] = BoolGetDatum(inh);		/* stainherit */
//		values[i++] = Float4GetDatum(stats->stanullfrac);		/* stanullfrac */
//		values[i++] = Int32GetDatum(stats->stawidth);	/* stawidth */
//		values[i++] = Float4GetDatum(stats->stadistinct);		/* stadistinct */
//		for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
//		{
//			values[i++] = Int16GetDatum(stats->stakind[k]);		/* stakindN */
//		}
//		for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
//		{
//			values[i++] = ObjectIdGetDatum(stats->staop[k]);	/* staopN */
//		}
//		for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
//		{
//			int			nnum = stats->numnumbers[k];
//
//			if (nnum > 0)
//			{
//				Datum	   *numdatums = (Datum *) palloc(nnum * sizeof(Datum));
//				ArrayType  *arry;
//
//				for (n = 0; n < nnum; n++)
//					numdatums[n] = Float4GetDatum(stats->stanumbers[k][n]);
//				/* XXX knows more than it should about type float4: */
//				arry = construct_array(numdatums, nnum,
//									   FLOAT4OID,
//									   sizeof(float4), FLOAT4PASSBYVAL, 'i');
//				values[i++] = PointerGetDatum(arry);	/* stanumbersN */
//			}
//			else
//			{
//				nulls[i] = true;
//				values[i++] = (Datum) 0;
//			}
//		}
//		for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
//		{
//			if (stats->numvalues[k] > 0)
//			{
//				ArrayType  *arry;
//
//				arry = construct_array(stats->stavalues[k],
//									   stats->numvalues[k],
//									   stats->statypid[k],
//									   stats->statyplen[k],
//									   stats->statypbyval[k],
//									   stats->statypalign[k]);
//				values[i++] = PointerGetDatum(arry);	/* stavaluesN */
//			}
//			else
//			{
//				nulls[i] = true;
//				values[i++] = (Datum) 0;
//			}
//		}
//
//		/* Is there already a pg_statistic tuple for this attribute? */
//		oldtup = SearchSysCache3(STATRELATTINH,
//								 ObjectIdGetDatum(relid),
//								 Int16GetDatum(stats->attr->attnum),
//								 BoolGetDatum(inh));
//
//		if (HeapTupleIsValid(oldtup))
//		{
//			/* Yes, replace it */
//			stup = heap_modify_tuple(oldtup,
//									 RelationGetDescr(sd),
//									 values,
//									 nulls,
//									 replaces);
//			ReleaseSysCache(oldtup);
//			simple_heap_update(sd, &stup->t_self, stup);
//		}
//		else
//		{
//			/* No, insert new tuple */
//			stup = heap_form_tuple(RelationGetDescr(sd), values, nulls);
//			simple_heap_insert(sd, stup);
//		}
//
//		/* update indexes too */
//		CatalogUpdateIndexes(sd, stup);
//
//		heap_freetuple(stup);
//	}
//
//	heap_close(sd, RowExclusiveLock);
//}
//
//
//
//
//
//
//
