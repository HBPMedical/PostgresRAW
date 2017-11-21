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
 * load.h
 *	  Definitions for using new loading command based on copy of PostgreSQL.
 *
 *
 * $PostgreSQL: pgsql/src/include/snooping/load.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef LOAD_H
#define LOAD_H

#include "postgres.h"

#include "nodes/parsenodes.h"

#include "tcop/dest.h"

#include "snooping/global.h"
#include "snooping/metadata.h"
#include "snooping/coordinator.h"
#include "snooping/positionalIndex.h"
#include "snooping/storageComponent.h"

//
///*
// * TupleSector keeps a subset of the original attributes
// * It
// */
///*Currently not used...*/
//typedef struct TupleSelector
//{
//	int new_size;
//	int original_size;
//	TupleDesc newTupDesc;
//	int *selectedAttributes;
//	Datum *values;
//	bool *nulls;
//}TupleSelector;
//
//
//
//
//TupleTableSlot* getNextTupleV2( TupleTableSlot *ss_ScanTupleSlot, bool *done, int pos);
//TupleTableSlot* getNextTuple_Qual(bool *done, int pos, List *qual, ExprContext *econtext, TupleTableSlot *ss_ScanTupleSlot, bool *pass);
//TupleTableSlot* getNextTupleCacheOnly(TupleTableSlot *ss_ScanTupleSlot, bool *done, int pos);
//TupleTableSlot* getNextTuple_QualCacheOnly(bool *done, int pos, List *qual, ExprContext *econtext, TupleTableSlot *ss_ScanTupleSlot, bool *pass);
//TupleTableSlot* getNextTupleWithoutInterestingAtts(TupleTableSlot *ss_ScanTupleSlot, bool *done, int pos);
//
//
//void initializeTupleSelector(TupleDesc originalTupDesc, int *selectedAttributes,int new_size);
//void freeTupleSelector(void);
//TupleDesc getSelectiveTupleDesc(TupleDesc originalTupDesc, int *which, int size);
//
////void getSelectiveValues(CopyExecutionInfo activeExec, int* which, int size, int fldct, Datum *values, bool *nulls);
//
//TupleSelector getCurrentTupleSelector(void);
//TupleTableSlot* getNextTuple_PM(TupleTableSlot *ss_ScanTupleSlot, bool *done, int pos);
//TupleTableSlot* getNextTupleFileScan(TupleTableSlot *ss_ScanTupleSlot, bool *done, int pos);
//



#endif   /* LOAD_H */
