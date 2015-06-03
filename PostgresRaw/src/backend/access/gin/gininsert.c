/*-------------------------------------------------------------------------
 *
 * gininsert.c
 *	  insert routines for the postgres inverted index access method.
 *
 *
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			$PostgreSQL: pgsql/src/backend/access/gin/gininsert.c,v 1.26.6.1 2010/08/01 02:12:51 tgl Exp $
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/gin.h"
#include "catalog/index.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/indexfsm.h"
#include "utils/memutils.h"


typedef struct
{
	GinState	ginstate;
	double		indtuples;
	MemoryContext tmpCtx;
	MemoryContext funcCtx;
	BuildAccumulator accum;
} GinBuildState;

/*
 * Creates posting tree with one page. Function
 * suppose that items[] fits to page
 */
static BlockNumber
createPostingTree(Relation index, ItemPointerData *items, uint32 nitems)
{
	BlockNumber blkno;
	Buffer		buffer = GinNewBuffer(index);
	Page		page;

	START_CRIT_SECTION();

	GinInitBuffer(buffer, GIN_DATA | GIN_LEAF);
	page = BufferGetPage(buffer);
	blkno = BufferGetBlockNumber(buffer);

	memcpy(GinDataPageGetData(page), items, sizeof(ItemPointerData) * nitems);
	GinPageGetOpaque(page)->maxoff = nitems;

	MarkBufferDirty(buffer);

	if (!index->rd_istemp)
	{
		XLogRecPtr	recptr;
		XLogRecData rdata[2];
		ginxlogCreatePostingTree data;

		data.node = index->rd_node;
		data.blkno = blkno;
		data.nitem = nitems;

		rdata[0].buffer = InvalidBuffer;
		rdata[0].data = (char *) &data;
		rdata[0].len = sizeof(ginxlogCreatePostingTree);
		rdata[0].next = &rdata[1];

		rdata[1].buffer = InvalidBuffer;
		rdata[1].data = (char *) items;
		rdata[1].len = sizeof(ItemPointerData) * nitems;
		rdata[1].next = NULL;



		recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_CREATE_PTREE, rdata);
		PageSetLSN(page, recptr);
		PageSetTLI(page, ThisTimeLineID);

	}

	UnlockReleaseBuffer(buffer);

	END_CRIT_SECTION();

	return blkno;
}


/*
 * Adds array of item pointers to tuple's posting list or
 * creates posting tree and tuple pointed to tree in a case
 * of not enough space.  Max size of tuple is defined in
 * GinFormTuple().
 */
static IndexTuple
addItemPointersToTuple(Relation index, GinState *ginstate, GinBtreeStack *stack,
		  IndexTuple old, ItemPointerData *items, uint32 nitem, bool isBuild)
{
	Datum		key = gin_index_getattr(ginstate, old);
	OffsetNumber attnum = gintuple_get_attrnum(ginstate, old);
	IndexTuple	res = GinFormTuple(index, ginstate, attnum, key,
								   NULL, nitem + GinGetNPosting(old),
								   false);

	if (res)
	{
		/* good, small enough */
		uint32		newnitem;

		newnitem = MergeItemPointers(GinGetPosting(res),
									 GinGetPosting(old), GinGetNPosting(old),
									 items, nitem);
		/* merge might have eliminated some duplicate items */
		GinShortenTuple(res, newnitem);
	}
	else
	{
		BlockNumber postingRoot;
		GinPostingTreeScan *gdi;

		/* posting list becomes big, so we need to make posting's tree */
		res = GinFormTuple(index, ginstate, attnum, key, NULL, 0, true);
		postingRoot = createPostingTree(index, GinGetPosting(old), GinGetNPosting(old));
		GinSetPostingTree(res, postingRoot);

		gdi = prepareScanPostingTree(index, postingRoot, FALSE);
		gdi->btree.isBuild = isBuild;

		insertItemPointer(gdi, items, nitem);

		pfree(gdi);
	}

	return res;
}

/*
 * Inserts only one entry to the index, but it can add more than 1 ItemPointer.
 */
void
ginEntryInsert(Relation index, GinState *ginstate,
			   OffsetNumber attnum, Datum value,
			   ItemPointerData *items, uint32 nitem,
			   bool isBuild)
{
	GinBtreeData btree;
	GinBtreeStack *stack;
	IndexTuple	itup;
	Page		page;

	prepareEntryScan(&btree, index, attnum, value, ginstate);

	stack = ginFindLeafPage(&btree, NULL);
	page = BufferGetPage(stack->buffer);

	if (btree.findItem(&btree, stack))
	{
		/* found entry */
		itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, stack->off));

		if (GinIsPostingTree(itup))
		{
			/* lock root of posting tree */
			GinPostingTreeScan *gdi;
			BlockNumber rootPostingTree = GinGetPostingTree(itup);

			/* release all stack */
			LockBuffer(stack->buffer, GIN_UNLOCK);
			freeGinBtreeStack(stack);

			/* insert into posting tree */
			gdi = prepareScanPostingTree(index, rootPostingTree, FALSE);
			gdi->btree.isBuild = isBuild;
			insertItemPointer(gdi, items, nitem);
			pfree(gdi);

			return;
		}

		itup = addItemPointersToTuple(index, ginstate, stack, itup, items, nitem, isBuild);

		btree.isDelete = TRUE;
	}
	else
	{
		/* We suppose that tuple can store at least one itempointer */
		itup = GinFormTuple(index, ginstate, attnum, value, items, 1, true);

		if (nitem > 1)
		{
			/* Add the rest, making a posting tree if necessary */
			IndexTuple	previtup = itup;

			itup = addItemPointersToTuple(index, ginstate, stack, previtup, items + 1, nitem - 1, isBuild);
			pfree(previtup);
		}
	}

	btree.entry = itup;
	ginInsertValue(&btree, stack);
	pfree(itup);
}

/*
 * Saves indexed value in memory accumulator during index creation
 * Function isn't used during normal insert
 */
static uint32
ginHeapTupleBulkInsert(GinBuildState *buildstate, OffsetNumber attnum, Datum value, ItemPointer heapptr)
{
	Datum	   *entries;
	int32		nentries;
	MemoryContext oldCtx;

	oldCtx = MemoryContextSwitchTo(buildstate->funcCtx);
	entries = extractEntriesSU(buildstate->accum.ginstate, attnum, value, &nentries);
	MemoryContextSwitchTo(oldCtx);

	if (nentries == 0)
		/* nothing to insert */
		return 0;

	ginInsertRecordBA(&buildstate->accum, heapptr, attnum, entries, nentries);

	MemoryContextReset(buildstate->funcCtx);

	return nentries;
}

static void
ginBuildCallback(Relation index, HeapTuple htup, Datum *values,
				 bool *isnull, bool tupleIsAlive, void *state)
{
	GinBuildState *buildstate = (GinBuildState *) state;
	MemoryContext oldCtx;
	int			i;

	oldCtx = MemoryContextSwitchTo(buildstate->tmpCtx);

	for (i = 0; i < buildstate->ginstate.origTupdesc->natts; i++)
		if (!isnull[i])
			buildstate->indtuples += ginHeapTupleBulkInsert(buildstate,
										   (OffsetNumber) (i + 1), values[i],
															&htup->t_self);

	/* If we've maxed out our available memory, dump everything to the index */
	if (buildstate->accum.allocatedMemory >= maintenance_work_mem * 1024L)
	{
		ItemPointerData *list;
		Datum		entry;
		uint32		nlist;
		OffsetNumber attnum;

		ginBeginBAScan(&buildstate->accum);
		while ((list = ginGetEntry(&buildstate->accum, &attnum, &entry, &nlist)) != NULL)
		{
			/* there could be many entries, so be willing to abort here */
			CHECK_FOR_INTERRUPTS();
			ginEntryInsert(index, &buildstate->ginstate, attnum, entry, list, nlist, TRUE);
		}

		MemoryContextReset(buildstate->tmpCtx);
		ginInitBA(&buildstate->accum);
	}

	MemoryContextSwitchTo(oldCtx);
}

Datum
ginbuild(PG_FUNCTION_ARGS)
{
	Relation	heap = (Relation) PG_GETARG_POINTER(0);
	Relation	index = (Relation) PG_GETARG_POINTER(1);
	IndexInfo  *indexInfo = (IndexInfo *) PG_GETARG_POINTER(2);
	IndexBuildResult *result;
	double		reltuples;
	GinBuildState buildstate;
	Buffer		RootBuffer,
				MetaBuffer;
	ItemPointerData *list;
	Datum		entry;
	uint32		nlist;
	MemoryContext oldCtx;
	OffsetNumber attnum;

	if (RelationGetNumberOfBlocks(index) != 0)
		elog(ERROR, "index \"%s\" already contains data",
			 RelationGetRelationName(index));

	initGinState(&buildstate.ginstate, index);

	/* initialize the meta page */
	MetaBuffer = GinNewBuffer(index);

	/* initialize the root page */
	RootBuffer = GinNewBuffer(index);

	START_CRIT_SECTION();
	GinInitMetabuffer(MetaBuffer);
	MarkBufferDirty(MetaBuffer);
	GinInitBuffer(RootBuffer, GIN_LEAF);
	MarkBufferDirty(RootBuffer);

	if (!index->rd_istemp)
	{
		XLogRecPtr	recptr;
		XLogRecData rdata;
		Page		page;

		rdata.buffer = InvalidBuffer;
		rdata.data = (char *) &(index->rd_node);
		rdata.len = sizeof(RelFileNode);
		rdata.next = NULL;

		recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_CREATE_INDEX, &rdata);

		page = BufferGetPage(RootBuffer);
		PageSetLSN(page, recptr);
		PageSetTLI(page, ThisTimeLineID);

		page = BufferGetPage(MetaBuffer);
		PageSetLSN(page, recptr);
		PageSetTLI(page, ThisTimeLineID);
	}

	UnlockReleaseBuffer(MetaBuffer);
	UnlockReleaseBuffer(RootBuffer);
	END_CRIT_SECTION();

	/* build the index */
	buildstate.indtuples = 0;

	/*
	 * create a temporary memory context that is reset once for each tuple
	 * inserted into the index
	 */
	buildstate.tmpCtx = AllocSetContextCreate(CurrentMemoryContext,
											  "Gin build temporary context",
											  ALLOCSET_DEFAULT_MINSIZE,
											  ALLOCSET_DEFAULT_INITSIZE,
											  ALLOCSET_DEFAULT_MAXSIZE);

	buildstate.funcCtx = AllocSetContextCreate(buildstate.tmpCtx,
					 "Gin build temporary context for user-defined function",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	buildstate.accum.ginstate = &buildstate.ginstate;
	ginInitBA(&buildstate.accum);

	/*
	 * Do the heap scan.  We disallow sync scan here because dataPlaceToPage
	 * prefers to receive tuples in TID order.
	 */
	reltuples = IndexBuildHeapScan(heap, index, indexInfo, false,
								   ginBuildCallback, (void *) &buildstate);

	/* dump remaining entries to the index */
	oldCtx = MemoryContextSwitchTo(buildstate.tmpCtx);
	ginBeginBAScan(&buildstate.accum);
	while ((list = ginGetEntry(&buildstate.accum, &attnum, &entry, &nlist)) != NULL)
	{
		/* there could be many entries, so be willing to abort here */
		CHECK_FOR_INTERRUPTS();
		ginEntryInsert(index, &buildstate.ginstate, attnum, entry, list, nlist, TRUE);
	}
	MemoryContextSwitchTo(oldCtx);

	MemoryContextDelete(buildstate.tmpCtx);

	/*
	 * Return statistics
	 */
	result = (IndexBuildResult *) palloc(sizeof(IndexBuildResult));

	result->heap_tuples = reltuples;
	result->index_tuples = buildstate.indtuples;

	PG_RETURN_POINTER(result);
}

/*
 * Inserts value during normal insertion
 */
static uint32
ginHeapTupleInsert(Relation index, GinState *ginstate, OffsetNumber attnum, Datum value, ItemPointer item)
{
	Datum	   *entries;
	int32		i,
				nentries;

	entries = extractEntriesSU(ginstate, attnum, value, &nentries);

	if (nentries == 0)
		/* nothing to insert */
		return 0;

	for (i = 0; i < nentries; i++)
		ginEntryInsert(index, ginstate, attnum, entries[i], item, 1, FALSE);

	return nentries;
}

Datum
gininsert(PG_FUNCTION_ARGS)
{
	Relation	index = (Relation) PG_GETARG_POINTER(0);
	Datum	   *values = (Datum *) PG_GETARG_POINTER(1);
	bool	   *isnull = (bool *) PG_GETARG_POINTER(2);
	ItemPointer ht_ctid = (ItemPointer) PG_GETARG_POINTER(3);

#ifdef NOT_USED
	Relation	heapRel = (Relation) PG_GETARG_POINTER(4);
	IndexUniqueCheck checkUnique = (IndexUniqueCheck) PG_GETARG_INT32(5);
#endif
	GinState	ginstate;
	MemoryContext oldCtx;
	MemoryContext insertCtx;
	int			i;

	insertCtx = AllocSetContextCreate(CurrentMemoryContext,
									  "Gin insert temporary context",
									  ALLOCSET_DEFAULT_MINSIZE,
									  ALLOCSET_DEFAULT_INITSIZE,
									  ALLOCSET_DEFAULT_MAXSIZE);

	oldCtx = MemoryContextSwitchTo(insertCtx);

	initGinState(&ginstate, index);

	if (GinGetUseFastUpdate(index))
	{
		GinTupleCollector collector;

		memset(&collector, 0, sizeof(GinTupleCollector));
		for (i = 0; i < ginstate.origTupdesc->natts; i++)
			if (!isnull[i])
				ginHeapTupleFastCollect(index, &ginstate, &collector,
								 (OffsetNumber) (i + 1), values[i], ht_ctid);

		ginHeapTupleFastInsert(index, &ginstate, &collector);
	}
	else
	{
		for (i = 0; i < ginstate.origTupdesc->natts; i++)
			if (!isnull[i])
				ginHeapTupleInsert(index, &ginstate,
								 (OffsetNumber) (i + 1), values[i], ht_ctid);

	}

	MemoryContextSwitchTo(oldCtx);
	MemoryContextDelete(insertCtx);

	PG_RETURN_BOOL(false);
}
