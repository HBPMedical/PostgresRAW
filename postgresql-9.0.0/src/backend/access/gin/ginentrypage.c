/*-------------------------------------------------------------------------
 *
 * ginentrypage.c
 *	  page utilities routines for the postgres inverted index access method.
 *
 *
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			$PostgreSQL: pgsql/src/backend/access/gin/ginentrypage.c,v 1.24.4.1 2010/08/01 02:12:51 tgl Exp $
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gin.h"
#include "storage/bufmgr.h"
#include "utils/rel.h"

/*
 * Form a tuple for entry tree.
 *
 * If the tuple would be too big to be stored, function throws a suitable
 * error if errorTooBig is TRUE, or returns NULL if errorTooBig is FALSE.
 *
 * On leaf pages, Index tuple has non-traditional layout. Tuple may contain
 * posting list or root blocknumber of posting tree.
 * Macros: GinIsPostingTree(itup) / GinSetPostingTree(itup, blkno)
 * 1) Posting list
 *		- itup->t_info & INDEX_SIZE_MASK contains total size of tuple as usual
 *		- ItemPointerGetBlockNumber(&itup->t_tid) contains original
 *		  size of tuple (without posting list).
 *		  Macros: GinGetOrigSizePosting(itup) / GinSetOrigSizePosting(itup,n)
 *		- ItemPointerGetOffsetNumber(&itup->t_tid) contains number
 *		  of elements in posting list (number of heap itempointers)
 *		  Macros: GinGetNPosting(itup) / GinSetNPosting(itup,n)
 *		- After standard part of tuple there is a posting list, ie, array
 *		  of heap itempointers
 *		  Macros: GinGetPosting(itup)
 * 2) Posting tree
 *		- itup->t_info & INDEX_SIZE_MASK contains size of tuple as usual
 *		- ItemPointerGetBlockNumber(&itup->t_tid) contains block number of
 *		  root of posting tree
 *		- ItemPointerGetOffsetNumber(&itup->t_tid) contains magic number
 *		  GIN_TREE_POSTING, which distinguishes this from posting-list case
 *
 * Attributes of an index tuple are different for single and multicolumn index.
 * For single-column case, index tuple stores only value to be indexed.
 * For multicolumn case, it stores two attributes: column number of value
 * and value.
 */
IndexTuple
GinFormTuple(Relation index, GinState *ginstate,
			 OffsetNumber attnum, Datum key,
			 ItemPointerData *ipd, uint32 nipd, bool errorTooBig)
{
	bool		isnull[2] = {FALSE, FALSE};
	IndexTuple	itup;
	uint32		newsize;

	if (ginstate->oneCol)
		itup = index_form_tuple(ginstate->origTupdesc, &key, isnull);
	else
	{
		Datum		datums[2];

		datums[0] = UInt16GetDatum(attnum);
		datums[1] = key;
		itup = index_form_tuple(ginstate->tupdesc[attnum - 1], datums, isnull);
	}

	GinSetOrigSizePosting(itup, IndexTupleSize(itup));

	if (nipd > 0)
	{
		newsize = MAXALIGN(SHORTALIGN(IndexTupleSize(itup)) + sizeof(ItemPointerData) * nipd);
		if (newsize > Min(INDEX_SIZE_MASK, GinMaxItemSize))
		{
			if (errorTooBig)
				ereport(ERROR,
						(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
						 errmsg("index row size %lu exceeds maximum %lu for index \"%s\"",
								(unsigned long) newsize,
								(unsigned long) Min(INDEX_SIZE_MASK,
													GinMaxItemSize),
								RelationGetRelationName(index))));
			return NULL;
		}

		itup = repalloc(itup, newsize);

		/* set new size */
		itup->t_info &= ~INDEX_SIZE_MASK;
		itup->t_info |= newsize;

		if (ipd)
			memcpy(GinGetPosting(itup), ipd, sizeof(ItemPointerData) * nipd);
		GinSetNPosting(itup, nipd);
	}
	else
	{
		/*
		 * Gin tuple without any ItemPointers should be large enough to keep
		 * one ItemPointer, to prevent inconsistency between
		 * ginHeapTupleFastCollect and ginEntryInsert called by
		 * ginHeapTupleInsert.	ginHeapTupleFastCollect forms tuple without
		 * extra pointer to heap, but ginEntryInsert (called for pending list
		 * cleanup during vacuum) will form the same tuple with one
		 * ItemPointer.
		 */
		newsize = MAXALIGN(SHORTALIGN(IndexTupleSize(itup)) + sizeof(ItemPointerData));
		if (newsize > Min(INDEX_SIZE_MASK, GinMaxItemSize))
		{
			if (errorTooBig)
				ereport(ERROR,
						(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
						 errmsg("index row size %lu exceeds maximum %lu for index \"%s\"",
								(unsigned long) newsize,
								(unsigned long) Min(INDEX_SIZE_MASK,
													GinMaxItemSize),
								RelationGetRelationName(index))));
			return NULL;
		}

		GinSetNPosting(itup, 0);
	}
	return itup;
}

/*
 * Sometimes we reduce the number of posting list items in a tuple after
 * having built it with GinFormTuple.  This function adjusts the size
 * fields to match.
 */
void
GinShortenTuple(IndexTuple itup, uint32 nipd)
{
	uint32		newsize;

	Assert(nipd <= GinGetNPosting(itup));

	newsize = MAXALIGN(SHORTALIGN(GinGetOrigSizePosting(itup)) + sizeof(ItemPointerData) * nipd);

	Assert(newsize <= (itup->t_info & INDEX_SIZE_MASK));

	itup->t_info &= ~INDEX_SIZE_MASK;
	itup->t_info |= newsize;

	GinSetNPosting(itup, nipd);
}

/*
 * Entry tree is a "static", ie tuple never deletes from it,
 * so we don't use right bound, we use rightest key instead.
 */
static IndexTuple
getRightMostTuple(Page page)
{
	OffsetNumber maxoff = PageGetMaxOffsetNumber(page);

	return (IndexTuple) PageGetItem(page, PageGetItemId(page, maxoff));
}

static bool
entryIsMoveRight(GinBtree btree, Page page)
{
	IndexTuple	itup;

	if (GinPageRightMost(page))
		return FALSE;

	itup = getRightMostTuple(page);

	if (compareAttEntries(btree->ginstate,
						  btree->entryAttnum, btree->entryValue,
						  gintuple_get_attrnum(btree->ginstate, itup),
						  gin_index_getattr(btree->ginstate, itup)) > 0)
		return TRUE;

	return FALSE;
}

/*
 * Find correct tuple in non-leaf page. It supposed that
 * page correctly choosen and searching value SHOULD be on page
 */
static BlockNumber
entryLocateEntry(GinBtree btree, GinBtreeStack *stack)
{
	OffsetNumber low,
				high,
				maxoff;
	IndexTuple	itup = NULL;
	int			result;
	Page		page = BufferGetPage(stack->buffer);

	Assert(!GinPageIsLeaf(page));
	Assert(!GinPageIsData(page));

	if (btree->fullScan)
	{
		stack->off = FirstOffsetNumber;
		stack->predictNumber *= PageGetMaxOffsetNumber(page);
		return btree->getLeftMostPage(btree, page);
	}

	low = FirstOffsetNumber;
	maxoff = high = PageGetMaxOffsetNumber(page);
	Assert(high >= low);

	high++;

	while (high > low)
	{
		OffsetNumber mid = low + ((high - low) / 2);

		if (mid == maxoff && GinPageRightMost(page))
			/* Right infinity */
			result = -1;
		else
		{
			itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, mid));
			result = compareAttEntries(btree->ginstate,
									   btree->entryAttnum, btree->entryValue,
								 gintuple_get_attrnum(btree->ginstate, itup),
								   gin_index_getattr(btree->ginstate, itup));
		}

		if (result == 0)
		{
			stack->off = mid;
			Assert(GinItemPointerGetBlockNumber(&(itup)->t_tid) != GIN_ROOT_BLKNO);
			return GinItemPointerGetBlockNumber(&(itup)->t_tid);
		}
		else if (result > 0)
			low = mid + 1;
		else
			high = mid;
	}

	Assert(high >= FirstOffsetNumber && high <= maxoff);

	stack->off = high;
	itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, high));
	Assert(GinItemPointerGetBlockNumber(&(itup)->t_tid) != GIN_ROOT_BLKNO);
	return GinItemPointerGetBlockNumber(&(itup)->t_tid);
}

/*
 * Searches correct position for value on leaf page.
 * Page should be corrrectly choosen.
 * Returns true if value found on page.
 */
static bool
entryLocateLeafEntry(GinBtree btree, GinBtreeStack *stack)
{
	Page		page = BufferGetPage(stack->buffer);
	OffsetNumber low,
				high;
	IndexTuple	itup;

	Assert(GinPageIsLeaf(page));
	Assert(!GinPageIsData(page));

	if (btree->fullScan)
	{
		stack->off = FirstOffsetNumber;
		return TRUE;
	}

	low = FirstOffsetNumber;
	high = PageGetMaxOffsetNumber(page);

	if (high < low)
	{
		stack->off = FirstOffsetNumber;
		return false;
	}

	high++;

	while (high > low)
	{
		OffsetNumber mid = low + ((high - low) / 2);
		int			result;

		itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, mid));
		result = compareAttEntries(btree->ginstate,
								   btree->entryAttnum, btree->entryValue,
								 gintuple_get_attrnum(btree->ginstate, itup),
								   gin_index_getattr(btree->ginstate, itup));
		if (result == 0)
		{
			stack->off = mid;
			return true;
		}
		else if (result > 0)
			low = mid + 1;
		else
			high = mid;
	}

	stack->off = high;
	return false;
}

static OffsetNumber
entryFindChildPtr(GinBtree btree, Page page, BlockNumber blkno, OffsetNumber storedOff)
{
	OffsetNumber i,
				maxoff = PageGetMaxOffsetNumber(page);
	IndexTuple	itup;

	Assert(!GinPageIsLeaf(page));
	Assert(!GinPageIsData(page));

	/* if page isn't changed, we returns storedOff */
	if (storedOff >= FirstOffsetNumber && storedOff <= maxoff)
	{
		itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, storedOff));
		if (GinItemPointerGetBlockNumber(&(itup)->t_tid) == blkno)
			return storedOff;

		/*
		 * we hope, that needed pointer goes to right. It's true if there
		 * wasn't a deletion
		 */
		for (i = storedOff + 1; i <= maxoff; i++)
		{
			itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, i));
			if (GinItemPointerGetBlockNumber(&(itup)->t_tid) == blkno)
				return i;
		}
		maxoff = storedOff - 1;
	}

	/* last chance */
	for (i = FirstOffsetNumber; i <= maxoff; i++)
	{
		itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, i));
		if (GinItemPointerGetBlockNumber(&(itup)->t_tid) == blkno)
			return i;
	}

	return InvalidOffsetNumber;
}

static BlockNumber
entryGetLeftMostPage(GinBtree btree, Page page)
{
	IndexTuple	itup;

	Assert(!GinPageIsLeaf(page));
	Assert(!GinPageIsData(page));
	Assert(PageGetMaxOffsetNumber(page) >= FirstOffsetNumber);

	itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, FirstOffsetNumber));
	return GinItemPointerGetBlockNumber(&(itup)->t_tid);
}

static bool
entryIsEnoughSpace(GinBtree btree, Buffer buf, OffsetNumber off)
{
	Size		itupsz = 0;
	Page		page = BufferGetPage(buf);

	Assert(btree->entry);
	Assert(!GinPageIsData(page));

	if (btree->isDelete)
	{
		IndexTuple	itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, off));

		itupsz = MAXALIGN(IndexTupleSize(itup)) + sizeof(ItemIdData);
	}

	if (PageGetFreeSpace(page) + itupsz >= MAXALIGN(IndexTupleSize(btree->entry)) + sizeof(ItemIdData))
		return true;

	return false;
}

/*
 * Delete tuple on leaf page if tuples was existed and we
 * should update it, update old child blkno to new right page
 * if child split is occured
 */
static BlockNumber
entryPreparePage(GinBtree btree, Page page, OffsetNumber off)
{
	BlockNumber ret = InvalidBlockNumber;

	Assert(btree->entry);
	Assert(!GinPageIsData(page));

	if (btree->isDelete)
	{
		Assert(GinPageIsLeaf(page));
		PageIndexTupleDelete(page, off);
	}

	if (!GinPageIsLeaf(page) && btree->rightblkno != InvalidBlockNumber)
	{
		IndexTuple	itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, off));

		ItemPointerSet(&itup->t_tid, btree->rightblkno, InvalidOffsetNumber);
		ret = btree->rightblkno;
	}

	btree->rightblkno = InvalidBlockNumber;

	return ret;
}

/*
 * Place tuple on page and fills WAL record
 */
static void
entryPlaceToPage(GinBtree btree, Buffer buf, OffsetNumber off, XLogRecData **prdata)
{
	Page		page = BufferGetPage(buf);
	static XLogRecData rdata[3];
	OffsetNumber placed;
	static ginxlogInsert data;
	int			cnt = 0;

	*prdata = rdata;
	data.updateBlkno = entryPreparePage(btree, page, off);

	placed = PageAddItem(page, (Item) btree->entry, IndexTupleSize(btree->entry), off, false, false);
	if (placed != off)
		elog(ERROR, "failed to add item to index page in \"%s\"",
			 RelationGetRelationName(btree->index));

	data.node = btree->index->rd_node;
	data.blkno = BufferGetBlockNumber(buf);
	data.offset = off;
	data.nitem = 1;
	data.isDelete = btree->isDelete;
	data.isData = false;
	data.isLeaf = GinPageIsLeaf(page) ? TRUE : FALSE;

	/*
	 * Prevent full page write if child's split occurs. That is needed to
	 * remove incomplete splits while replaying WAL
	 *
	 * data.updateBlkno contains new block number (of newly created right
	 * page) for recently splited page.
	 */
	if (data.updateBlkno == InvalidBlockNumber)
	{
		rdata[0].buffer = buf;
		rdata[0].buffer_std = TRUE;
		rdata[0].data = NULL;
		rdata[0].len = 0;
		rdata[0].next = &rdata[1];
		cnt++;
	}

	rdata[cnt].buffer = InvalidBuffer;
	rdata[cnt].data = (char *) &data;
	rdata[cnt].len = sizeof(ginxlogInsert);
	rdata[cnt].next = &rdata[cnt + 1];
	cnt++;

	rdata[cnt].buffer = InvalidBuffer;
	rdata[cnt].data = (char *) btree->entry;
	rdata[cnt].len = IndexTupleSize(btree->entry);
	rdata[cnt].next = NULL;

	btree->entry = NULL;
}

/*
 * Returns new tuple with copied value from source tuple.
 * New tuple will not store posting list
 */
static IndexTuple
copyIndexTuple(IndexTuple itup, Page page)
{
	IndexTuple	nitup;

	if (GinPageIsLeaf(page) && !GinIsPostingTree(itup))
	{
		nitup = (IndexTuple) palloc(MAXALIGN(GinGetOrigSizePosting(itup)));
		memcpy(nitup, itup, GinGetOrigSizePosting(itup));
		nitup->t_info &= ~INDEX_SIZE_MASK;
		nitup->t_info |= GinGetOrigSizePosting(itup);
	}
	else
	{
		nitup = (IndexTuple) palloc(MAXALIGN(IndexTupleSize(itup)));
		memcpy(nitup, itup, IndexTupleSize(itup));
	}

	return nitup;
}

/*
 * Place tuple and split page, original buffer(lbuf) leaves untouched,
 * returns shadow page of lbuf filled new data.
 * Tuples are distributed between pages by equal size on its, not
 * an equal number!
 */
static Page
entrySplitPage(GinBtree btree, Buffer lbuf, Buffer rbuf, OffsetNumber off, XLogRecData **prdata)
{
	static XLogRecData rdata[2];
	OffsetNumber i,
				maxoff,
				separator = InvalidOffsetNumber;
	Size		totalsize = 0;
	Size		lsize = 0,
				size;
	static char tupstore[2 * BLCKSZ];
	char	   *ptr;
	IndexTuple	itup,
				leftrightmost = NULL;
	static ginxlogSplit data;
	Page		page;
	Page		lpage = PageGetTempPageCopy(BufferGetPage(lbuf));
	Page		rpage = BufferGetPage(rbuf);
	Size		pageSize = PageGetPageSize(lpage);

	*prdata = rdata;
	data.leftChildBlkno = (GinPageIsLeaf(lpage)) ?
		InvalidOffsetNumber : GinItemPointerGetBlockNumber(&(btree->entry->t_tid));
	data.updateBlkno = entryPreparePage(btree, lpage, off);

	maxoff = PageGetMaxOffsetNumber(lpage);
	ptr = tupstore;

	for (i = FirstOffsetNumber; i <= maxoff; i++)
	{
		if (i == off)
		{
			size = MAXALIGN(IndexTupleSize(btree->entry));
			memcpy(ptr, btree->entry, size);
			ptr += size;
			totalsize += size + sizeof(ItemIdData);
		}

		itup = (IndexTuple) PageGetItem(lpage, PageGetItemId(lpage, i));
		size = MAXALIGN(IndexTupleSize(itup));
		memcpy(ptr, itup, size);
		ptr += size;
		totalsize += size + sizeof(ItemIdData);
	}

	if (off == maxoff + 1)
	{
		size = MAXALIGN(IndexTupleSize(btree->entry));
		memcpy(ptr, btree->entry, size);
		ptr += size;
		totalsize += size + sizeof(ItemIdData);
	}

	GinInitPage(rpage, GinPageGetOpaque(lpage)->flags, pageSize);
	GinInitPage(lpage, GinPageGetOpaque(rpage)->flags, pageSize);

	ptr = tupstore;
	maxoff++;
	lsize = 0;

	page = lpage;
	for (i = FirstOffsetNumber; i <= maxoff; i++)
	{
		itup = (IndexTuple) ptr;

		if (lsize > totalsize / 2)
		{
			if (separator == InvalidOffsetNumber)
				separator = i - 1;
			page = rpage;
		}
		else
		{
			leftrightmost = itup;
			lsize += MAXALIGN(IndexTupleSize(itup)) + sizeof(ItemIdData);
		}

		if (PageAddItem(page, (Item) itup, IndexTupleSize(itup), InvalidOffsetNumber, false, false) == InvalidOffsetNumber)
			elog(ERROR, "failed to add item to index page in \"%s\"",
				 RelationGetRelationName(btree->index));
		ptr += MAXALIGN(IndexTupleSize(itup));
	}

	btree->entry = copyIndexTuple(leftrightmost, lpage);
	ItemPointerSet(&(btree->entry)->t_tid, BufferGetBlockNumber(lbuf), InvalidOffsetNumber);

	btree->rightblkno = BufferGetBlockNumber(rbuf);

	data.node = btree->index->rd_node;
	data.rootBlkno = InvalidBlockNumber;
	data.lblkno = BufferGetBlockNumber(lbuf);
	data.rblkno = BufferGetBlockNumber(rbuf);
	data.separator = separator;
	data.nitem = maxoff;
	data.isData = FALSE;
	data.isLeaf = GinPageIsLeaf(lpage) ? TRUE : FALSE;
	data.isRootSplit = FALSE;

	rdata[0].buffer = InvalidBuffer;
	rdata[0].data = (char *) &data;
	rdata[0].len = sizeof(ginxlogSplit);
	rdata[0].next = &rdata[1];

	rdata[1].buffer = InvalidBuffer;
	rdata[1].data = tupstore;
	rdata[1].len = MAXALIGN(totalsize);
	rdata[1].next = NULL;

	return lpage;
}

/*
 * return newly allocated rightmost tuple
 */
IndexTuple
ginPageGetLinkItup(Buffer buf)
{
	IndexTuple	itup,
				nitup;
	Page		page = BufferGetPage(buf);

	itup = getRightMostTuple(page);
	nitup = copyIndexTuple(itup, page);
	ItemPointerSet(&nitup->t_tid, BufferGetBlockNumber(buf), InvalidOffsetNumber);

	return nitup;
}

/*
 * Fills new root by rightest values from child.
 * Also called from ginxlog, should not use btree
 */
void
entryFillRoot(GinBtree btree, Buffer root, Buffer lbuf, Buffer rbuf)
{
	Page		page;
	IndexTuple	itup;

	page = BufferGetPage(root);

	itup = ginPageGetLinkItup(lbuf);
	if (PageAddItem(page, (Item) itup, IndexTupleSize(itup), InvalidOffsetNumber, false, false) == InvalidOffsetNumber)
		elog(ERROR, "failed to add item to index root page");
	pfree(itup);

	itup = ginPageGetLinkItup(rbuf);
	if (PageAddItem(page, (Item) itup, IndexTupleSize(itup), InvalidOffsetNumber, false, false) == InvalidOffsetNumber)
		elog(ERROR, "failed to add item to index root page");
	pfree(itup);
}

void
prepareEntryScan(GinBtree btree, Relation index, OffsetNumber attnum, Datum value, GinState *ginstate)
{
	memset(btree, 0, sizeof(GinBtreeData));

	btree->isMoveRight = entryIsMoveRight;
	btree->findChildPage = entryLocateEntry;
	btree->findItem = entryLocateLeafEntry;
	btree->findChildPtr = entryFindChildPtr;
	btree->getLeftMostPage = entryGetLeftMostPage;
	btree->isEnoughSpace = entryIsEnoughSpace;
	btree->placeToPage = entryPlaceToPage;
	btree->splitPage = entrySplitPage;
	btree->fillRoot = entryFillRoot;

	btree->index = index;
	btree->ginstate = ginstate;
	btree->entryAttnum = attnum;
	btree->entryValue = value;

	btree->isDelete = FALSE;
	btree->searchMode = FALSE;
	btree->fullScan = FALSE;
	btree->isBuild = FALSE;
}
