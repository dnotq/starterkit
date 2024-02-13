/**
 * cidx_test.c
 *
 * *nix:
 * gcc   -Wall -Wextra -O2 -I../xyz -o test_nix cidx_test.c -lpthread
 * clang -Wall -Wextra -O2 -I../xyz -o test_nix cidx_test.c -lpthread
 *
 * MINGW:
 * gcc   -Wall -Wextra -O2 -I../xyz -o test_mgw cidx_test.c -lpthread
 * clang -Wall -Wextra -O2 -I../xyz -o test_mgw cidx_test.c -lpthread
 *
 * Windows:
 * cl -MT -Gy -W4 -nologo -experimental:c11atomics -std:c11 -I../xyz -Fe:test_win.exe cidx_test.c
 */


#include <stdio.h>
#include <stdint.h>

// Windows has its own thread model, but MINGW supports pthreads.
#if !defined(__MINGW32__) && !defined(__MINGW64__) && (defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__))
#define WIN_THREADS
#endif


#ifdef WIN_THREADS
#include <windows.h>
#include <process.h>    // _beginthreadex
#else
#include <pthread.h>    // pthreads
#endif

#define CIDX_IMPLEMENTATION
#include "cidx.h"
#include "xyz.h"


s32 freecnt = 0; // unprotected counter, reader and writer update.
size_t wcnt = 0; // writer use only.
size_t rcnt = 0; // reader use only.
size_t rderr = 0; // read errors.

size_t full = 0;  // number of times the writer found the index full.
size_t empty = 0; // number of times the reader found the index empty.

// Test data on the heap, no need to jam up the stack.
#define DATAMAX 10
s32 g_data[DATAMAX];

// Number of writes / reads, should be a multiple of DATAMAX for the
// test to work correctly.
#define ITERATIONS 1000000

// Signals are being used since all forms of thread-yeild failed during
// testing on all operating systems.  Windows wins for simplicity.
#ifdef WIN_THREADS
HANDLE full_event = NULL;
HANDLE empty_event = NULL;
#else
pthread_mutex_t full_mutex  = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  full_cond   = PTHREAD_COND_INITIALIZER;
pthread_mutex_t empty_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  empty_cond  = PTHREAD_COND_INITIALIZER;
#endif


/// Single writer thread to shared variable.
void *
wr(void *thr_data) {

    cidx_s *ci = (cidx_s *)thr_data;

    bool signaled = false;
    while ( ITERATIONS > wcnt ) {
        if ( false == cidx_isfull(ci) ) {
            s32 *n = (s32 *)cidx_we(ci);
            (*n)++;
            cidx_commit(ci);

            wcnt++;
            freecnt++; // unprotected update.

            // Signal when half way full to try and add some variation to the
            // thread interactions.
            if ( false == signaled && cidx_free(ci) < (ci->max / 2) ) {
                #ifdef WIN_THREADS
                SignalObjectAndWait(/*signal*/ full_event, /*wait*/ empty_event, /*timeout_ms*/ 0, /*alertable*/ FALSE);
                #else
                pthread_cond_signal(&full_cond);
                #endif
                signaled = true;
            }
        }

        else {
            full++;
            signaled = false;
            #ifdef WIN_THREADS
            SignalObjectAndWait(/*signal*/ full_event, /*wait*/ empty_event, /*timeout_ms*/ 1, /*alertable*/ FALSE);
            #else
            pthread_mutex_lock(&empty_mutex);
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += (/*1ms*/1 * 1000 * 1000);
            pthread_cond_signal(&full_cond);
            pthread_cond_timedwait(&empty_cond, &empty_mutex, &ts);
            pthread_mutex_unlock(&empty_mutex);
            #endif
        }
    }

    return NULL;
}
// wr()


/// Single reader thread from shared variable.
void *
rd(void *thr_data) {

    cidx_s *ci = (cidx_s *)thr_data;
    s32 cnt = 0; // count up to DATAMAX, then increment series.
    s32 series = 1;

    bool signaled = false;
    while ( ITERATIONS > rcnt ) {
        if ( false == cidx_isempty(ci) ) {
            s32 *n = (s32 *)cidx_re(ci);
            // Check that the data is correct.
            if ( series != *n ) {
                rderr++;
            }
            cidx_consume(ci);

            cnt++;
            if ( DATAMAX == cnt ) {
                cnt = 0;
                series++;
            }

            rcnt++;
            freecnt--; // unprotected update.

            // Signal when half way empty to try and add some variation to the
            // thread interactions.
            if ( false == signaled && cidx_free(ci) > (ci->max / 2) ) {
                #ifdef WIN_THREADS
                SignalObjectAndWait(/*signal*/ empty_event, /*wait*/ full_event, /*timeout_ms*/ 0, /*alertable*/ FALSE);
                #else
                pthread_cond_signal(&empty_cond);
                #endif
                signaled = true;
            }
        }

        else {
            empty++;
            signaled = false;
            #ifdef WIN_THREADS
            SignalObjectAndWait(/*signal*/ empty_event, /*wait*/ full_event, /*timeout_ms*/ 1, /*alertable*/ FALSE);
            #else
            pthread_mutex_lock(&full_mutex);
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += (/*1ms*/1 * 1000 * 1000);
            pthread_cond_signal(&empty_cond);
            pthread_cond_timedwait(&full_cond, &full_mutex, &ts);
            pthread_mutex_unlock(&full_mutex);
            #endif
        }
    }

    return NULL;
}
// rd()


// ====
// Thread wrapper functions for Win vs pthread.

#ifdef WIN_THREADS
unsigned
thd_wr(void *thd_data) {
    wr(thd_data);
    _endthreadex(0);
    return 0;
}

unsigned
thd_rd(void *thd_data) {
    rd(thd_data);
    _endthreadex(0);
    return 0;
}


#else
void *
thd_wr(void *thd_data) {
    wr(thd_data);
    return NULL;
}


void *
thd_rd(void *thd_data) {
    rd(thd_data);
    return NULL;
}
#endif


int
main(void) {

    cidx_s ci;
    cidx_init(&ci, sizeof(s32), &g_data, DATAMAX);

    printf("CIDX Test\n");
    printf("buf max:  %zu\n", ci.max);
    printf("buf free: %zu\n", cidx_free(&ci));
    printf("buf used: %zu\n", cidx_used(&ci));

    #ifdef WIN_THREADS
    full_event = CreateEventA(/*security_attrs*/ NULL, /*manual_reset*/ FALSE,
        /*initial_state*/ FALSE, /*name*/ NULL);
    empty_event = CreateEventA(/*security_attrs*/ NULL,/*manual_reset*/ FALSE,
        /*initial_state*/ FALSE, /*name*/ NULL);

    HANDLE thd[2]; // Handles for created threads

    thd[0] = (HANDLE)_beginthreadex(/*security*/ NULL, /*stack_size*/ 0,
        &thd_wr, &ci, /*initflag*/ 0, /*thrdaddr*/ NULL);
    thd[1] = (HANDLE)_beginthreadex(/*security*/ NULL, /*stack_size*/ 0,
        &thd_rd, &ci, /*initflag*/ 0, /*thrdaddr*/ NULL);

    WaitForSingleObject(thd[0], INFINITE);
    WaitForSingleObject(thd[1], INFINITE);
    CloseHandle(thd[0]);
    CloseHandle(thd[1]);
    CloseHandle(full_event);
    CloseHandle(empty_event);

    #else
    pthread_t thr[2];
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_create(&thr[0], &attr, &thd_wr, &ci);
    pthread_create(&thr[1], &attr, &thd_rd, &ci);
    pthread_join(thr[0], NULL);
    pthread_join(thr[1], NULL);
    #endif

    printf("CIDX reader/writer:\n");
    printf("The free counter: %d\n", freecnt);
    printf("The wcnt counter: %zu, full count:  %zu\n", wcnt, full);
    printf("The rcnt counter: %zu, empty count: %zu\n", rcnt, empty);
    printf("The rderr: %zu\n", rderr);

    printf("Data values (expect %d):\n", (ITERATIONS / DATAMAX));
    for ( s32 i = 0 ; i < DATAMAX ; i++ ) {
        printf("  %d. %d\n", (i+1), g_data[i]);
    }


    // Add some additional individual tests.
    printf("\nAdditional tests.\n");
    printf("isfull:  %d (expect 0)\n", cidx_isfull(&ci));
    printf("isempty: %d (expect 1)\n", cidx_isempty(&ci));
    printf("used:    %zu (expect 0)\n", cidx_used(&ci));
    printf("free:    %zu (expect 10)\n", cidx_free(&ci));

    printf("\nWriting 4 values...\n");
    g_data[ci.wr] = 1; cidx_commit(&ci);
    g_data[ci.wr] = 2; cidx_commit(&ci);
    g_data[ci.wr] = 3; cidx_commit(&ci);
    g_data[ci.wr] = 4; cidx_commit(&ci);
    printf("isfull:  %d (expect 0)\n", cidx_isfull(&ci));
    printf("isempty: %d (expect 0)\n", cidx_isempty(&ci));
    printf("used:    %zu (expect 4)\n", cidx_used(&ci));
    printf("free:    %zu (expect 6)\n", cidx_free(&ci));

    printf("\nWriting 6 values...\n");
    g_data[ci.wr] = 5; cidx_commit(&ci);
    g_data[ci.wr] = 6; cidx_commit(&ci);
    g_data[ci.wr] = 7; cidx_commit(&ci);
    g_data[ci.wr] = 8; cidx_commit(&ci);
    g_data[ci.wr] = 9; cidx_commit(&ci);
    g_data[ci.wr] = 10; cidx_commit(&ci);
    printf("isfull:  %d (expect 1)\n", cidx_isfull(&ci));
    printf("isempty: %d (expect 0)\n", cidx_isempty(&ci));
    printf("used:    %zu (expect 10)\n", cidx_used(&ci));
    printf("free:    %zu (expect 0)\n", cidx_free(&ci));

    printf("\nReading 2 values...\n");
    printf("Read: %d (expect 1)\n", g_data[ci.rd]);
    cidx_consume(&ci);
    printf("Read: %d (expect 2)\n", g_data[ci.rd]);
    cidx_consume(&ci);

    printf("\nDraining index: %zu (expect 8)\n", cidx_drain(&ci));
    printf("rd: %zu, wr: %zu (expect equal)\n", ci.rd, ci.wr);

    return 0 == rderr ? 0 : 1;
}
// main()
