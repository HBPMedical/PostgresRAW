/*-------------------------------------------------------------------------
 *
 * cluster.c
 *	  CLUSTER a table on an index.	This is now also used for VACUUM FULL.
 *
 * There is hardly anything left of Paul Brown's original implementation...
 *
 *
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994-5, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/cluster.c,v 1.203 2010/04/28 16:10:41 heikki Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/rewriteheap.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_namespace.h"
#include "catalog/toasting.h"
#include "commands/cluster.h"
#include "commands/tablecmds.h"
#include "commands/trigger.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/procarray.h"
#include "storage/smgr.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/relcache.h"
#include "utils/relmapper.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/tqual.h"


/*
 * This struct is used to pass around the information on tables to be
 * clustered. We need this so we can make a list of them when invoked without
 * a specific table/index pair.
 */
typedef struct
{
	Oid			tableOid;
	Oid			indexOid;
} RelToCluster;


static void rebuild_relation(Relation OldHeap, Oid indexOid,
				 int freeze_min_age, int freeze_table_age);
static void copy_heap_data(Oid OIDNewHeap, Oid OIDOldHeap, Oid OIDOldIndex,
			   int freeze_min_age, int freeze_table_age,
			   bool *pSwapToastByContent, TransactionId *pFreezeXid);
static List *get_tables_to_cluster(MemoryContext cluster_context);



/*---------------------------------------------------------------------------
 * This cluster code allows for clustering multiple tables at once. Because
 * of this, we cannot just run everything on a single transaction, or we
 * would be forced to acquire exclusive locks on all the tables being
 * clustered, simultaneously --- very likely leading to deadlock.
 *
 * To solve this we follow a similar strategy to VACUUM code,
 * clustering each relation in a separate transaction. For this to work,
 * we need to:
 *	- provide a separate memory context so that we can pass information in
 *	  a way that survives across transactions
 *	- start a new transaction every time a new relation is clustered
 *	- check for validity of the information on to-be-clustered relations,
 *	  as someone might have deleted a relation behind our back, or
 *	  clustered one on a different index
 *	- end the transaction
 *
 * The single-relation case does not have any such overhead.
 *
 * We also allow a relation to be specified without index.	In that case,
 * the indisclustered bit will be looked up, and an ERROR will be thrown
 * if there is no index with the bit set.
 *---------------------------------------------------------------------------
 */
void
cluster(ClusterStmt *stmt, bool isTopLevel)
{
	if (stmt->relation != NULL)
	{
		/* This is the single-relation case. */
		Oid			tableOid,
					indexOid = InvalidOid;
		Relation	rel;

		/* Find and lock the table */
		rel = heap_openrv(stmt->relation, AccessExclusiveLock);

		tableOid = RelationGetRelid(rel);

		/* Check permissions */
		if (!pg_class_ownercheck(tableOid, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
						   RelationGetRelationName(rel));

		/*
		 * Reject clustering a remote temp table ... their local buffer
		 * manager is not going to cope.
		 */
		if (RELATION_IS_OTHER_TEMP(rel))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			   errmsg("cannot cluster temporary tables of other sessions")));

		if (stmt->indexname == NULL)
		{
			ListCell   *index;

			/* We need to find the index that has indisclustered set. */
			foreach(index, RelationGetIndexList(rel))
			{
				HeapTuple	idxtuple;
				Form_pg_index indexForm;

				indexOid = lfirst_oid(index);
				idxtuple = SearchSysCache1(INDEXRELID,
										   ObjectIdGetDatum(indexOid));
				if (!HeapTupleIsValid(idxtuple))
					elog(ERROR, "cache lookup failed for index %u", indexOid);
				indexForm = (Form_pg_index) GETSTRUCT(idxtuple);
				if (indexForm->indisclustered)
				{
					ReleaseSysCache(idxtuple);
					break;
				}
				ReleaseSysCache(idxtuple);
				indexOid = InvalidOid;
			}

			if (!OidIsValid(indexOid))
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("there is no previously clustered index for table \"%s\"",
								stmt->relation->relname)));
		}
		else
		{
			/*
			 * The index is expected to be in the same namespace as the
			 * relation.
			 */
			indexOid = get_relname_relid(stmt->indexname,
										 rel->rd_rel->relnamespace);
			if (!OidIsValid(indexOid))
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
					   errmsg("index \"%s\" for table \"%s\" does not exist",
							  stmt->indexname, stmt->relation->relname)));
		}

		/* close relation, keep lock till commit */
		heap_close(rel, NoLock);

		/* Do the job */
		cluster_rel(tableOid, indexOid, false, stmt->verbose, -1, -1);
	}
	else
	{
		/*
		 * This is the "multi relation" case. We need to cluster all tables
		 * that have some index with indisclustered set.
		 */
		MemoryContext cluster_context;
		List	   *rvs;
		ListCell   *rv;

		/*
		 * We cannot run this form of CLUSTER inside a user transaction block;
		 * we'd be holding locks way too long.
		 */
		PreventTransactionChain(isTopLevel, "CLUSTER");

		/*
		 * Create special memory context for cross-transaction storage.
		 *
		 * Since it is a child of PortalContext, it will go away even in case
		 * of error.
		 */
		cluster_context = AllocSetContextCreate(PortalContext,
												"Cluster",
												ALLOCSET_DEFAULT_MINSIZE,
												ALLOCSET_DEFAULT_INITSIZE,
												ALLOCSET_DEFAULT_MAXSIZE);

		/*
		 * Build the list of relations to cluster.	Note that this lives in
		 * cluster_context.
		 */
		rvs = get_tables_to_cluster(cluster_context);

		/* Commit to get out of starting transaction */
		PopActiveSnapshot();
		CommitTransactionCommand();

		/* Ok, now that we've got them all, cluster them one by one */
		foreach(rv, rvs)
		{
			RelToCluster *rvtc = (RelToCluster *) lfirst(rv);

			/* Start a new transaction for each relation. */
			StartTransactionCommand();
			/* functions in indexes may want a snapshot set */
			PushActiveSnapshot(GetTransactionSnapshot());
			cluster_rel(rvtc->tableOid, rvtc->indexOid, true, stmt->verbose,
						-1, -1);
			PopActiveSnapshot();
			CommitTransactionCommand();
		}

		/* Start a new transaction for the cleanup work. */
		StartTransactionCommand();

		/* Clean up working storage */
		MemoryContextDelete(cluster_context);
	}
}

/*
 * cluster_rel
 *
 * This clusters the table by creating a new, clustered table and
 * swapping the relfilenodes of the new table and the old table, so
 * the OID of the original table is preserved.	Thus we do not lose
 * GRANT, inheritance nor references to this table (this was a bug
 * in releases thru 7.3).
 *
 * Indexes are rebuilt too, via REINDEX. Since we are effectively bulk-loading
 * the new table, it's better to create the indexes afterwards than to fill
 * them incrementally while we load the table.
 *
 * If indexOid is InvalidOid, the table will be rewritten in physical order
 * instead of index order.	This is the new implementation of VACUUM FULL,
 * and error messages should refer to the operation as VACUUM not CLUSTER.
 */
void
cluster_rel(Oid tableOid, Oid indexOid, bool recheck, bool verbose,
			int freeze_min_age, int freeze_table_age)
{
	Relation	OldHeap;

	/* Check for user-requested abort. */
	CHECK_FOR_INTERRUPTS();

	/*
	 * We grab exclusive access to the target rel and index for the duration
	 * of the transaction.	(This is redundant for the single-transaction
	 * case, since cluster() already did it.)  The index lock is taken inside
	 * check_index_is_clusterable.
	 */
	OldHeap = try_relation_open(tableOid, AccessExclusiveLock);

	/* If the table has gone away, we can skip processing it */
	if (!OldHeap)
		return;

	/*
	 * Since we may open a new transaction for each relation, we have to check
	 * that the relation still is what we think it is.
	 *
	 * If this is a single-transaction CLUSTER, we can skip these tests. We
	 * *must* skip the one on indisclustered since it would reject an attempt
	 * to cluster a not-previously-clustered index.
	 */
	if (recheck)
	{
		HeapTuple	tuple;
		Form_pg_index indexForm;

		/* Check that the user still owns the relation */
		if (!pg_class_ownercheck(tableOid, GetUserId()))
		{
			relation_close(OldHeap, AccessExclusiveLock);
			return;
		}

		/*
		 * Silently skip a temp table for a remote session.  Only doing this
		 * check in the "recheck" case is appropriate (which currently means
		 * somebody is executing a database-wide CLUSTER), because there is
		 * another check in cluster() which will stop any attempt to cluster
		 * remote temp tables by name.	There is another check in cluster_rel
		 * which is redundant, but we leave it for extra safety.
		 */
		if (RELATION_IS_OTHER_TEMP(OldHeap))
		{
			relation_close(OldHeap, AccessExclusiveLock);
			return;
		}

		if (OidIsValid(indexOid))
		{
			/*
			 * Check that the index still exists
			 */
			if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(indexOid)))
			{
				relation_close(OldHeap, AccessExclusiveLock);
				return;
			}

			/*
			 * Check that the index is still the one with indisclustered set.
			 */
			tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexOid));
			if (!HeapTupleIsValid(tuple))		/* probably can't happen */
			{
				relation_close(OldHeap, AccessExclusiveLock);
				return;
			}
			indexForm = (Form_pg_index) GETSTRUCT(tuple);
			if (!indexForm->indisclustered)
			{
				ReleaseSysCache(tuple);
				relation_close(OldHeap, AccessExclusiveLock);
				return;
			}
			ReleaseSysCache(tuple);
		}
	}

	/*
	 * We allow VACUUM FULL, but not CLUSTER, on shared catalogs.  CLUSTER
	 * would work in most respects, but the index would only get marked as
	 * indisclustered in the current database, leading to unexpected behavior
	 * if CLUSTER were later invoked in another database.
	 */
	if (OidIsValid(indexOid) && OldHeap->rd_rel->relisshared)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot cluster a shared catalog")));

	/*
	 * Don't process temp tables of other backends ... their local buffer
	 * manager is not going to cope.
	 */
	if (RELATION_IS_OTHER_TEMP(OldHeap))
	{
		if (OidIsValid(indexOid))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			   errmsg("cannot cluster temporary tables of other sessions")));
		else
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("cannot vacuum temporary tables of other sessions")));
	}

	/*
	 * Also check for active uses of the relation in the current transaction,
	 * including open scans and pending AFTER trigger events.
	 */
	CheckTableNotInUse(OldHeap, OidIsValid(indexOid) ? "CLUSTER" : "VACUUM");

	/* Check heap and index are valid to cluster on */
	if (OidIsValid(indexOid))
		check_index_is_clusterable(OldHeap, indexOid, recheck);

	/* Log what we're doing (this could use more effort) */
	if (OidIsValid(indexOid))
		ereport(verbose ? INFO : DEBUG2,
				(errmsg("clustering \"%s.%s\"",
						get_namespace_name(RelationGetNamespace(OldHeap)),
						RelationGetRelationName(OldHeap))));
	else
		ereport(verbose ? INFO : DEBUG2,
				(errmsg("vacuuming \"%s.%s\"",
						get_namespace_name(RelationGetNamespace(OldHeap)),
						RelationGetRelationName(OldHeap))));

	/* rebuild_relation does all the dirty work */
	rebuild_relation(OldHeap, indexOid, freeze_min_age, freeze_table_age);

	/* NB: rebuild_relation does heap_close() on OldHeap */
}

/*
 * Verify that the specified heap and index are valid to cluster on
 *
 * Side effect: obtains exclusive lock on the index.  The caller should
 * already have exclusive lock on the table, so the index lock is likely
 * redundant, but it seems best to grab it anyway to ensure the index
 * definition can't change under us.
 */
void
check_index_is_clusterable(Relation OldHeap, Oid indexOid, bool recheck)
{
	Relation	OldIndex;

	OldIndex = index_open(indexOid, AccessExclusiveLock);

	/*
	 * Check that index is in fact an index on the given relation
	 */
	if (OldIndex->rd_index == NULL ||
		OldIndex->rd_index->indrelid != RelationGetRelid(OldHeap))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not an index for table \"%s\"",
						RelationGetRelationName(OldIndex),
						RelationGetRelationName(OldHeap))));

	/* Index AM must allow clustering */
	if (!OldIndex->rd_am->amclusterable)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot cluster on index \"%s\" because access method does not support clustering",
						RelationGetRelationName(OldIndex))));

	/*
	 * Disallow clustering on incomplete indexes (those that might not index
	 * every row of the relation).	We could relax this by making a separate
	 * seqscan pass over the table to copy the missing rows, but that seems
	 * expensive and tedious.
	 */
	if (!heap_attisnull(OldIndex->rd_indextuple, Anum_pg_index_indpred))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot cluster on partial index \"%s\"",
						RelationGetRelationName(OldIndex))));

	if (!OldIndex->rd_am->amindexnulls)
	{
		AttrNumber	colno;

		/*
		 * If the AM doesn't index nulls, then it's a partial index unless we
		 * can prove all the rows are non-null.  Note we only need look at the
		 * first column; multicolumn-capable AMs are *required* to index nulls
		 * in columns after the first.
		 */
		colno = OldIndex->rd_index->indkey.values[0];
		if (colno > 0)
		{
			/* ordinary user attribute */
			if (!OldHeap->rd_att->attrs[colno - 1]->attnotnull)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot cluster on index \"%s\" because access method does not handle null values",
								RelationGetRelationName(OldIndex)),
						 recheck
						 ? errhint("You might be able to work around this by marking column \"%s\" NOT NULL, or use ALTER TABLE ... SET WITHOUT CLUSTER to remove the cluster specification from the table.",
						 NameStr(OldHeap->rd_att->attrs[colno - 1]->attname))
						 : errhint("You might be able to work around this by marking column \"%s\" NOT NULL.",
					  NameStr(OldHeap->rd_att->attrs[colno - 1]->attname))));
		}
		else if (colno < 0)
		{
			/* system column --- okay, always non-null */
		}
		else
			/* index expression, lose... */
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot cluster on expressional index \"%s\" because its index access method does not handle null values",
							RelationGetRelationName(OldIndex))));
	}

	/*
	 * Disallow if index is left over from a failed CREATE INDEX CONCURRENTLY;
	 * it might well not contain entries for every heap row, or might not even
	 * be internally consistent.  (But note that we don't check indcheckxmin;
	 * the worst consequence of following broken HOT chains would be that we
	 * might put recently-dead tuples out-of-order in the new table, and there
	 * is little harm in that.)
	 */
	if (!OldIndex->rd_index->indisvalid)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot cluster on invalid index \"%s\"",
						RelationGetRelationName(OldIndex))));

	/* Drop relcache refcnt on OldIndex, but keep lock */
	index_close(OldIndex, NoLock);
}

/*
 * mark_index_clustered: mark the specified index as the one clustered on
 *
 * With indexOid == InvalidOid, will mark all indexes of rel not-clustered.
 */
void
mark_index_clustered(Relation rel, Oid indexOid)
{
	HeapTuple	indexTuple;
	Form_pg_index indexForm;
	Relation	pg_index;
	ListCell   *index;

	/*
	 * If the index is already marked clustered, no need to do anything.
	 */
	if (OidIsValid(indexOid))
	{
		indexTuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexOid));
		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failed for index %u", indexOid);
		indexForm = (Form_pg_index) GETSTRUCT(indexTuple);

		if (indexForm->indisclustered)
		{
			ReleaseSysCache(indexTuple);
			return;
		}

		ReleaseSysCache(indexTuple);
	}

	/*
	 * Check each index of the relation and set/clear the bit as needed.
	 */
	pg_index = heap_open(IndexRelationId, RowExclusiveLock);

	foreach(index, RelationGetIndexList(rel))
	{
		Oid			thisIndexOid = lfirst_oid(index);

		indexTuple = SearchSysCacheCopy1(INDEXRELID,
										 ObjectIdGetDatum(thisIndexOid));
		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failed for index %u", thisIndexOid);
		indexForm = (Form_pg_index) GETSTRUCT(indexTuple);

		/*
		 * Unset the bit if set.  We know it's wrong because we checked this
		 * earlier.
		 */
		if (indexForm->indisclustered)
		{
			indexForm->indisclustered = false;
			simple_heap_update(pg_index, &indexTuple->t_self, indexTuple);
			CatalogUpdateIndexes(pg_index, indexTuple);
		}
		else if (thisIndexOid == indexOid)
		{
			indexForm->indisclustered = true;
			simple_heap_update(pg_index, &indexTuple->t_self, indexTuple);
			CatalogUpdateIndexes(pg_index, indexTuple);
		}
		heap_freetuple(indexTuple);
	}

	heap_close(pg_index, RowExclusiveLock);
}

/*
 * rebuild_relation: rebuild an existing relation in index or physical order
 *
 * OldHeap: table to rebuild --- must be opened and exclusive-locked!
 * indexOid: index to cluster by, or InvalidOid to rewrite in physical order.
 *
 * NB: this routine closes OldHeap at the right time; caller should not.
 */
static void
rebuild_relation(Relation OldHeap, Oid indexOid,
				 int freeze_min_age, int freeze_table_age)
{
	Oid			tableOid = RelationGetRelid(OldHeap);
	Oid			tableSpace = OldHeap->rd_rel->reltablespace;
	Oid			OIDNewHeap;
	bool		is_system_catalog;
	bool		swap_toast_by_content;
	TransactionId frozenXid;

	/* Mark the correct index as clustered */
	if (OidIsValid(indexOid))
		mark_index_clustered(OldHeap, indexOid);

	/* Remember if it's a system catalog */
	is_system_catalog = IsSystemRelation(OldHeap);

	/* Close relcache entry, but keep lock until transaction commit */
	heap_close(OldHeap, NoLock);

	/* Create the transient table that will receive the re-ordered data */
	OIDNewHeap = make_new_heap(tableOid, tableSpace);

	/* Copy the heap data into the new table in the desired order */
	copy_heap_data(OIDNewHeap, tableOid, indexOid,
				   freeze_min_age, freeze_table_age,
				   &swap_toast_by_content, &frozenXid);

	/*
	 * Swap the physical files of the target and transient tables, then
	 * rebuild the target's indexes and throw away the transient table.
	 */
	finish_heap_swap(tableOid, OIDNewHeap, is_system_catalog,
					 swap_toast_by_content, frozenXid);
}


/*
 * Create the transient table that will be filled with new data during
 * CLUSTER, ALTER TABLE, and similar operations.  The transient table
 * duplicates the logical structure of the OldHeap, but is placed in
 * NewTableSpace which might be different from OldHeap's.
 *
 * After this, the caller should load the new heap with transferred/modified
 * data, then call finish_heap_swap to complete the operation.
 */
Oid
make_new_heap(Oid OIDOldHeap, Oid NewTableSpace)
{
	TupleDesc	OldHeapDesc,
				tupdesc;
	char		NewHeapName[NAMEDATALEN];
	Oid			OIDNewHeap;
	Oid			toastid;
	Relation	OldHeap;
	HeapTuple	tuple;
	Datum		reloptions;
	bool		isNull;

	OldHeap = heap_open(OIDOldHeap, AccessExclusiveLock);
	OldHeapDesc = RelationGetDescr(OldHeap);

	/*
	 * Need to make a copy of the tuple descriptor, since
	 * heap_create_with_catalog modifies it.  Note that the NewHeap will not
	 * receive any of the defaults or constraints associated with the OldHeap;
	 * we don't need 'em, and there's no reason to spend cycles inserting them
	 * into the catalogs only to delete them.
	 */
	tupdesc = CreateTupleDescCopy(OldHeapDesc);

	/*
	 * But we do want to use reloptions of the old heap for new heap.
	 */
	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(OIDOldHeap));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", OIDOldHeap);
	reloptions = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_reloptions,
								 &isNull);
	if (isNull)
		reloptions = (Datum) 0;

	/*
	 * Create the new heap, using a temporary name in the same namespace as
	 * the existing table.	NOTE: there is some risk of collision with user
	 * relnames.  Working around this seems more trouble than it's worth; in
	 * particular, we can't create the new heap in a different namespace from
	 * the old, or we will have problems with the TEMP status of temp tables.
	 *
	 * Note: the new heap is not a shared relation, even if we are rebuilding
	 * a shared rel.  However, we do make the new heap mapped if the source is
	 * mapped.	This simplifies swap_relation_files, and is absolutely
	 * necessary for rebuilding pg_class, for reasons explained there.
	 */
	snprintf(NewHeapName, sizeof(NewHeapName), "pg_temp_%u", OIDOldHeap);

	OIDNewHeap = heap_create_with_catalog(NewHeapName,
										  RelationGetNamespace(OldHeap),
										  NewTableSpace,
										  InvalidOid,
										  InvalidOid,
										  InvalidOid,
										  OldHeap->rd_rel->relowner,
										  tupdesc,
										  NIL,
										  OldHeap->rd_rel->relkind,
										  false,
										  RelationIsMapped(OldHeap),
										  true,
										  0,
										  ONCOMMIT_NOOP,
										  reloptions,
										  false,
										  true);

	ReleaseSysCache(tuple);

	/*
	 * Advance command counter so that the newly-created relation's catalog
	 * tuples will be visible to heap_open.
	 */
	CommandCounterIncrement();

	/*
	 * If necessary, create a TOAST table for the new relation.
	 *
	 * If the relation doesn't have a TOAST table already, we can't need one
	 * for the new relation.  The other way around is possible though: if some
	 * wide columns have been dropped, AlterTableCreateToastTable can decide
	 * that no TOAST table is needed for the new table.
	 *
	 * Note that AlterTableCreateToastTable ends with CommandCounterIncrement,
	 * so that the TOAST table will be visible for insertion.
	 */
	toastid = OldHeap->rd_rel->reltoastrelid;
	if (OidIsValid(toastid))
	{
		/* keep the existing toast table's reloptions, if any */
		tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(toastid));
		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "cache lookup failed for relation %u", toastid);
		reloptions = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_reloptions,
									 &isNull);
		if (isNull)
			reloptions = (Datum) 0;

		AlterTableCreateToastTable(OIDNewHeap, reloptions);

		ReleaseSysCache(tuple);
	}

	heap_close(OldHeap, NoLock);

	return OIDNewHeap;
}

/*
 * Do the physical copying of heap data.
 *
 * There are two output parameters:
 * *pSwapToastByContent is set true if toast tables must be swapped by content.
 * *pFreezeXid receives the TransactionId used as freeze cutoff point.
 */
static void
copy_heap_data(Oid OIDNewHeap, Oid OIDOldHeap, Oid OIDOldIndex,
			   int freeze_min_age, int freeze_table_age,
			   bool *pSwapToastByContent, TransactionId *pFreezeXid)
{
	Relation	NewHeap,
				OldHeap,
				OldIndex;
	TupleDesc	oldTupDesc;
	TupleDesc	newTupDesc;
	int			natts;
	Datum	   *values;
	bool	   *isnull;
	IndexScanDesc indexScan;
	HeapScanDesc heapScan;
	bool		use_wal;
	bool		is_system_catalog;
	TransactionId OldestXmin;
	TransactionId FreezeXid;
	RewriteState rwstate;

	/*
	 * Open the relations we need.
	 */
	NewHeap = heap_open(OIDNewHeap, AccessExclusiveLock);
	OldHeap = heap_open(OIDOldHeap, AccessExclusiveLock);
	if (OidIsValid(OIDOldIndex))
		OldIndex = index_open(OIDOldIndex, AccessExclusiveLock);
	else
		OldIndex = NULL;

	/*
	 * Their tuple descriptors should be exactly alike, but here we only need
	 * assume that they have the same number of columns.
	 */
	oldTupDesc = RelationGetDescr(OldHeap);
	newTupDesc = RelationGetDescr(NewHeap);
	Assert(newTupDesc->natts == oldTupDesc->natts);

	/* Preallocate values/isnull arrays */
	natts = newTupDesc->natts;
	values = (Datum *) palloc(natts * sizeof(Datum));
	isnull = (bool *) palloc(natts * sizeof(bool));

	/*
	 * We need to log the copied data in WAL iff WAL archiving/streaming is
	 * enabled AND it's not a temp rel.
	 */
	use_wal = XLogIsNeeded() && !NewHeap->rd_istemp;

	/* use_wal off requires smgr_targblock be initially invalid */
	Assert(RelationGetTargetBlock(NewHeap) == InvalidBlockNumber);

	/*
	 * If both tables have TOAST tables, perform toast swap by content.  It is
	 * possible that the old table has a toast table but the new one doesn't,
	 * if toastable columns have been dropped.	In that case we have to do
	 * swap by links.  This is okay because swap by content is only essential
	 * for system catalogs, and we don't support schema changes for them.
	 */
	if (OldHeap->rd_rel->reltoastrelid && NewHeap->rd_rel->reltoastrelid)
	{
		*pSwapToastByContent = true;

		/*
		 * When doing swap by content, any toast pointers written into NewHeap
		 * must use the old toast table's OID, because that's where the toast
		 * data will eventually be found.  Set this up by setting rd_toastoid.
		 * Note that we must hold NewHeap open until we are done writing data,
		 * since the relcache will not guarantee to remember this setting once
		 * the relation is closed.	Also, this technique depends on the fact
		 * that no one will try to read from the NewHeap until after we've
		 * finished writing it and swapping the rels --- otherwise they could
		 * follow the toast pointers to the wrong place.
		 */
		NewHeap->rd_toastoid = OldHeap->rd_rel->reltoastrelid;
	}
	else
		*pSwapToastByContent = false;

	/*
	 * compute xids used to freeze and weed out dead tuples.  We use -1
	 * freeze_min_age to avoid having CLUSTER freeze tuples earlier than a
	 * plain VACUUM would.
	 */
	vacuum_set_xid_limits(freeze_min_age, freeze_table_age,
						  OldHeap->rd_rel->relisshared,
						  &OldestXmin, &FreezeXid, NULL);

	/*
	 * FreezeXid will become the table's new relfrozenxid, and that mustn't go
	 * backwards, so take the max.
	 */
	if (TransactionIdPrecedes(FreezeXid, OldHeap->rd_rel->relfrozenxid))
		FreezeXid = OldHeap->rd_rel->relfrozenxid;

	/* return selected value to caller */
	*pFreezeXid = FreezeXid;

	/* Remember if it's a system catalog */
	is_system_catalog = IsSystemRelation(OldHeap);

	/* Initialize the rewrite operation */
	rwstate = begin_heap_rewrite(NewHeap, OldestXmin, FreezeXid, use_wal);

	/*
	 * Scan through the OldHeap, either in OldIndex order or sequentially, and
	 * copy each tuple into the NewHeap.  To ensure we see recently-dead
	 * tuples that still need to be copied, we scan with SnapshotAny and use
	 * HeapTupleSatisfiesVacuum for the visibility test.
	 */
	if (OldIndex != NULL)
	{
		heapScan = NULL;
		indexScan = index_beginscan(OldHeap, OldIndex,
									SnapshotAny, 0, (ScanKey) NULL);
	}
	else
	{
		heapScan = heap_beginscan(OldHeap, SnapshotAny, 0, (ScanKey) NULL);
		indexScan = NULL;
	}

	for (;;)
	{
		HeapTuple	tuple;
		HeapTuple	copiedTuple;
		Buffer		buf;
		bool		isdead;
		int			i;

		CHECK_FOR_INTERRUPTS();

		if (OldIndex != NULL)
		{
			tuple = index_getnext(indexScan, ForwardScanDirection);
			if (tuple == NULL)
				break;

			/* Since we used no scan keys, should never need to recheck */
			if (indexScan->xs_recheck)
				elog(ERROR, "CLUSTER does not support lossy index conditions");

			buf = indexScan->xs_cbuf;
		}
		else
		{
			tuple = heap_getnext(heapScan, ForwardScanDirection);
			if (tuple == NULL)
				break;

			buf = heapScan->rs_cbuf;
		}

		LockBuffer(buf, BUFFER_LOCK_SHARE);

		switch (HeapTupleSatisfiesVacuum(tuple->t_data, OldestXmin, buf))
		{
			case HEAPTUPLE_DEAD:
				/* Definitely dead */
				isdead = true;
				break;
			case HEAPTUPLE_LIVE:
			case HEAPTUPLE_RECENTLY_DEAD:
				/* Live or recently dead, must copy it */
				isdead = false;
				break;
			case HEAPTUPLE_INSERT_IN_PROGRESS:

				/*
				 * Since we hold exclusive lock on the relation, normally the
				 * only way to see this is if it was inserted earlier in our
				 * own transaction.  However, it can happen in system
				 * catalogs, since we tend to release write lock before commit
				 * there.  Give a warning if neither case applies; but in any
				 * case we had better copy it.
				 */
				if (!is_system_catalog &&
					!TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tuple->t_data)))
					elog(WARNING, "concurrent insert in progress within table \"%s\"",
						 RelationGetRelationName(OldHeap));
				/* treat as live */
				isdead = false;
				break;
			case HEAPTUPLE_DELETE_IN_PROGRESS:

				/*
				 * Similar situation to INSERT_IN_PROGRESS case.
				 */
				Assert(!(tuple->t_data->t_infomask & HEAP_XMAX_IS_MULTI));
				if (!is_system_catalog &&
					!TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuple->t_data)))
					elog(WARNING, "concurrent delete in progress within table \"%s\"",
						 RelationGetRelationName(OldHeap));
				/* treat as recently dead */
				isdead = false;
				break;
			default:
				elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
				isdead = false; /* keep compiler quiet */
				break;
		}

		LockBuffer(buf, BUFFER_LOCK_UNLOCK);

		if (isdead)
		{
			/* heap rewrite module still needs to see it... */
			rewrite_heap_dead_tuple(rwstate, tuple);
			continue;
		}

		/*
		 * We cannot simply copy the tuple as-is, for several reasons:
		 *
		 * 1. We'd like to squeeze out the values of any dropped columns, both
		 * to save space and to ensure we have no corner-case failures. (It's
		 * possible for example that the new table hasn't got a TOAST table
		 * and so is unable to store any large values of dropped cols.)
		 *
		 * 2. The tuple might not even be legal for the new table; this is
		 * currently only known to happen as an after-effect of ALTER TABLE
		 * SET WITHOUT OIDS.
		 *
		 * So, we must reconstruct the tuple from component Datums.
		 */
		heap_deform_tuple(tuple, oldTupDesc, values, isnull);

		/* Be sure to null out any dropped columns */
		for (i = 0; i < natts; i++)
		{
			if (newTupDesc->attrs[i]->attisdropped)
				isnull[i] = true;
		}

		copiedTuple = heap_form_tuple(newTupDesc, values, isnull);

		/* Preserve OID, if any */
		if (NewHeap->rd_rel->relhasoids)
			HeapTupleSetOid(copiedTuple, HeapTupleGetOid(tuple));

		/* The heap rewrite module does the rest */
		rewrite_heap_tuple(rwstate, tuple, copiedTuple);

		heap_freetuple(copiedTuple);
	}

	if (OldIndex != NULL)
		index_endscan(indexScan);
	else
		heap_endscan(heapScan);

	/* Write out any remaining tuples, and fsync if needed */
	end_heap_rewrite(rwstate);

	/* Reset rd_toastoid just to be tidy --- it shouldn't be looked at again */
	NewHeap->rd_toastoid = InvalidOid;

	/* Clean up */
	pfree(values);
	pfree(isnull);

	if (OldIndex != NULL)
		index_close(OldIndex, NoLock);
	heap_close(OldHeap, NoLock);
	heap_close(NewHeap, NoLock);
}

/*
 * Swap the physical files of two given relations.
 *
 * We swap the physical identity (reltablespace and relfilenode) while
 * keeping the same logical identities of the two relations.
 *
 * We can swap associated TOAST data in either of two ways: recursively swap
 * the physical content of the toast tables (and their indexes), or swap the
 * TOAST links in the given relations' pg_class entries.  The former is needed
 * to manage rewrites of shared catalogs (where we cannot change the pg_class
 * links) while the latter is the only way to handle cases in which a toast
 * table is added or removed altogether.
 *
 * Additionally, the first relation is marked with relfrozenxid set to
 * frozenXid.  It seems a bit ugly to have this here, but the caller would
 * have to do it anyway, so having it here saves a heap_update.  Note: in
 * the swap-toast-links case, we assume we don't need to change the toast
 * table's relfrozenxid: the new version of the toast table should already
 * have relfrozenxid set to RecentXmin, which is good enough.
 *
 * Lastly, if r2 and its toast table and toast index (if any) are mapped,
 * their OIDs are emitted into mapped_tables[].  This is hacky but beats
 * having to look the information up again later in finish_heap_swap.
 */
static void
swap_relation_files(Oid r1, Oid r2, bool target_is_pg_class,
					bool swap_toast_by_content,
					TransactionId frozenXid,
					Oid *mapped_tables)
{
	Relation	relRelation;
	HeapTuple	reltup1,
				reltup2;
	Form_pg_class relform1,
				relform2;
	Oid			relfilenode1,
				relfilenode2;
	Oid			swaptemp;
	CatalogIndexState indstate;

	/* We need writable copies of both pg_class tuples. */
	relRelation = heap_open(RelationRelationId, RowExclusiveLock);

	reltup1 = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(r1));
	if (!HeapTupleIsValid(reltup1))
		elog(ERROR, "cache lookup failed for relation %u", r1);
	relform1 = (Form_pg_class) GETSTRUCT(reltup1);

	reltup2 = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(r2));
	if (!HeapTupleIsValid(reltup2))
		elog(ERROR, "cache lookup failed for relation %u", r2);
	relform2 = (Form_pg_class) GETSTRUCT(reltup2);

	relfilenode1 = relform1->relfilenode;
	relfilenode2 = relform2->relfilenode;

	if (OidIsValid(relfilenode1) && OidIsValid(relfilenode2))
	{
		/* Normal non-mapped relations: swap relfilenodes and reltablespaces */
		Assert(!target_is_pg_class);

		swaptemp = relform1->relfilenode;
		relform1->relfilenode = relform2->relfilenode;
		relform2->relfilenode = swaptemp;

		swaptemp = relform1->reltablespace;
		relform1->reltablespace = relform2->reltablespace;
		relform2->reltablespace = swaptemp;

		/* Also swap toast links, if we're swapping by links */
		if (!swap_toast_by_content)
		{
			swaptemp = relform1->reltoastrelid;
			relform1->reltoastrelid = relform2->reltoastrelid;
			relform2->reltoastrelid = swaptemp;

			/* we should NOT swap reltoastidxid */
		}
	}
	else
	{
		/*
		 * Mapped-relation case.  Here we have to swap the relation mappings
		 * instead of modifying the pg_class columns.  Both must be mapped.
		 */
		if (OidIsValid(relfilenode1) || OidIsValid(relfilenode2))
			elog(ERROR, "cannot swap mapped relation \"%s\" with non-mapped relation",
				 NameStr(relform1->relname));

		/*
		 * We can't change the tablespace of a mapped rel, and we can't handle
		 * toast link swapping for one either, because we must not apply any
		 * critical changes to its pg_class row.  These cases should be
		 * prevented by upstream permissions tests, so this check is a
		 * non-user-facing emergency backstop.
		 */
		if (relform1->reltablespace != relform2->reltablespace)
			elog(ERROR, "cannot change tablespace of mapped relation \"%s\"",
				 NameStr(relform1->relname));
		if (!swap_toast_by_content &&
			(relform1->reltoastrelid || relform2->reltoastrelid))
			elog(ERROR, "cannot swap toast by links for mapped relation \"%s\"",
				 NameStr(relform1->relname));

		/*
		 * Fetch the mappings --- shouldn't fail, but be paranoid
		 */
		relfilenode1 = RelationMapOidToFilenode(r1, relform1->relisshared);
		if (!OidIsValid(relfilenode1))
			elog(ERROR, "could not find relation mapping for relation \"%s\", OID %u",
				 NameStr(relform1->relname), r1);
		relfilenode2 = RelationMapOidToFilenode(r2, relform2->relisshared);
		if (!OidIsValid(relfilenode2))
			elog(ERROR, "could not find relation mapping for relation \"%s\", OID %u",
				 NameStr(relform2->relname), r2);

		/*
		 * Send replacement mappings to relmapper.	Note these won't actually
		 * take effect until CommandCounterIncrement.
		 */
		RelationMapUpdateMap(r1, relfilenode2, relform1->relisshared, false);
		RelationMapUpdateMap(r2, relfilenode1, relform2->relisshared, false);

		/* Pass OIDs of mapped r2 tables back to caller */
		*mapped_tables++ = r2;
	}

	/*
	 * In the case of a shared catalog, these next few steps will only affect
	 * our own database's pg_class row; but that's okay, because they are all
	 * noncritical updates.  That's also an important fact for the case of a
	 * mapped catalog, because it's possible that we'll commit the map change
	 * and then fail to commit the pg_class update.
	 */

	/* set rel1's frozen Xid */
	if (relform1->relkind != RELKIND_INDEX)
	{
		Assert(TransactionIdIsNormal(frozenXid));
		relform1->relfrozenxid = frozenXid;
	}

	/* swap size statistics too, since new rel has freshly-updated stats */
	{
		int4		swap_pages;
		float4		swap_tuples;

		swap_pages = relform1->relpages;
		relform1->relpages = relform2->relpages;
		relform2->relpages = swap_pages;

		swap_tuples = relform1->reltuples;
		relform1->reltuples = relform2->reltuples;
		relform2->reltuples = swap_tuples;
	}

	/*
	 * Update the tuples in pg_class --- unless the target relation of the
	 * swap is pg_class itself.  In that case, there is zero point in making
	 * changes because we'd be updating the old data that we're about to throw
	 * away.  Because the real work being done here for a mapped relation is
	 * just to change the relation map settings, it's all right to not update
	 * the pg_class rows in this case.
	 */
	if (!target_is_pg_class)
	{
		simple_heap_update(relRelation, &reltup1->t_self, reltup1);
		simple_heap_update(relRelation, &reltup2->t_self, reltup2);

		/* Keep system catalogs current */
		indstate = CatalogOpenIndexes(relRelation);
		CatalogIndexInsert(indstate, reltup1);
		CatalogIndexInsert(indstate, reltup2);
		CatalogCloseIndexes(indstate);
	}
	else
	{
		/* no update ... but we do still need relcache inval */
		CacheInvalidateRelcacheByTuple(reltup1);
		CacheInvalidateRelcacheByTuple(reltup2);
	}

	/*
	 * If we have toast tables associated with the relations being swapped,
	 * deal with them too.
	 */
	if (relform1->reltoastrelid || relform2->reltoastrelid)
	{
		if (swap_toast_by_content)
		{
			if (relform1->reltoastrelid && relform2->reltoastrelid)
			{
				/* Recursively swap the contents of the toast tables */
				swap_relation_files(relform1->reltoastrelid,
									relform2->reltoastrelid,
									target_is_pg_class,
									swap_toast_by_content,
									frozenXid,
									mapped_tables);
			}
			else
			{
				/* caller messed up */
				elog(ERROR, "cannot swap toast files by content when there's only one");
			}
		}
		else
		{
			/*
			 * We swapped the ownership links, so we need to change dependency
			 * data to match.
			 *
			 * NOTE: it is possible that only one table has a toast table.
			 *
			 * NOTE: at present, a TOAST table's only dependency is the one on
			 * its owning table.  If more are ever created, we'd need to use
			 * something more selective than deleteDependencyRecordsFor() to
			 * get rid of just the link we want.
			 */
			ObjectAddress baseobject,
						toastobject;
			long		count;

			/*
			 * We disallow this case for system catalogs, to avoid the
			 * possibility that the catalog we're rebuilding is one of the
			 * ones the dependency changes would change.  It's too late to be
			 * making any data changes to the target catalog.
			 */
			if (IsSystemClass(relform1))
				elog(ERROR, "cannot swap toast files by links for system catalogs");

			/* Delete old dependencies */
			if (relform1->reltoastrelid)
			{
				count = deleteDependencyRecordsFor(RelationRelationId,
												   relform1->reltoastrelid);
				if (count != 1)
					elog(ERROR, "expected one dependency record for TOAST table, found %ld",
						 count);
			}
			if (relform2->reltoastrelid)
			{
				count = deleteDependencyRecordsFor(RelationRelationId,
												   relform2->reltoastrelid);
				if (count != 1)
					elog(ERROR, "expected one dependency record for TOAST table, found %ld",
						 count);
			}

			/* Register new dependencies */
			baseobject.classId = RelationRelationId;
			baseobject.objectSubId = 0;
			toastobject.classId = RelationRelationId;
			toastobject.objectSubId = 0;

			if (relform1->reltoastrelid)
			{
				baseobject.objectId = r1;
				toastobject.objectId = relform1->reltoastrelid;
				recordDependencyOn(&toastobject, &baseobject,
								   DEPENDENCY_INTERNAL);
			}

			if (relform2->reltoastrelid)
			{
				baseobject.objectId = r2;
				toastobject.objectId = relform2->reltoastrelid;
				recordDependencyOn(&toastobject, &baseobject,
								   DEPENDENCY_INTERNAL);
			}
		}
	}

	/*
	 * If we're swapping two toast tables by content, do the same for their
	 * indexes.
	 */
	if (swap_toast_by_content &&
		relform1->reltoastidxid && relform2->reltoastidxid)
		swap_relation_files(relform1->reltoastidxid,
							relform2->reltoastidxid,
							target_is_pg_class,
							swap_toast_by_content,
							InvalidTransactionId,
							mapped_tables);

	/* Clean up. */
	heap_freetuple(reltup1);
	heap_freetuple(reltup2);

	heap_close(relRelation, RowExclusiveLock);

	/*
	 * Close both relcache entries' smgr links.  We need this kluge because
	 * both links will be invalidated during upcoming CommandCounterIncrement.
	 * Whichever of the rels is the second to be cleared will have a dangling
	 * reference to the other's smgr entry.  Rather than trying to avoid this
	 * by ordering operations just so, it's easiest to close the links first.
	 * (Fortunately, since one of the entries is local in our transaction,
	 * it's sufficient to clear out our own relcache this way; the problem
	 * cannot arise for other backends when they see our update on the
	 * non-transient relation.)
	 *
	 * Caution: the placement of this step interacts with the decision to
	 * handle toast rels by recursion.	When we are trying to rebuild pg_class
	 * itself, the smgr close on pg_class must happen after all accesses in
	 * this function.
	 */
	RelationCloseSmgrByOid(r1);
	RelationCloseSmgrByOid(r2);
}

/*
 * Remove the transient table that was built by make_new_heap, and finish
 * cleaning up (including rebuilding all indexes on the old heap).
 */
void
finish_heap_swap(Oid OIDOldHeap, Oid OIDNewHeap,
				 bool is_system_catalog,
				 bool swap_toast_by_content,
				 TransactionId frozenXid)
{
	ObjectAddress object;
	Oid			mapped_tables[4];
	int			i;

	/* Zero out possible results from swapped_relation_files */
	memset(mapped_tables, 0, sizeof(mapped_tables));

	/*
	 * Swap the contents of the heap relations (including any toast tables).
	 * Also set old heap's relfrozenxid to frozenXid.
	 */
	swap_relation_files(OIDOldHeap, OIDNewHeap,
						(OIDOldHeap == RelationRelationId),
						swap_toast_by_content, frozenXid, mapped_tables);

	/*
	 * If it's a system catalog, queue an sinval message to flush all
	 * catcaches on the catalog when we reach CommandCounterIncrement.
	 */
	if (is_system_catalog)
		CacheInvalidateCatalog(OIDOldHeap);

	/*
	 * Rebuild each index on the relation (but not the toast table, which is
	 * all-new at this point).	It is important to do this before the DROP
	 * step because if we are processing a system catalog that will be used
	 * during DROP, we want to have its indexes available.	There is no
	 * advantage to the other order anyway because this is all transactional,
	 * so no chance to reclaim disk space before commit.  We do not need a
	 * final CommandCounterIncrement() because reindex_relation does it.
	 */
	reindex_relation(OIDOldHeap, false, true);

	/* Destroy new heap with old filenode */
	object.classId = RelationRelationId;
	object.objectId = OIDNewHeap;
	object.objectSubId = 0;

	/*
	 * The new relation is local to our transaction and we know nothing
	 * depends on it, so DROP_RESTRICT should be OK.
	 */
	performDeletion(&object, DROP_RESTRICT);

	/* performDeletion does CommandCounterIncrement at end */

	/*
	 * Now we must remove any relation mapping entries that we set up for the
	 * transient table, as well as its toast table and toast index if any. If
	 * we fail to do this before commit, the relmapper will complain about new
	 * permanent map entries being added post-bootstrap.
	 */
	for (i = 0; OidIsValid(mapped_tables[i]); i++)
		RelationMapRemoveMapping(mapped_tables[i]);

	/*
	 * At this point, everything is kosher except that, if we did toast swap
	 * by links, the toast table's name corresponds to the transient table.
	 * The name is irrelevant to the backend because it's referenced by OID,
	 * but users looking at the catalogs could be confused.  Rename it to
	 * prevent this problem.
	 *
	 * Note no lock required on the relation, because we already hold an
	 * exclusive lock on it.
	 */
	if (!swap_toast_by_content)
	{
		Relation	newrel;

		newrel = heap_open(OIDOldHeap, NoLock);
		if (OidIsValid(newrel->rd_rel->reltoastrelid))
		{
			Relation	toastrel;
			Oid			toastidx;
			Oid			toastnamespace;
			char		NewToastName[NAMEDATALEN];

			toastrel = relation_open(newrel->rd_rel->reltoastrelid,
									 AccessShareLock);
			toastidx = toastrel->rd_rel->reltoastidxid;
			toastnamespace = toastrel->rd_rel->relnamespace;
			relation_close(toastrel, AccessShareLock);

			/* rename the toast table ... */
			snprintf(NewToastName, NAMEDATALEN, "pg_toast_%u",
					 OIDOldHeap);
			RenameRelationInternal(newrel->rd_rel->reltoastrelid,
								   NewToastName,
								   toastnamespace);

			/* ... and its index too */
			snprintf(NewToastName, NAMEDATALEN, "pg_toast_%u_index",
					 OIDOldHeap);
			RenameRelationInternal(toastidx,
								   NewToastName,
								   toastnamespace);
		}
		relation_close(newrel, NoLock);
	}
}


/*
 * Get a list of tables that the current user owns and
 * have indisclustered set.  Return the list in a List * of rvsToCluster
 * with the tableOid and the indexOid on which the table is already
 * clustered.
 */
static List *
get_tables_to_cluster(MemoryContext cluster_context)
{
	Relation	indRelation;
	HeapScanDesc scan;
	ScanKeyData entry;
	HeapTuple	indexTuple;
	Form_pg_index index;
	MemoryContext old_context;
	RelToCluster *rvtc;
	List	   *rvs = NIL;

	/*
	 * Get all indexes that have indisclustered set and are owned by
	 * appropriate user. System relations or nailed-in relations cannot ever
	 * have indisclustered set, because CLUSTER will refuse to set it when
	 * called with one of them as argument.
	 */
	indRelation = heap_open(IndexRelationId, AccessShareLock);
	ScanKeyInit(&entry,
				Anum_pg_index_indisclustered,
				BTEqualStrategyNumber, F_BOOLEQ,
				BoolGetDatum(true));
	scan = heap_beginscan(indRelation, SnapshotNow, 1, &entry);
	while ((indexTuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		index = (Form_pg_index) GETSTRUCT(indexTuple);

		if (!pg_class_ownercheck(index->indrelid, GetUserId()))
			continue;

		/*
		 * We have to build the list in a different memory context so it will
		 * survive the cross-transaction processing
		 */
		old_context = MemoryContextSwitchTo(cluster_context);

		rvtc = (RelToCluster *) palloc(sizeof(RelToCluster));
		rvtc->tableOid = index->indrelid;
		rvtc->indexOid = index->indexrelid;
		rvs = lcons(rvtc, rvs);

		MemoryContextSwitchTo(old_context);
	}
	heap_endscan(scan);

	relation_close(indRelation, AccessShareLock);

	return rvs;
}
