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

#include "noDB/auxiliary/NoDBTimer.h"



NoDBTimer_t NoDBLastQueryBreakdown;
int        NoDBBreakDown = 0;

static void NoDBTimerUpdateElapsed(NoDBTimer_t *timer, TimerType_t type);

void NoDBTimerSetZero(NoDBTimer_t *timer)
{
    int i;
    for ( i = 0 ; i < NTIMER_TYPES; i++)
    {
        TIMESPEC_SET_ZERO(timer->begin[i]);
        TIMESPEC_SET_ZERO(timer->end[i]);
        TIMESPEC_SET_ZERO(timer->elapsed[i]);
    }
}

void NoDBTimerSetBegin(NoDBTimer_t *timer, TimerType_t type)
{
//	clock_gettime(CLOCK_MONOTONIC, &timer->begin[type]);
}

void NoDBTimerSetEnd(NoDBTimer_t *timer, TimerType_t type)
{
//    clock_gettime(CLOCK_MONOTONIC, &timer->end[type]);
    NoDBTimerUpdateElapsed(timer,type);
}

void NoDBTimerSumElapsed(NoDBTimer_t *timer1, NoDBTimer_t *timer2)
{
    int i;
    for ( i = 0 ; i < NTIMER_TYPES; i++)
    {
        timer1->elapsed[i].tv_sec += timer2->elapsed[i].tv_sec;
        timer1->elapsed[i].tv_nsec += timer2->elapsed[i].tv_nsec;

        /* Normalize after each add to avoid overflow/underflow of tv_nsec */ \
        while (timer1->elapsed[i].tv_nsec < 0)
        {
            timer1->elapsed[i].tv_nsec += 1000000000;
            timer1->elapsed[i].tv_sec--;
        }
        while (timer1->elapsed[i].tv_nsec >= 1000000000)
        {
            timer1->elapsed[i].tv_nsec -= 1000000000;
            timer1->elapsed[i].tv_sec++;
        }
    }
}

uint64_t NoDBGetElapsedTime(NoDBTimer_t timer, TimerType_t type)
{
    if ( type == PARSING )
        return (timer.elapsed[PARSING].tv_sec * 1000000000LL + timer.elapsed[PARSING].tv_nsec) -
                (timer.elapsed[IO].tv_sec * 1000000000LL + timer.elapsed[IO].tv_nsec);
    return timer.elapsed[type].tv_sec * 1000000000LL + timer.elapsed[type].tv_nsec;
}

static void NoDBTimerUpdateElapsed(NoDBTimer_t *timer, TimerType_t type)
{
    timer->elapsed[type].tv_sec += (timer->end[type].tv_sec - timer->begin[type].tv_sec);
    timer->elapsed[type].tv_nsec += (timer->end[type].tv_nsec - timer->begin[type].tv_nsec);

    /* Normalize after each add to avoid overflow/underflow of tv_nsec */ \
    while (timer->elapsed[type].tv_nsec < 0)
    {
        timer->elapsed[type].tv_nsec += 1000000000;
        timer->elapsed[type].tv_sec--;
    }
    while (timer->elapsed[type].tv_nsec >= 1000000000)
    {
        timer->elapsed[type].tv_nsec -= 1000000000;
        timer->elapsed[type].tv_sec++;
    }

}

void printTimer(NoDBTimer_t timer)
{
    fprintf(stderr,"PARSING: %lf\n", (double)NoDBGetElapsedTime(timer,PARSING) / 1000000);
    fprintf(stderr,"TOKENIZING: %lf\n",(double)NoDBGetElapsedTime(timer,TOKENIZING)  / 1000000);
    fprintf(stderr,"CONVERSION: %lf\n",(double)NoDBGetElapsedTime(timer,CONVERSION)  / 1000000);
    fprintf(stderr,"IO: %lf\n",(double)NoDBGetElapsedTime(timer,IO)  / 1000000);
}


























