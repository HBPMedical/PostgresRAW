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


// FIXME: Document usage of memory arena here and in NoDBCacheWorld.

#include "noDB/NoDBScanStrategy.h"

#include "noDB/auxiliary/NoDBCol.h"
#include "noDB/auxiliary/NoDBLimits.h"
#include "noDB/auxiliary/NoDBMalloc.h"
#include "noDB/auxiliary/NoDBMap.h"
#include "noDB/auxiliary/NoDBList.h"


#define NODB_CACHE_DEFAULT_ROWS         10000                      // Default number of rows to add to a cache.
#define NODB_DATA_CACHE_DEFAULT_SIZE	256							// Default size (width in bytes) of a data cache.
#define NODB_POS_CACHE_DEFAULT_SIZE		(sizeof(NoDBPMPair_t) * 16)	// Default size (width in bytes) of a positions cache.

//#define NODB_CACHE_DEFAULT_ROWS         500000                      // Default number of rows to add to a cache.
//#define NODB_DATA_CACHE_DEFAULT_SIZE	256							// Default size (width in bytes) of a data cache.
//#define NODB_POS_CACHE_DEFAULT_SIZE		(sizeof(NoDBPMPair_t) * 16)	// Default size (width in bytes) of a positions cache.

NoDBList_t *NoDBCaches = NULL;

typedef enum Type_t {
	UNKNOWN,
	CACHE_HIT,							// Row/Column present in a cache.
	CACHE_MISS,							// Row/Column missing from a cache but will not be added.
	CACHE_ADD,							// Row/Column missing from a cache but will be added.
} Type_t;

typedef struct Node_t {
	Type_t		dataHit;				// Data cache hit, miss or to be added.
	NoDBCache_t	*dataCache;				// Data cache pointer (in case of CACHE_HIT or CACHE_ADD).
	Type_t		pmHit;					// Positions cache hit, miss or to be added.
	NoDBCache_t	*pmCache;				// Pointer to positions cache (in case of CACHE_ADD only).
	NoDBCache_t	*closestPMCache;		// Pointer to closest PM cache.
	NoDBCol_t	closestColumn;			// Closest column.
	int			distance;				// Distance to closest column.
} Node_t;

typedef struct ColDistance_t {
	int			forwardDistance;
	int			backwardDistance;
	NoDBCache_t	*cache;
} ColDistance_t;

/*
 ============
 AddColToMap
 ============
*/
static void AddColToMap(NoDBMap_t *map, NoDBCache_t *cache, NoDBCol_t col)
{
	NoDBColList_t	*colList;

	if (NoDBMapGetPtrPtr(map, cache, (void **) &colList)) {
		colList = NoDBColListArenaAdd(colList, NODB_ARENA_STRATEGY, col);
		NoDBMapSetPtrPtr(map, cache, colList);
	} else {
		colList = NoDBColListArenaAdd(NULL, NODB_ARENA_STRATEGY, col);
		NoDBMapArenaAddPtrPtr(map, NODB_ARENA_STRATEGY, cache, colList);
	}
}

/*
 ============
 AddUniqueColToMap

 Returns True if col was unique.
 ============
*/
static int AddUniqueColToMap(NoDBMap_t *map, NoDBCache_t *cache, NoDBCol_t col)
{
	NoDBColList_t	*colList;
	NoDBColList_t	*ncolList;

	if (NoDBMapGetPtrPtr(map, cache, (void **) &colList)) {
		ncolList = NoDBColListArenaAddUnique(colList, NODB_ARENA_STRATEGY, col);
		NoDBMapSetPtrPtr(map, cache, ncolList);
		return colList != ncolList;
	} else {
		ncolList = NoDBColListArenaAdd(NULL, NODB_ARENA_STRATEGY, col);
		NoDBMapArenaAddPtrPtr(map, NODB_ARENA_STRATEGY, cache, ncolList);
		return 1;
	}
}

/*
 ============
 MapContainsColumn
 ============
*/
int MapContainsColumn(NoDBMap_t *map, NoDBCol_t col)
{
	NoDBMapIterator_t	*it;

	for (it = NoDBMapIterator(map); it; it = NoDBMapIteratorNext(map, it)) {
		NoDBColList_t	*colList;

		colList = NoDBMapIteratorValuePtr(map, it);
		if (NoDBColListContains(colList, col)) {
			return 1;
		}
	}
	return 0;
}

/*
 ============
 AddIntToMap
 ============
*/
static void AddIntToMap(NoDBMap_t *map, NoDBCache_t *cache, int v)
{
	NoDBIntList_t	*intList;

	if (NoDBMapGetPtrPtr(map, cache, (void **) &intList)) {
		intList = NoDBIntListArenaAdd(intList, NODB_ARENA_STRATEGY, v);
		NoDBMapSetPtrPtr(map, cache, intList);
	} else {
		intList = NoDBIntListArenaAdd(NULL, NODB_ARENA_STRATEGY, v);
		NoDBMapArenaAddPtrPtr(map, NODB_ARENA_STRATEGY, cache, intList);
	}
}

/*
 ============
 SetColsPerCache
 ============
*/
static void SetColsPerCache(NoDBMap_t *map, int *n, NoDBColumnsPerCache_t **cacheCols)
{
	*n = NoDBMapSize(map);
	if (*n) {
		NoDBMapIterator_t	*it;
		int					i;

		*cacheCols = NoDBArenaAlloc(NODB_ARENA_ITERATOR, sizeof(NoDBColumnsPerCache_t) * (*n));
		i = 0;
		for (it = NoDBMapIterator(map); it; it = NoDBMapIteratorNext(map, it)) {
			NoDBCache_t		*cache;
			NoDBColList_t	*colList;

			cache = NoDBMapIteratorKeyPtr(map, it);
			colList = NoDBMapIteratorValuePtr(map, it);

			(*cacheCols)[i].cache = cache;
			(*cacheCols)[i].cols = NoDBColVectorArenaFromList(colList, NODB_ARENA_ITERATOR);

			i++;
		}
	}
}

/*
 ============
 GetColListFromBitmap

 Convert columns in a vector and bitmap to a list.
 ============
*/
NoDBColList_t	*GetColListFromBitmap(NoDBColVector_t cols, NoDBBitmap_t *bitmap)
{
	NoDBColList_t	*colList;
	int				i;
	int				ncols;

	colList = NULL;
	ncols = NoDBColVectorSize(cols);
	for (i = 0; i < ncols; i++) {
		if (NoDBBitmapIsSet(bitmap, i)) {
			colList = NoDBColListArenaAddUnique(colList, NODB_ARENA_STRATEGY, NoDBColVectorGet(cols, i));
		}
	}
	return colList;
}

/*
 ============
 GetCachesWithRow

 Return list of caches that have the given row in use.
 ============
*/
NoDBList_t	*GetCachesWithRow(NoDBRelation_t *rel, NoDBCacheType_t cacheType, NoDBRow_t row)
{
	NoDBList_t	*caches;
	NoDBList_t	*allCaches;

	caches = NULL;
	for (allCaches = NoDBCaches; allCaches; allCaches = allCaches->next) {
		NoDBCache_t	*cache;

		cache = allCaches->ptr;
		if (NoDBCacheGetRelation(cache) == rel && NoDBCacheGetType(cache) == cacheType && NoDBCacheHasRow(cache, row)) {
			caches = NoDBListArenaAdd(caches, NODB_ARENA_STRATEGY, cache);
		}
	}
	return caches;
}

/*
 ============
 GetBestCacheHit

 Choose best cache to read data from.
 The algorithm is greedy: it chooses the cache with most columns in the query.
 ============
*/
NoDBCache_t	*GetBestCacheHit(NoDBList_t *caches, NoDBCol_t hitCol, NoDBColList_t *colList, NoDBColList_t **bestCacheCols)
{
	NoDBCache_t		*bestCache;
	int				bestN;

	bestCache = NULL;
	bestN = 0;
	for (; caches; caches = caches->next) {
		NoDBCache_t		*cache;
		NoDBColList_t	*cacheCols;
		int				n;
		NoDBColList_t	*cols;

		cache = caches->ptr;
		if (!NoDBCacheHasColumn(cache, hitCol)) {
			continue;
		}

		cacheCols = NoDBColListArenaAdd(NULL, NODB_ARENA_STRATEGY, hitCol);;
		n = 1;		// n must be set to 1 since hitCol may not be part of colList.
					// colList is the read set, but hitCol may be only in the write set.
		for (cols = colList; cols; cols = cols->next) {
			NoDBCol_t	col;

			col = cols->col;
			if (NoDBCacheHasColumn(cache, col)) {
				cacheCols = NoDBColListArenaAddUnique(cacheCols, NODB_ARENA_STRATEGY, col);
				n++;
			}
		}

		if (n > bestN) {
			bestCache = cache;
			bestN = n;
			*bestCacheCols = cacheCols;
		}
	}
	return bestCache;
}

/*
 ============
 CacheContainsSubset

 Returns true if the columns in the cache are a subset of columns passed as an argument.
 ============
*/
int CacheContainsSubset(NoDBCache_t *cache, NoDBColList_t *cols)
{
	int i;
	for (i = 0; i < cache->usedCols; i++) {
		NoDBCol_t	col;

		col = cache->cols[i];
		if (!NoDBColListContains(cols, col)) {
			return 0;
		}
	}
	return 1;
}

/*
 ============
 CacheStartsBelow

 Returns true if there is a cache including any of the columns passed as an argument and which starts under the given row.
 ============
*/
int CacheStartsBelow(NoDBRelation_t *rel, NoDBCacheType_t cacheType, NoDBColList_t *cols, NoDBRow_t row)
{
	NoDBList_t	*allCaches;

	for (allCaches = NoDBCaches; allCaches; allCaches = allCaches->next) {
		NoDBCache_t	*cache;

		cache = allCaches->ptr;
		if (NoDBCacheGetRelation(cache) == rel && NoDBCacheGetType(cache) == cacheType && NoDBCacheGetBegin(cache) >= row) {
			NoDBColList_t	*list;

			for (list = cols; list; list = list->next) {
				NoDBCol_t	col;

				col = list->col;
				if (NoDBCacheHasColumn(cache, col)) {
					return 1;
				}
			}
		}
	}
	return 0;
}

/*
 ============
 GetCachesCanGrow

 Returns list of caches that can grow until the given row and include a subset of columns passed as an argument.
 ============
*/
NoDBList_t *GetCachesCanGrow(NoDBRelation_t *rel, NoDBCacheType_t cacheType, NoDBCol_t col, NoDBRow_t row, NoDBColList_t *cols)
{
	NoDBList_t	*caches;
	NoDBList_t	*allCaches;

	if (row == 0) {
		return NULL;
	}

	caches = NULL;
	for (allCaches = NoDBCaches; allCaches; allCaches = allCaches->next) {
		NoDBCache_t	*cache;

		cache = allCaches->ptr;
		if (NoDBCacheGetRelation(cache) == rel && NoDBCacheGetType(cache) == cacheType && NoDBCacheCanGrow(cache, row) && NoDBCacheHasColumn(cache, col) && CacheContainsSubset(cache, cols) && !CacheStartsBelow(rel, cacheType, cols, NoDBCacheGetBegin(cache) + NoDBCacheGetUsedRows(cache))) {
			caches = NoDBListArenaAdd(caches, NODB_ARENA_STRATEGY, cache);
		}
	}
	return caches;
}

/*
 ============
 GetBestCacheToGrow

 Choose best cache to grow down.
 The algorithm chooses the cache with most columns in the query.
 ============
*/
NoDBCache_t	*GetBestCacheToGrow(NoDBList_t *caches, NoDBColList_t *colList, NoDBColList_t **bestCacheCols)
{
	NoDBCache_t		*bestCache;
	int				bestN;

	bestCache = NULL;
	bestN = 0;
	for (; caches; caches = caches->next) {
		NoDBCache_t		*cache;
		NoDBColList_t	*cacheCols;
		int				n;
		NoDBColList_t	*cols;

		cache = caches->ptr;
		cacheCols = NULL;
		n = 0;
		for (cols = colList; cols; cols = cols->next) {
			NoDBCol_t	col;

			col = cols->col;
			if (NoDBCacheHasColumn(cache, col)) {
				cacheCols = NoDBColListArenaAdd(cacheCols, NODB_ARENA_STRATEGY, col);
				n++;
			}
		}

		if (n > bestN) {
			bestCache = cache;
			bestN = n;
			*bestCacheCols = cacheCols;
		}
	}
	return bestCache;
}

/*
 ============
 GetCachesFreeColumns

 Returns list of caches that start in the this row and have sufficient free space.
 ============
*/
NoDBList_t *GetCachesFreeColumns(NoDBRelation_t *rel, NoDBCacheType_t cacheType, NoDBRow_t limit, NoDBRow_t row, unsigned int size)
{
	NoDBList_t	*caches;
	NoDBList_t	*allCaches;

	caches = NULL;
	for (allCaches = NoDBCaches; allCaches; allCaches = allCaches->next) {
		NoDBCache_t	*cache;

		cache = allCaches->ptr;
		if (NoDBCacheGetRelation(cache) == rel && NoDBCacheGetType(cache) == cacheType && NoDBCacheGetBegin(cache) == row && NoDBCacheGetFreeSize(cache) >= size &&
			(!limit || (NoDBCacheGetBegin(cache) + NoDBCacheGetUsedRows(cache)) <= limit)) {
			caches = NoDBListArenaAdd(caches, NODB_ARENA_STRATEGY, cache);
		}
	}
	return caches;
}

/*
 ============
 GetBestCacheToAddColumn

 Choose best cache to add a column to.
 The algorithm chooses the cache with most free space.
============
*/
NoDBCache_t *GetBestCacheToAddColumn(NoDBList_t *caches)
{
	NoDBCache_t		*bestCache;
	unsigned int	bestFreeSpace;

	bestCache = NULL;
	bestFreeSpace = 0;
	for (; caches; caches = caches->next) {
		NoDBCache_t		*cache;
		unsigned int	freeSpace;

		cache = caches->ptr;
		freeSpace = NoDBCacheGetFreeSize(cache);
		if (freeSpace > bestFreeSpace) {
			bestCache = cache;
			bestFreeSpace = freeSpace;
		}
	}
	return bestCache;
}

//static int abs(int a)
//{
//	return (a > 0) ? a : -a;
//}

/*
 ============
 GrowToLastRow

 Make sure that all caches being written to are big enough.
 ============
*/
void GrowToLastRow(Node_t **matrix, int lastRowIdx, NoDBRow_t lastRow, int ncols)
{
	int	colIdx;

	for (colIdx = 0; colIdx < ncols; colIdx++) {
		if (matrix[lastRowIdx][colIdx].dataHit == CACHE_ADD) {
			NoDBCacheGrow(matrix[lastRowIdx][colIdx].dataCache, lastRow);
		}
		if (matrix[lastRowIdx][colIdx].pmHit == CACHE_ADD) {
			NoDBCacheGrow(matrix[lastRowIdx][colIdx].pmCache, lastRow);
		}
	}
}

/*
 ============
 BuildMatrix
 ============
*/
Node_t **BuildMatrix(NoDBScanStrategyIterator_t *it)
{
	Node_t			**matrix;
	int				nrows;
	int				ncols;
	int				rowIdx;

	nrows = NoDBRowVectorSize(it->queryInfo.rows);
	ncols = NoDBColVectorSize(it->queryInfo.cols);

	matrix = NoDBArenaAlloc(NODB_ARENA_STRATEGY, sizeof(Node_t *) * nrows);
	for (rowIdx = 0; rowIdx < nrows; rowIdx++) {
		int			colIdx;

		matrix[rowIdx] = NoDBArenaAlloc(NODB_ARENA_STRATEGY, sizeof(Node_t) * ncols);
		for (colIdx = 0; colIdx < ncols; colIdx++) {
			matrix[rowIdx][colIdx].dataHit = UNKNOWN;
			matrix[rowIdx][colIdx].pmHit = UNKNOWN;
			matrix[rowIdx][colIdx].closestPMCache = NULL;
		}
	}

	// Mark cache hits in the matrix.
	for (rowIdx = 0; rowIdx < nrows - 1; rowIdx++) {
		NoDBRow_t	row;
		NoDBList_t	*dataCachesWithRow;		// List of data caches containing the current row.
		NoDBList_t	*pmCachesWithRow;		// List of position caches containing the current row.
		int			colIdx;

		row = NoDBRowVectorGet(it->queryInfo.rows, rowIdx);

		// The caches containing the row have to be read now (otherwise we'd get fake "hits" due to caches growing down).
		// FIXME: This will need to change when i change the other stuff for the iterator. It will go within the loop below.
		dataCachesWithRow = GetCachesWithRow(it->queryInfo.rel, NODB_DATA_CACHE, row);
		pmCachesWithRow = GetCachesWithRow(it->queryInfo.rel, NODB_POSITIONS_CACHE, row);

		for (colIdx = 0; colIdx < ncols; colIdx++) {

			// Handle data caches.
			if (matrix[rowIdx][colIdx].dataHit == UNKNOWN) {
				NoDBCol_t		col;
				NoDBCache_t		*cacheHit;
				NoDBColList_t	*cacheHitCols;

				col = NoDBColVectorGet(it->queryInfo.cols, colIdx);

				// Get best cache hit.
				cacheHit = GetBestCacheHit(dataCachesWithRow, col, it->queryInfo.readCols, &cacheHitCols);

				if (cacheHit) {
					// Mark matrix hits for all columns/rows in the cache.
					int nrowIdx;

					nrowIdx = rowIdx;
					while (NoDBCacheHasRow(cacheHit, NoDBRowVectorGet(it->queryInfo.rows, nrowIdx))) {
						for (; cacheHitCols; cacheHitCols = cacheHitCols->next) {
							NoDBCol_t	ncol;
							int			ncolIdx;

							ncol = cacheHitCols->col;

							ncolIdx = NoDBColVectorBinarySearch(it->queryInfo.cols, ncol);

							matrix[nrowIdx][ncolIdx].dataHit = CACHE_HIT;
							matrix[nrowIdx][ncolIdx].dataCache = cacheHit;
						}
						nrowIdx++;
					}
				}
			}

			// Handle position caches.
			if (matrix[rowIdx][colIdx].pmHit == UNKNOWN && (matrix[rowIdx][colIdx].dataHit != CACHE_HIT || NoDBBitmapIsSet(it->queryInfo.writePositionsBitmap, colIdx))) {
				NoDBCol_t		col;
				NoDBCache_t		*cacheHit;
				NoDBColList_t	*cacheHitCols;

				col = NoDBColVectorGet(it->queryInfo.cols, colIdx);

				// Get best cache hit.
				cacheHit = GetBestCacheHit(pmCachesWithRow, col, it->queryInfo.readCols, &cacheHitCols);
				if (cacheHit) {
					// Mark matrix hits for all columns/rows in the cache.
					int nrowIdx;

					nrowIdx = rowIdx;
					while (NoDBCacheHasRow(cacheHit, NoDBRowVectorGet(it->queryInfo.rows, nrowIdx))) {
						for (; cacheHitCols; cacheHitCols = cacheHitCols->next) {
							NoDBCol_t	ncol;
							int			ncolIdx;

							ncol = cacheHitCols->col;
							ncolIdx = NoDBColVectorBinarySearch(it->queryInfo.cols, ncol);

							matrix[nrowIdx][ncolIdx].pmHit = CACHE_HIT;
							matrix[nrowIdx][ncolIdx].closestPMCache = cacheHit;
							matrix[nrowIdx][ncolIdx].closestColumn = ncol;
							matrix[nrowIdx][ncolIdx].distance = 0;
						}
						nrowIdx++;
					}
				}
			}
		}
	}

	// Handle cache misses in the matrix.
	for (rowIdx = 0; rowIdx < nrows - 1; rowIdx++) {
		NoDBRow_t		row;
		NoDBColList_t	*growDataCandidates;
		NoDBColList_t	*growPMCandidates;
		int				colIdx;

		row = NoDBRowVectorGet(it->queryInfo.rows, rowIdx);

		// Build list of columns that can grow down (i.e. the write set minus the cache hits).
		growDataCandidates = NULL;
		growPMCandidates = NULL;
		for (colIdx = 0; colIdx < ncols; colIdx++) {
			if (NoDBBitmapIsSet(it->queryInfo.writeDataBitmap, colIdx) && matrix[rowIdx][colIdx].dataHit != CACHE_HIT) {
				NoDBCol_t col;

				col = NoDBColVectorGet(it->queryInfo.cols, colIdx);
				growDataCandidates = NoDBColListArenaAdd(growDataCandidates, NODB_ARENA_STRATEGY, col);
			}
			if (NoDBBitmapIsSet(it->queryInfo.writePositionsBitmap, colIdx) && matrix[rowIdx][colIdx].pmHit != CACHE_HIT) {
				NoDBCol_t col;

				col = NoDBColVectorGet(it->queryInfo.cols, colIdx);
				growPMCandidates = NoDBColListArenaAdd(growPMCandidates, NODB_ARENA_STRATEGY, col);
			}
		}

		for (colIdx = 0; colIdx < ncols; colIdx++) {

			// Handle data caches.
			if (matrix[rowIdx][colIdx].dataHit == UNKNOWN) {
				NoDBCol_t		col;

				col = NoDBColVectorGet(it->queryInfo.cols, colIdx);

				// Check if part of write set.
				if (NoDBBitmapIsSet(it->queryInfo.writeDataBitmap, colIdx)) {
					NoDBList_t		*cachesToGrow;
					NoDBCache_t		*cacheToGrow;
					NoDBColList_t	*cacheGrowCols;

					// Build list of caches that can grow down to this row.
					cachesToGrow = GetCachesCanGrow(it->queryInfo.rel, NODB_DATA_CACHE, col, row, growDataCandidates);

					// Get best cache to grow (i.e. the one with most columns in the write set).
					cacheToGrow = GetBestCacheToGrow(cachesToGrow, growDataCandidates, &cacheGrowCols);

					if (cacheToGrow) {
						// Grow cache down and mark matrix hits for all columns/rows in the cache.
						NoDBCacheGrow(cacheToGrow, row);

						for (; cacheGrowCols; cacheGrowCols = cacheGrowCols->next) {
							NoDBCol_t	ncol;
							int			ncolIdx;

							ncol = cacheGrowCols->col;
							ncolIdx = NoDBColVectorBinarySearch(it->queryInfo.cols, ncol);
							matrix[rowIdx][ncolIdx].dataHit = CACHE_ADD;
							matrix[rowIdx][ncolIdx].dataCache = cacheToGrow;
						}
					} else {
						unsigned int	size;
						NoDBList_t 		*cachesFreeCols;
						NoDBCache_t		*cacheToAddCol;

						// Build list of caches in which we can add this column.
						size = NoDBRelationGetColumnSize(it->queryInfo.rel, col);
						cachesFreeCols = GetCachesFreeColumns(it->queryInfo.rel, NODB_DATA_CACHE, it->queryInfo.limit, row, size);

						// Get best cache to add column
						cacheToAddCol = GetBestCacheToAddColumn(cachesFreeCols);

						if (cacheToAddCol) {
							// Add column to cache and mark matrix hits for all columns/rows in the cache.
							int nrowIdx;

							NoDBCacheAddColumn(cacheToAddCol, col, size);

							nrowIdx = rowIdx;
							do {
								matrix[nrowIdx][colIdx].dataHit = CACHE_ADD;
								matrix[nrowIdx][colIdx].dataCache = cacheToAddCol;
								nrowIdx++;
							} while (NoDBCacheHasRow(cacheToAddCol, NoDBRowVectorGet(it->queryInfo.rows, nrowIdx)));
						} else {
							NoDBCache_t	*newCache;

							// Create new cache.
							newCache = NoDBCacheInit(it->queryInfo.rel, NODB_DATA_CACHE, row, NODB_DATA_CACHE_DEFAULT_SIZE);
							NoDBCacheAddColumn(newCache, col, size);

							matrix[rowIdx][colIdx].dataHit = CACHE_ADD;
							matrix[rowIdx][colIdx].dataCache = newCache;

							// Add to global list of caches.
							NoDBCaches = NoDBListAdd(NoDBCaches, newCache);
						}
					}
				} else {
					matrix[rowIdx][colIdx].dataHit = CACHE_MISS;
				}
			}

			// Handle position caches.
			if (matrix[rowIdx][colIdx].pmHit == UNKNOWN && (matrix[rowIdx][colIdx].dataHit != CACHE_HIT || NoDBBitmapIsSet(it->queryInfo.writePositionsBitmap, colIdx))) {
				NoDBCol_t		col;

				col = NoDBColVectorGet(it->queryInfo.cols, colIdx);

				// Check if part of write set.
				if (NoDBBitmapIsSet(it->queryInfo.writePositionsBitmap, colIdx)) {
					NoDBList_t		*cachesToGrow;
					NoDBCache_t		*cacheToGrow;
					NoDBColList_t	*cacheGrowCols;

					// Build list of caches that can grow down to this row.
					cachesToGrow = GetCachesCanGrow(it->queryInfo.rel, NODB_POSITIONS_CACHE, col, row, growPMCandidates);

					// Get best cache to grow (i.e. the one with most columns in the write set).
					cacheToGrow = GetBestCacheToGrow(cachesToGrow, growPMCandidates, &cacheGrowCols);

					if (cacheToGrow) {
						// Grow cache down and mark matrix hits for all columns/rows in the cache.
						NoDBCacheGrow(cacheToGrow, row);

						for (; cacheGrowCols; cacheGrowCols = cacheGrowCols->next) {
							NoDBCol_t	ncol;
							int			ncolIdx;

							ncol = cacheGrowCols->col;
							ncolIdx = NoDBColVectorBinarySearch(it->queryInfo.cols, ncol);
							matrix[rowIdx][ncolIdx].pmHit = CACHE_ADD;
							matrix[rowIdx][ncolIdx].pmCache = cacheToGrow;
						}
					} else {
						unsigned int	size;
						NoDBList_t 		*cachesFreeCols;
						NoDBCache_t		*cacheToAddCol;

						// Build list of caches in which we can add this column.
						size = sizeof(NoDBPMPair_t);
						cachesFreeCols = GetCachesFreeColumns(it->queryInfo.rel, NODB_POSITIONS_CACHE, it->queryInfo.limit, row, size);

						// Get best cache to add column
						cacheToAddCol = GetBestCacheToAddColumn(cachesFreeCols);

						if (cacheToAddCol) {
							// Add column to cache and mark matrix hits for all columns/rows in the cache.
							int nrowIdx;

							NoDBCacheAddColumn(cacheToAddCol, col, size);

							nrowIdx = rowIdx;
							do {

								matrix[nrowIdx][colIdx].pmHit = CACHE_ADD;
								matrix[nrowIdx][colIdx].pmCache = cacheToAddCol;
								nrowIdx++;
							} while (NoDBCacheHasRow(cacheToAddCol, NoDBRowVectorGet(it->queryInfo.rows, nrowIdx)));
						} else {
							NoDBCache_t	*newCache;

							// Create new cache.
							newCache = NoDBCacheInit(it->queryInfo.rel, NODB_POSITIONS_CACHE, row, NODB_POS_CACHE_DEFAULT_SIZE);
							NoDBCacheAddColumn(newCache, col, size);

							matrix[rowIdx][colIdx].pmHit = CACHE_ADD;
							matrix[rowIdx][colIdx].pmCache = newCache;

							// Add to global list of caches.
							NoDBCaches = NoDBListAdd(NoDBCaches, newCache);
						}
					}
				} else {
					matrix[rowIdx][colIdx].pmHit = CACHE_MISS;
				}
			}
		}
	}

	// Find closest position for every column that was not a CACHE_HIT in any positional map.
	for (rowIdx = 0; rowIdx < nrows - 1; rowIdx++) {
		NoDBRow_t		row;
		NoDBColList_t	*excludeCols;
		int				colIdx;

		row = NoDBRowVectorGet(it->queryInfo.rows, rowIdx);

		// Build list of columns to exclude, i.e. the columns marked as CACHE_ADD (not yet written).
		excludeCols = NULL;
		for (colIdx = 0; colIdx < ncols; colIdx++) {
			if (matrix[rowIdx][colIdx].pmHit == CACHE_ADD) {
				NoDBCol_t col;

				col = NoDBColVectorGet(it->queryInfo.cols, colIdx);
				excludeCols = NoDBColListArenaAdd(excludeCols, NODB_ARENA_STRATEGY, col);
			}
		}

		// Find cache with closest column.
		for (colIdx = 0; colIdx < ncols; colIdx++) {
			NoDBCol_t	col;

			col = NoDBColVectorGet(it->queryInfo.cols, colIdx);
			if (matrix[rowIdx][colIdx].pmHit != CACHE_HIT) {
				NoDBCache_t *bestCache;
				int			bestDistance;
				NoDBCol_t	closestCol;
				int			first;
				NoDBList_t	*allCaches;

				bestCache = NULL;
				first = 1;
				for (allCaches = NoDBCaches; allCaches; allCaches = allCaches->next) {
					NoDBCache_t	*cache;

					cache = allCaches->ptr;
					if (NoDBCacheGetRelation(cache) == it->queryInfo.rel && NoDBCacheGetType(cache) == NODB_POSITIONS_CACHE && NoDBCacheHasRow(cache, row)) {
						int i;

						for (i = 0; i < cache->usedCols; i++) {
							int d;
							if (NoDBColListContains(excludeCols, cache->cols[i])) {
								continue;
							}

							d = col - cache->cols[i];
							if (first || abs(d) < abs(bestDistance)) {
								bestCache = cache;
								bestDistance = d;
								closestCol = cache->cols[i];
								first = 0;
							}
						}
					}
				}

				if (bestCache) {
					matrix[rowIdx][colIdx].closestPMCache = bestCache;
					matrix[rowIdx][colIdx].closestColumn = closestCol;
					matrix[rowIdx][colIdx].distance = bestDistance;
				}
			}
		}
	}

	// Grow until row before last.
	GrowToLastRow(matrix, nrows - 2, NoDBRowVectorGet(it->queryInfo.rows, nrows - 1) - 1, ncols);
	return matrix;
}

typedef struct {
	NoDBMap_t		*mapDataHit;
	NoDBMap_t		*mapDataAddByValue;
	NoDBMap_t		*mapDataAddByRef;
	NoDBColList_t	*convert;				// List of unique columns to convert (i.e. extract).
	NoDBColList_t	*collectFromFile;		// List of unique columns to collect from the file.
	NoDBMap_t		*mapPositionsAdd;
	NoDBMap_t		*mapReadFromPM;
	NoDBReadViaPM_t	*readViaPM;
	int				nreadViaPM;
} FillRowReply_t;

FillRowReply_t FillRow(Node_t **matrix, int rowIdx, NoDBRelation_t *rel, NoDBColVector_t cols, NoDBBitmap_t *readBitmap, NoDBBitmap_t *writeDataBitmap, NoDBBitmap_t *writePositionsBitmap)
{
	FillRowReply_t	r;
	int				pmAvailable;
	int				colIdx;

	r.mapDataHit = NoDBMapArenaInit(NODB_ARENA_STRATEGY);
	r.mapDataAddByValue = NoDBMapArenaInit(NODB_ARENA_STRATEGY);
	r.mapDataAddByRef = NoDBMapArenaInit(NODB_ARENA_STRATEGY);
	r.convert = NULL;
	r.collectFromFile = NULL;
	r.mapPositionsAdd = NoDBMapArenaInit(NODB_ARENA_STRATEGY);
	r.mapReadFromPM = NoDBMapArenaInit(NODB_ARENA_STRATEGY);
	r.readViaPM = NULL;
	r.nreadViaPM = 0;

	// Check if there is any positional map to use.
	pmAvailable = 0;
	for (colIdx = 0; colIdx < NoDBColVectorSize(cols); colIdx++) {
		if (matrix[rowIdx][colIdx].closestPMCache) {
			pmAvailable = 1;
			break;
		}
	}

	if (!pmAvailable) {		// There are no positional maps.
		for (colIdx = 0; colIdx < NoDBColVectorSize(cols); colIdx++) {
			NoDBCol_t		col;

			col = NoDBColVectorGet(cols, colIdx);
			if (NoDBBitmapIsSet(readBitmap, colIdx)) {
				if (matrix[rowIdx][colIdx].dataHit == CACHE_HIT) {
					// Include data hits in the strategy.
					AddColToMap(r.mapDataHit, matrix[rowIdx][colIdx].dataCache, col);
				} else {
					// Make sure position is collected from the file.
					r.collectFromFile = NoDBColListArenaAddUnique(r.collectFromFile, NODB_ARENA_STRATEGY, col);
					// Make sure data is extracted/converted.
					r.convert = NoDBColListArenaAddUnique(r.convert, NODB_ARENA_STRATEGY, col);
				}
			}
			if (NoDBBitmapIsSet(writeDataBitmap, colIdx) && matrix[rowIdx][colIdx].dataHit == CACHE_ADD) {
				// Make sure position is collected from the file.
				r.collectFromFile = NoDBColListArenaAddUnique(r.collectFromFile, NODB_ARENA_STRATEGY, col);
				// Make sure data is extracted/converted.
				r.convert = NoDBColListArenaAddUnique(r.convert, NODB_ARENA_STRATEGY, col);
				// Make sure data is written to data cache.
				if (NoDBRelationIsColumnByValue(rel, col)) {
					AddColToMap(r.mapDataAddByValue, matrix[rowIdx][colIdx].dataCache, col);
				} else {
					AddColToMap(r.mapDataAddByRef, matrix[rowIdx][colIdx].dataCache, col);
				}
			}
			if (NoDBBitmapIsSet(writePositionsBitmap, colIdx) && matrix[rowIdx][colIdx].pmHit == CACHE_ADD) {
				// Make sure position is collected from the file.
				r.collectFromFile = NoDBColListArenaAddUnique(r.collectFromFile, NODB_ARENA_STRATEGY, col);
				// Make sure position is written to position cache.
				AddColToMap(r.mapPositionsAdd, matrix[rowIdx][colIdx].pmCache, col);
			}
		}
	} else {						// There are positional maps.
		NoDBIntList_t	*collectFromPMIdx;	// List of unique column indexes to collect from positional maps.
		ColDistance_t	*colDistances;
		NoDBIntList_t	*list;
		int				n;

		collectFromPMIdx = NULL;
		colDistances = NoDBArenaAlloc(NODB_ARENA_STRATEGY, NoDBRelationGetNumberColumns(rel) * sizeof(ColDistance_t));

		for (colIdx = 0; colIdx < NoDBColVectorSize(cols); colIdx++) {
			NoDBCol_t		col;

			col = NoDBColVectorGet(cols, colIdx);
			if (NoDBBitmapIsSet(readBitmap, colIdx)) {
				if (matrix[rowIdx][colIdx].dataHit == CACHE_HIT) {
					// Include data hits in the strategy.
					AddColToMap(r.mapDataHit, matrix[rowIdx][colIdx].dataCache, col);
				} else {
					// Make sure position is collected from a positional map.
					collectFromPMIdx = NoDBIntListArenaAddUnique(collectFromPMIdx, NODB_ARENA_STRATEGY, colIdx);
					// Make sure data is extracted/converted.
					r.convert = NoDBColListArenaAddUnique(r.convert, NODB_ARENA_STRATEGY, col);
				}
			}
			if (NoDBBitmapIsSet(writeDataBitmap, colIdx) && matrix[rowIdx][colIdx].dataHit == CACHE_ADD) {
				// Make sure position is collected from a positional map.
				collectFromPMIdx = NoDBIntListArenaAddUnique(collectFromPMIdx, NODB_ARENA_STRATEGY, colIdx);
				// Make sure data is extracted/converted.
				r.convert = NoDBColListArenaAddUnique(r.convert, NODB_ARENA_STRATEGY, col);
				// Make sure data is written to data cache.
				if (NoDBRelationIsColumnByValue(rel, col)) {
					AddColToMap(r.mapDataAddByValue, matrix[rowIdx][colIdx].dataCache, col);
				} else {
					AddColToMap(r.mapDataAddByRef, matrix[rowIdx][colIdx].dataCache, col);
				}
			}
			if (NoDBBitmapIsSet(writePositionsBitmap, colIdx) && matrix[rowIdx][colIdx].pmHit == CACHE_ADD) {
				// Make sure position is collected from a positional map.
				collectFromPMIdx = NoDBIntListArenaAddUnique(collectFromPMIdx, NODB_ARENA_STRATEGY, colIdx);
				// Make sure position is written to position cache.
				AddColToMap(r.mapPositionsAdd, matrix[rowIdx][colIdx].pmCache, col);
			}
		}

		// Determine best parsing strategy using positional maps.
		// This is done in two steps:
		// Step 1: For every column of interest (to read and/or add), there is a closest column.
		//    Build list indexed by closest column to determine the positional map that encompasses most fields
		//	  (i.e. longer distance).
		// Step 2: Go through columns of interest again, mark those to read from the positional map,
		//    and add the reading strategy.

		// Step 1.
		for (colIdx = 0; colIdx < NoDBRelationGetNumberColumns(rel); colIdx++) {
			colDistances[colIdx].cache = NULL;
			colDistances[colIdx].forwardDistance = 0;
			colDistances[colIdx].backwardDistance = 0;
		}
		for (list = collectFromPMIdx; list; list = list->next) {
			NoDBCol_t	closestColumn;

			colIdx = list->v;

			// Get closest column for desired column.
			closestColumn = matrix[rowIdx][colIdx].closestColumn;

			if (!colDistances[closestColumn].cache) {
				r.nreadViaPM++;
			}

			// Find min/max distances.
			if (matrix[rowIdx][colIdx].distance >= 0) {
				if (colDistances[closestColumn].cache == NULL || matrix[rowIdx][colIdx].distance > colDistances[closestColumn].forwardDistance) {
					colDistances[closestColumn].cache = matrix[rowIdx][colIdx].closestPMCache;
					colDistances[closestColumn].forwardDistance = matrix[rowIdx][colIdx].distance;
				}
			} else {
				if (matrix[rowIdx][colIdx].distance < colDistances[closestColumn].backwardDistance) {
					colDistances[closestColumn].cache = matrix[rowIdx][colIdx].closestPMCache;
					colDistances[closestColumn].backwardDistance = matrix[rowIdx][colIdx].distance;
				}
			}
		}

		// Step 2.
		r.readViaPM = NoDBArenaAlloc(NODB_ARENA_ITERATOR, r.nreadViaPM * sizeof(NoDBReadViaPM_t));
		n = 0;
		for (list = collectFromPMIdx; list; list = list->next) {
			NoDBCol_t	closestColumn;

			colIdx = list->v;

			// Get closest column for desired column.
			closestColumn = matrix[rowIdx][colIdx].closestColumn;

			// Make sure the strategy reads its position from the positional map.
			if (AddUniqueColToMap(r.mapReadFromPM, colDistances[closestColumn].cache, closestColumn)) {
				// Add parsing information.
				r.readViaPM[n].col = closestColumn;
				r.readViaPM[n].forward = colDistances[closestColumn].forwardDistance;
				r.readViaPM[n].backward = colDistances[closestColumn].backwardDistance;
				n++;
			}
		}
	}

	return r;
}

/*
 ============
 BuildStrategy
 ============
*/
void BuildStrategy(NoDBScanStrategyIterator_t *it)
{
	Node_t			**matrix;

	matrix = BuildMatrix(it);

	it->cur = 0;
	it->len = NoDBRowVectorSize(it->queryInfo.rows) - 1;

	// Build scan strategy.
	it->strategy = NoDBArenaAlloc(NODB_ARENA_ITERATOR, sizeof(NoDBScanStrategy_t) * it->len);

	// There are no pre-filters in the request.
	if (NoDBBitmapIsAllClear(it->queryInfo.readFilterBitmap)) {
		int				rowIdx;

		for (rowIdx = 0; rowIdx < it->len; rowIdx++) {
			FillRowReply_t 	r;

			// Set strategy rows.
			it->strategy[rowIdx].startRow = NoDBRowVectorGet(it->queryInfo.rows, rowIdx);
			it->strategy[rowIdx].nrows = NoDBRowVectorGet(it->queryInfo.rows, rowIdx + 1) - it->strategy[rowIdx].startRow;

			// Pre-filter fields are not needed.
			it->strategy[rowIdx].nreadPreFilterWithCache = 0;
			it->strategy[rowIdx].readPreFilterWithCache = NULL;
			it->strategy[rowIdx].nreadPreFilterWithPM = 0;
			it->strategy[rowIdx].readPreFilterWithPM = NULL;
			it->strategy[rowIdx].nreadPreFilterViaPM = 0;
			it->strategy[rowIdx].readPreFilterViaPM = NULL;
			it->strategy[rowIdx].readPreFilterWithFile = NoDBColVectorArenaInit(NODB_ARENA_ITERATOR, 0);
			it->strategy[rowIdx].convertPreFilter = NoDBColVectorArenaInit(NODB_ARENA_ITERATOR, 0);

			// Fill post-filter fields.
			r = FillRow(matrix, rowIdx, it->queryInfo.rel, it->queryInfo.cols, it->queryInfo.readRestBitmap, it->queryInfo.writeDataBitmap, it->queryInfo.writePositionsBitmap);
			SetColsPerCache(r.mapDataHit, &it->strategy[rowIdx].nreadPostFilterWithCache, &it->strategy[rowIdx].readPostFilterWithCache);
			it->strategy[rowIdx].readPostFilterWithFile = NoDBColVectorArenaFromList(r.collectFromFile, NODB_ARENA_ITERATOR);
			it->strategy[rowIdx].convertPostFilter = NoDBColVectorArenaFromList(r.convert, NODB_ARENA_ITERATOR);
			SetColsPerCache(r.mapDataAddByValue, &it->strategy[rowIdx].nwriteToCacheByValue, &it->strategy[rowIdx].writeToCacheByValue);
			SetColsPerCache(r.mapDataAddByRef, &it->strategy[rowIdx].nwriteToCacheByRef, &it->strategy[rowIdx].writeToCacheByRef);
			SetColsPerCache(r.mapPositionsAdd, &it->strategy[rowIdx].nwriteToPM, &it->strategy[rowIdx].writeToPM);
			SetColsPerCache(r.mapReadFromPM, &it->strategy[rowIdx].nreadPostFilterWithPM, &it->strategy[rowIdx].readPostFilterWithPM);
			it->strategy[rowIdx].readPostFilterViaPM = r.readViaPM;
			it->strategy[rowIdx].nreadPostFilterViaPM = r.nreadViaPM;
		}
	} else {			// There are pre-filters in request.
		int				rowIdx;

		for (rowIdx = 0; rowIdx < it->len; rowIdx++) {
			FillRowReply_t 	r;
			NoDBBitmap_t	*readRestBitmap;
			int				colIdx;
			NoDBBitmap_t	*emptyBitmap;

			// Set strategy rows.
			it->strategy[rowIdx].startRow = NoDBRowVectorGet(it->queryInfo.rows, rowIdx);
			it->strategy[rowIdx].nrows = NoDBRowVectorGet(it->queryInfo.rows, rowIdx + 1) - it->strategy[rowIdx].startRow;

			// Fill pre-filter fields.
			r = FillRow(matrix, rowIdx, it->queryInfo.rel, it->queryInfo.cols, it->queryInfo.readFilterBitmap, it->queryInfo.writeDataBitmap, it->queryInfo.writePositionsBitmap);
			SetColsPerCache(r.mapDataHit, &it->strategy[rowIdx].nreadPreFilterWithCache, &it->strategy[rowIdx].readPreFilterWithCache);
			it->strategy[rowIdx].readPreFilterWithFile = NoDBColVectorArenaFromList(r.collectFromFile, NODB_ARENA_ITERATOR);
			it->strategy[rowIdx].convertPreFilter = NoDBColVectorArenaFromList(r.convert, NODB_ARENA_ITERATOR);
			SetColsPerCache(r.mapDataAddByValue, &it->strategy[rowIdx].nwriteToCacheByValue, &it->strategy[rowIdx].writeToCacheByValue);
			SetColsPerCache(r.mapDataAddByRef, &it->strategy[rowIdx].nwriteToCacheByRef, &it->strategy[rowIdx].writeToCacheByRef);
			SetColsPerCache(r.mapPositionsAdd, &it->strategy[rowIdx].nwriteToPM, &it->strategy[rowIdx].writeToPM);
			SetColsPerCache(r.mapReadFromPM, &it->strategy[rowIdx].nreadPreFilterWithPM, &it->strategy[rowIdx].readPreFilterWithPM);
			it->strategy[rowIdx].readPreFilterViaPM = r.readViaPM;
			it->strategy[rowIdx].nreadPreFilterViaPM = r.nreadViaPM;

			// Build list of fields that still need to be read. This can be a subset of it->queryInfo.readRestBitmap
			readRestBitmap = NoDBBitmapArenaInit(NODB_ARENA_STRATEGY, NoDBColVectorSize(it->queryInfo.cols));
			for (colIdx = 0; colIdx < NoDBColVectorSize(it->queryInfo.cols); colIdx++) {
				NoDBCol_t		col;

				col = NoDBColVectorGet(it->queryInfo.cols, colIdx);
				if (NoDBBitmapIsSet(it->queryInfo.readRestBitmap, colIdx)) {
					if (NoDBBitmapIsSet(it->queryInfo.readFilterBitmap, colIdx)) {
						// Column already handled by pre-filter.
						continue;
					} else if (NoDBColVectorContains(it->strategy[rowIdx].convertPreFilter, col)) {
						// Column already handled by pre-filter.
						continue;
					// FIXME:	There is an (important) optimization missing.
					//			If there is a posibitional map and the column to read is part of the "via PM" list before,
					//			then we just need to make sure it is included in the list of columns to convert/extract,
					//			since it's position must be already known.
					} else {
						NoDBBitmapSet(readRestBitmap, colIdx);
					}
				}
			}

			// Fill post-filter fields.
			emptyBitmap = NoDBBitmapArenaInit(NODB_ARENA_STRATEGY, NoDBColVectorSize(it->queryInfo.cols));
			r = FillRow(matrix, rowIdx, it->queryInfo.rel, it->queryInfo.cols, readRestBitmap, emptyBitmap, emptyBitmap);
			SetColsPerCache(r.mapDataHit, &it->strategy[rowIdx].nreadPostFilterWithCache, &it->strategy[rowIdx].readPostFilterWithCache);
			it->strategy[rowIdx].readPostFilterWithFile = NoDBColVectorArenaFromList(r.collectFromFile, NODB_ARENA_ITERATOR);
			it->strategy[rowIdx].convertPostFilter = NoDBColVectorArenaFromList(r.convert, NODB_ARENA_ITERATOR);
			SetColsPerCache(r.mapReadFromPM, &it->strategy[rowIdx].nreadPostFilterWithPM, &it->strategy[rowIdx].readPostFilterWithPM);
			it->strategy[rowIdx].readPostFilterViaPM = r.readViaPM;
			it->strategy[rowIdx].nreadPostFilterViaPM = r.nreadViaPM;
		}
	}
}

/*
 ============
 GetInterestingRows

 Scan list of caches and create an ordered list of row numbers where caches begin or end.
 These correspond to the locations where the read/write strategy may change.
 ============
*/
NoDBRowVector_t	GetInterestingRows(NoDBRelation_t *rel)
{
	NoDBList_t		*allCaches;
	NoDBRowList_t	*rows;

	rows = NULL;
	for (allCaches = NoDBCaches; allCaches; allCaches = allCaches->next) {
		NoDBCache_t	*cache;

		cache = allCaches->ptr;
		if (NoDBCacheGetRelation(cache) == rel && NoDBCacheGetUsedRows(cache) > 0) {
			NoDBRow_t	begin;

			begin = NoDBCacheGetBegin(cache);
			rows = NoDBRowListArenaAddSortUnique(rows, NODB_ARENA_STRATEGY, begin);
			rows = NoDBRowListArenaAddSortUnique(rows, NODB_ARENA_STRATEGY, begin + NoDBCacheGetUsedRows(cache));
		}
	}

	if (!rows) {
		return NoDBRowVectorArenaInit(NODB_ARENA_STRATEGY, 0);
	}

	rows = NoDBRowListArenaAddSortUnique(rows, NODB_ARENA_STRATEGY, 0);
	return NoDBRowVectorArenaFromList(rows, NODB_ARENA_STRATEGY);
}

/*
 ============
 NoDBScanStrategyInit
 ============
*/
NoDBScanStrategyIterator_t	*NoDBScanStrategyIterator(NoDBRelation_t *rel,
													  NoDBRow_t limit,
													  NoDBColVector_t filter,
													  NoDBColVector_t rest,
													  NoDBColVector_t writeData,
													  NoDBColVector_t writePositions)
{
	NoDBColList_t				*colList;
	int							i;
	NoDBScanStrategyIterator_t	*it;

	// Create iterator.
	it = NoDBArenaAlloc(NODB_ARENA_ITERATOR, sizeof(NoDBScanStrategyIterator_t));
	it->queryInfo.rel = rel;
	it->queryInfo.limit = limit;

	// Build lists with columns.
	it->queryInfo.readCols = NULL;
	colList = NULL;
	for (i = 0; i < NoDBColVectorSize(filter); i++) {
		NoDBCol_t col;

		col = NoDBColVectorGet(filter, i);
		colList = NoDBColListArenaAddUnique(colList, NODB_ARENA_ITERATOR, col);
		it->queryInfo.readCols = NoDBColListArenaAddUnique(it->queryInfo.readCols, NODB_ARENA_ITERATOR, col);
	}
	for (i = 0; i < NoDBColVectorSize(rest); i++) {
		NoDBCol_t col;

		col = NoDBColVectorGet(rest, i);
		colList = NoDBColListArenaAddUnique(colList, NODB_ARENA_ITERATOR, col);
		it->queryInfo.readCols = NoDBColListArenaAddUnique(it->queryInfo.readCols, NODB_ARENA_ITERATOR, col);
	}
	for (i = 0; i < NoDBColVectorSize(writeData); i++) {
		NoDBCol_t col;

		col = NoDBColVectorGet(writeData, i);
		colList = NoDBColListArenaAddUnique(colList, NODB_ARENA_ITERATOR, col);
	}
	for (i = 0; i < NoDBColVectorSize(writePositions); i++) {
		NoDBCol_t col;

		col = NoDBColVectorGet(writePositions, i);
		colList = NoDBColListArenaAddUnique(colList, NODB_ARENA_ITERATOR, col);
	}
	it->queryInfo.cols = NoDBColVectorArenaFromList(colList, NODB_ARENA_ITERATOR);

	// BuildTree() requires sorted columns.
	NoDBColVectorSort(it->queryInfo.cols);

	// Build read/write bitmaps.
	it->queryInfo.readBitmap = NoDBBitmapArenaInit(NODB_ARENA_ITERATOR, NoDBColVectorSize(it->queryInfo.cols));
	it->queryInfo.readFilterBitmap = NoDBBitmapArenaInit(NODB_ARENA_ITERATOR, NoDBColVectorSize(it->queryInfo.cols));
	it->queryInfo.readRestBitmap = NoDBBitmapArenaInit(NODB_ARENA_ITERATOR, NoDBColVectorSize(it->queryInfo.cols));
	it->queryInfo.writeDataBitmap = NoDBBitmapArenaInit(NODB_ARENA_ITERATOR, NoDBColVectorSize(it->queryInfo.cols));
	it->queryInfo.writePositionsBitmap = NoDBBitmapArenaInit(NODB_ARENA_ITERATOR, NoDBColVectorSize(it->queryInfo.cols));
	for (i = 0; i < NoDBColVectorSize(it->queryInfo.cols); i++) {
		NoDBCol_t col;

		col = NoDBColVectorGet(it->queryInfo.cols, i);
		if (NoDBColVectorContains(filter, col)) {
			NoDBBitmapSet(it->queryInfo.readBitmap, i);
			NoDBBitmapSet(it->queryInfo.readFilterBitmap, i);
		}
		if (NoDBColVectorContains(rest, col)) {
			NoDBBitmapSet(it->queryInfo.readBitmap, i);
			NoDBBitmapSet(it->queryInfo.readRestBitmap, i);
		}
		if (NoDBColVectorContains(writeData, col)) {
			NoDBBitmapSet(it->queryInfo.writeDataBitmap, i);
		}
		if (NoDBColVectorContains(writePositions, col)) {
			NoDBBitmapSet(it->queryInfo.writePositionsBitmap, i);
		}
	}

	// Find rows where caches either begin or end.
	it->queryInfo.rows = GetInterestingRows(it->queryInfo.rel);
	if (NoDBRowVectorSize(it->queryInfo.rows) == 0) {
		it->len = 0;
		it->cur = 0;
		return it;
	}

	BuildStrategy(it);
	NoDBArenaFree(NODB_ARENA_STRATEGY);
	return it;
}

/*
 ============
 NoDBScanStrategyIteratorNext
 ============
*/
void NoDBScanStrategyIteratorNext(NoDBScanStrategyIterator_t *it)
{
	it->cur++;
}

/*
 ============
 NoDBScanStrategyIteratorGet
 ============
*/
NoDBScanStrategy_t *NoDBScanStrategyIteratorGet(NoDBScanStrategyIterator_t *it)
{
	if (it->cur == it->len) {
		// Generate new strategy for unknown areas of the file.
		NoDBRow_t		row;

		if (!it->len) {
			// Handle case where there are no caches.
			row = 0;
		} else {
			// Start from where last strategy node ended.
			row = it->strategy[it->cur - 1].startRow + it->strategy[it->cur - 1].nrows;
		}

		it->queryInfo.rows = NoDBRowVectorArenaInit(NODB_ARENA_STRATEGY, 2);
		NoDBRowVectorSet(it->queryInfo.rows, 0, row);
		NoDBRowVectorSet(it->queryInfo.rows, 1, row + NODB_CACHE_DEFAULT_ROWS);

		BuildStrategy(it);
		NoDBArenaFree(NODB_ARENA_STRATEGY);
	}

	return &it->strategy[it->cur];
}

/*
 ============
 NoDBScanStrategyDestroy
 ============
*/
void NoDBScanStrategyIteratorDestroy(NoDBScanStrategyIterator_t *it)
{
	// FIXME: This frees all iterators, but should free only its own iterator.
	//NoDBArenaFree(NODB_ARENA_ITERATOR);
}


void prettyPrint(NoDBScanStrategy_t *strategy)
{
	int		j, k;

	printf("startRow %ld\n", strategy->startRow);
	printf("nrows %ld\n", strategy->nrows);

	for (j = 0; j < strategy->nreadPreFilterWithCache; j++) {
		printf("\tRead Pre-Filter with Cache %p\n", strategy->readPreFilterWithCache[j].cache);
		for (k = 0; k < strategy->readPreFilterWithCache[j].cols.size; k++) {
			printf("\t\tCol %d\n", strategy->readPreFilterWithCache[j].cols.cols[k]);
		}
	}

	for (j = 0; j < strategy->nreadPreFilterWithPM; j++) {
		printf("\tRead Pre-Filter with PM %p\n", strategy->readPreFilterWithPM[j].cache);
		for (k = 0; k < strategy->readPreFilterWithPM[j].cols.size; k++) {
			printf("\t\tCol %d\n", strategy->readPreFilterWithPM[j].cols.cols[k]);
		}
	}

	if (strategy->nreadPreFilterViaPM) {
		printf("\tRead Pre-Filter via PM\n");
		for (j = 0; j < strategy->nreadPreFilterViaPM; j++) {
			printf("\t\tCol %d Forward %d Backward %d\n", strategy->readPreFilterViaPM[j].col, strategy->readPreFilterViaPM[j].forward, strategy->readPreFilterViaPM[j].backward);
		}
	}

	if (strategy->readPreFilterWithFile.size) {
		printf("\tRead Pre-Filter with file\n");
		for (j = 0; j < strategy->readPreFilterWithFile.size; j++) {
			printf("\t\tCol %d\n", strategy->readPreFilterWithFile.cols[j]);
		}
	}

	if (strategy->convertPreFilter.size) {
		printf("\tConvert Pre-Filter\n");
		for (j = 0; j < strategy->convertPreFilter.size; j++) {
			printf("\t\tCol %d\n", strategy->convertPreFilter.cols[j]);
		}
	}

	for (j = 0; j < strategy->nreadPostFilterWithCache; j++) {
		printf("\tRead Post-Filter with Cache %p\n", strategy->readPostFilterWithCache[j].cache);
		for (k = 0; k < strategy->readPostFilterWithCache[j].cols.size; k++) {
			printf("\t\tCol %d\n", strategy->readPostFilterWithCache[j].cols.cols[k]);
		}
	}

	for (j = 0; j < strategy->nreadPostFilterWithPM; j++) {
		printf("\tRead Post-Filter with PM %p\n", strategy->readPostFilterWithPM[j].cache);
		for (k = 0; k < strategy->readPostFilterWithPM[j].cols.size; k++) {
			printf("\t\tCol %d\n", strategy->readPostFilterWithPM[j].cols.cols[k]);
		}
	}

	if (strategy->nreadPostFilterViaPM) {
		printf("\tRead Post-Filter via PM\n");
		for (j = 0; j < strategy->nreadPostFilterViaPM; j++) {
			printf("\t\tCol %d Forward %d Backward %d\n", strategy->readPostFilterViaPM[j].col, strategy->readPostFilterViaPM[j].forward, strategy->readPostFilterViaPM[j].backward);
		}
	}

	if (strategy->readPostFilterWithFile.size) {
		printf("\tRead Post-Filter with file\n");
		for (j = 0; j < strategy->readPostFilterWithFile.size; j++) {
			printf("\t\tCol %d\n", strategy->readPostFilterWithFile.cols[j]);
		}
	}

	if (strategy->convertPostFilter.size) {
		printf("\tConvert Post-Filter\n");
		for (j = 0; j < strategy->convertPostFilter.size; j++) {
			printf("\t\tCol %d\n", strategy->convertPostFilter.cols[j]);
		}
	}

	for (j = 0; j < strategy->nwriteToCacheByValue; j++) {
		printf("\tWrite to Cache By Value %p\n", strategy->writeToCacheByValue[j].cache);
		for (k = 0; k < strategy->writeToCacheByValue[j].cols.size; k++) {
			printf("\t\tCol %d\n", strategy->writeToCacheByValue[j].cols.cols[k]);
		}
	}

	for (j = 0; j < strategy->nwriteToCacheByRef; j++) {
		printf("\tWrite to Cache By Ref %p\n", strategy->writeToCacheByRef[j].cache);
		for (k = 0; k < strategy->writeToCacheByRef[j].cols.size; k++) {
			printf("\t\tCol %d\n", strategy->writeToCacheByRef[j].cols.cols[k]);
		}
	}

	for (j = 0; j < strategy->nwriteToPM; j++) {
		printf("\tWrite to PM %p\n", strategy->writeToPM[j].cache);
		for (k = 0; k < strategy->writeToPM[j].cols.size; k++) {
			printf("\t\tCol %d\n", strategy->writeToPM[j].cols.cols[k]);
		}
	}
}


