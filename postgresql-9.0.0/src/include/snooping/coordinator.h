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

#ifndef COORDINATOR_H_
#define COORDINATOR_H_

#include "postgres.h"

#include "nodes/execnodes.h"
#include "nodes/parsenodes.h"
#include "executor/execdesc.h"

#include "tcop/dest.h"

#include "snooping/global.h"
#include "snooping/common.h"
#include "snooping/metadata.h"
#include "snooping/positionalIndex.h"
#include "snooping/queryDescriptor.h"

#include "noDB/NoDBScan.h"


/*This struct is used to pass data for early tuple check*/
//typedef struct DataPointer
//{
//	bool enable;
//	Datum *values;
//	bool *isnull;
//} DataPointer;



//typedef struct ExecutionStateData
//{
//	bool tofree;
//
//} ExecutionStateData;
//
//typedef ExecutionStateData *ExecutionState;





/* *
 * Initialization Variables
 * Enable: InvisibleDB + extra features
 * */
//extern bool enable_invisible_db;
//extern bool enable_tuple_metapointers;
//extern bool enable_internal_metapointers;
//extern bool enable_pushdown_select;
//
//typedef struct CopyExecutionInfo
//{
////	ExecutionState execstate;
//	CopyState cstate;
//	CopyStmtExecStatus status;
//	Controller IntegrityCheck;
//	CopyStmt* planCopyStmt;
//	int initialized;
//	char relation[MAX_RELATION_NAME];
//} CopyExecutionInfo;
//
//
///* Execution information per relation () */
//extern CopyExecutionInfo CopyExec[NUMBER_OF_RELATIONS];
//extern int usedCopyExec;
//
//
//
////extern DataPointer ValuesAndNullsHolder;
//
//
//int findExecutionInfo(char *relation);
//CopyExecutionInfo getExecutionInfo(int pos);
////int getFilePointerID(char *relation);
//
//
//void initializeRelation(char *relation, bool hasQual);
//void reInitRelation(CopyState cstate, CopyStmtExecStatus status);
//
//void initializeLoadModule(void);
//void restartFilePointer(CopyState cstate);
//void resetLoadModule(char *relation);
//
//void freeCopyState(char *relation);
//void freeCopyStmtExecutionStatus(char *relation);
//
//bool getExecutionStatusCache(int pos);
//bool getExecutionStatusInteresting(int pos);
//
//
////void CopySendData(CopyState cstate, void *databuf, int datasize);
//int CopyGetData(CopyState cstate, void *databuf, int minread, int maxread);
////void CopySendInt32(CopyState cstate, int32 val);
//bool CopyGetInt32(CopyState cstate, int32 *val);
////void CopySendInt16(CopyState cstate, int16 val);
////bool CopyGetInt16(CopyState cstate, int16 *val);
//
//
//TupleTableSlot *heapTupleToTupleTableSlot(HeapTuple tuple, TupleTableSlot *ss_ScanTupleSlot, bool done);
//TupleTableSlot *generateTupleTableSlot( TupleTableSlot *ss_ScanTupleSlot, Datum *values, bool *isnull);
//
//
//ScanState *getProperScanState(PlanState *planstate);
//
//List *traversePlanTree(Plan *plan, PlanState *planstate, Plan *outer_plan, PlannedStmt *topPlan);
//void updatePlanTree(Plan *plan, PlanState *planstate, Plan *outer_plan, PlannedStmt *topPlan, NodeTag oldTag, NodeTag newTag);
//
//void printSystemInformation(void);
//
//void printCopyStateData(int pos);


#endif /* COORDINATOR_H_ */

