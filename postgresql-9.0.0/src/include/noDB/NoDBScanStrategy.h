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


#ifndef NODB_SCAN_STRATEGY_H
#define NODB_SCAN_STRATEGY_H

#include "noDB/auxiliary/NoDBRow.h"
#include "noDB/auxiliary/NoDBList.h"
#include "noDB/auxiliary/NoDBBitmap.h"
#include "noDB/auxiliary/NoDBRelation.h"

#include "noDB/NoDBCache.h"

typedef struct NoDBColumnsPerCache_t
{
	NoDBCache_t				*cache;
	NoDBColVector_t			cols;
} NoDBColumnsPerCache_t;

typedef struct NoDBReadViaPM_t
{
	NoDBCol_t				col;
	int						forward;
	int						backward;
} NoDBReadViaPM_t;

typedef struct NoDBScanStrategy_t {
	NoDBRow_t				startRow;
	NoDBRow_t				nrows;
	NoDBColumnsPerCache_t	*readPreFilterWithCache;
	int						nreadPreFilterWithCache;
	NoDBColumnsPerCache_t	*readPreFilterWithPM;
	int						nreadPreFilterWithPM;
	NoDBReadViaPM_t			*readPreFilterViaPM;
	int						nreadPreFilterViaPM;
	NoDBColVector_t			readPreFilterWithFile;
	NoDBColVector_t			convertPreFilter;
	NoDBColumnsPerCache_t	*readPostFilterWithCache;
	int						nreadPostFilterWithCache;
	NoDBColumnsPerCache_t	*readPostFilterWithPM;
	int						nreadPostFilterWithPM;
	NoDBReadViaPM_t			*readPostFilterViaPM;
	int						nreadPostFilterViaPM;
	NoDBColVector_t			readPostFilterWithFile;
	NoDBColVector_t			convertPostFilter;
	NoDBColumnsPerCache_t	*writeToCacheByValue;
	int						nwriteToCacheByValue;
	NoDBColumnsPerCache_t	*writeToCacheByRef;
	int						nwriteToCacheByRef;
	NoDBColumnsPerCache_t	*writeToPM;
	int						nwriteToPM;
} NoDBScanStrategy_t;

typedef struct NoDBQueryInfo_t {
	NoDBRelation_t			*rel;
	NoDBRow_t				limit;
	NoDBColVector_t			cols;
	NoDBRowVector_t			rows;
	NoDBBitmap_t			*readBitmap;
	NoDBBitmap_t			*readFilterBitmap;
	NoDBBitmap_t			*readRestBitmap;
	NoDBBitmap_t			*writeDataBitmap;
	NoDBBitmap_t			*writePositionsBitmap;
	NoDBColList_t			*readCols;
} NoDBQueryInfo_t;

typedef struct NoDBScanStrategyIterator_t {
	NoDBQueryInfo_t			queryInfo;
	NoDBScanStrategy_t		*strategy;
	int						len;
	int						cur;
} NoDBScanStrategyIterator_t;

NoDBList_t *NoDBCaches;

NoDBScanStrategyIterator_t	*NoDBScanStrategyIterator(NoDBRelation_t *rel,
													  NoDBRow_t limit,
													  NoDBColVector_t filter,
													  NoDBColVector_t rest,
													  NoDBColVector_t writeData,
													  NoDBColVector_t writePositions);
void						NoDBScanStrategyIteratorNext(NoDBScanStrategyIterator_t *it);
NoDBScanStrategy_t			*NoDBScanStrategyIteratorGet(NoDBScanStrategyIterator_t *it);
void 						NoDBScanStrategyIteratorDestroy(NoDBScanStrategyIterator_t *it);

void prettyPrint(NoDBScanStrategy_t *strategy);

#endif	/* NODB_SCAN_STRATEGY_H */
