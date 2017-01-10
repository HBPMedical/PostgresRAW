/*
 * NoDBLoad.h
 *
 *  Created on: Jul 6, 2012
 *      Author: yannis
 */

#ifndef NODBEXECUTOR_H_
#define NODBEXECUTOR_H_


#include "snooping/global.h"

#include "noDB/NoDBCache.h"
#include "noDB/NoDBScanStrategy.h"
#include "noDB/NoDBScan.h"





TupleTableSlot *NoDBExecPlan(NoDBScanState_t cstate, bool *pass);
TupleTableSlot *NoDBExecPlanWithFilters(NoDBScanState_t cstate, bool *pass);
TupleTableSlot *NoDBExecPlanCacheOnly(NoDBScanState_t cstate, bool *pass);
TupleTableSlot *NoDBExecPlanWithFiltersCacheOnly(NoDBScanState_t cstate, bool *pass);


bool NoDBGetNextTupleFromFile(NoDBScanState_t cstate);
bool NoDBGetNextTupleFromFileWithEOL(NoDBScanState_t cstate);
bool NoDBTryReFillRawBuf(NoDBScanState_t cstate);
bool NoDBCopyReadLineText(NoDBScanState_t cstate);


#endif /* NODBEXECUTOR_H_ */


