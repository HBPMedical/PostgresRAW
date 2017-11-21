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

#ifndef NODBTIMER_H_
#define NODBTIMER_H_

#include <time.h>
#include <stdint.h>
#include <stdio.h>

typedef struct timespec TimeSpec_t;

#define TIMESPEC_SET_ZERO(t)  ((t).tv_sec = 0, (t).tv_nsec = 0)

/* The timer works per query in the scan operator*/
typedef enum TimerType_t {
    PARSING = 0,
    TOKENIZING,
    CONVERSION,
    IO,
    NTIMER_TYPES
} TimerType_t;


typedef struct NoDBTimer_t {
    TimeSpec_t begin[NTIMER_TYPES];
    TimeSpec_t end[NTIMER_TYPES];
    TimeSpec_t elapsed[NTIMER_TYPES];
} NoDBTimer_t;


extern NoDBTimer_t NoDBLastQueryBreakdown;
extern int        NoDBBreakDown;

void        NoDBTimerSetZero(NoDBTimer_t *timer);
void        NoDBTimerSetBegin(NoDBTimer_t *timer, TimerType_t type);
void        NoDBTimerSetEnd(NoDBTimer_t *timer, TimerType_t type);
void        NoDBTimerSumElapsed(NoDBTimer_t *timer1, NoDBTimer_t *timer2);
uint64_t    NoDBGetElapsedTime(NoDBTimer_t timer, TimerType_t type);



#endif /* NODBTIMER_H_ */
