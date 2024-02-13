/**
 * cbuf_test.c
 *
 * *nix:
 * gcc   -Wall -Wextra -O2 -I../xyz -o test_nix cbuf_test.c -lpthread
 * clang -Wall -Wextra -O2 -I../xyz -o test_nix cbuf_test.c -lpthread
 *
 * MINGW:
 * gcc   -Wall -Wextra -O2 -I../xyz -o test_mgw cbuf_test.c -lpthread -lmincore
 * clang -Wall -Wextra -O2 -I../xyz -o test_mgw cbuf_test.c -lpthread -lmincore
 *
 * Windows:
 * cl -MT -Gy -W4 -nologo -experimental:c11atomics -std:c11 -I../xyz -Fe:test_win.exe cbuf_test.c
 */

#if defined(__MINGW32__) || defined(__MINGW64__)
// MinGW needs these for VirtualAlloc2 and MapViewOfFile3
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000005
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>    // true, false

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

#define CBUF_IMPLEMENTATION
#ifdef __linux__
#define CBUF_HAVE_MREMAP
#endif
#include "cbuf.h"
#include "xyz.h"


s32 freecnt = 0; // unprotected counter, reader and writer update.
size_t wcnt = 0; // writer use only.
size_t rcnt = 0; // reader use only.
size_t rderr = 0; // read errors.

size_t full = 0;  // number of times the writer found the index full.
size_t empty = 0; // number of times the reader found the index empty.

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
wr(void *thd_data) {

    cbuf_s *cb = (cbuf_s *)thd_data;

    bool signaled = false;
    u8 cnt = 1; // cnt counts, zero is skipped.
    while ( ITERATIONS > wcnt && ITERATIONS > full) {

        if ( cbuf_free(cb) >= cnt ) {

            // Write the value of cnt into the buffer, cnt times.
            u8 *wp = cbuf_wb(cb);
            for ( u8 idx = 0 ; idx < cnt ; idx++ ) {
                wp[idx] = cnt;
            }

            cbuf_commit(cb, cnt);

            // Increment cnt, skip zero.
            cnt = (255u == cnt) ? 1 : cnt + 1;

            wcnt++;
            freecnt++; // unprotected update.

            // Signal when half way full to try and add some variation to the
            // thread interactions.
            if ( false == signaled && cbuf_free(cb) < (cb->max / 2) ) {
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
rd(void *thd_data) {

    cbuf_s *cb = (cbuf_s *)thd_data;

    bool signaled = false;
    u8 cnt = 1; // cnt counts, zero is skipped.
    while ( ITERATIONS > rcnt && ITERATIONS > empty ) {

        if ( false == cbuf_isempty(cb) ) {

            u8 *rp = cbuf_rb(cb);
            // Check that the data is correct.  Should be cnt number of the
            // value cnt.
            for ( u8 idx = 0 ; idx < cnt ; idx++ ) {
                if ( rp[idx] != cnt ) {
                rderr++;
                cnt = idx; // try to get back on track.
                }
            }

            cbuf_consume(cb, cnt);

            // Increment cnt, skip zero.
            cnt = (255u == cnt) ? 1 : cnt + 1;

            rcnt++;
            freecnt--; // unprotected update.

            // Signal when half way empty to try and add some variation to the
            // thread interactions.
            if ( false == signaled && cbuf_free(cb) > (cb->max / 2) ) {
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

    printf("CBUF Test\n");

    cbuf_s cb;
    s32 ret = cbuf_create(&cb, 0, 1);
    printf("ret = %d\n", ret);

    printf("buf max:  %zu\n", cb.max);
    printf("buf free: %zu\n", cbuf_free(&cb));
    printf("buf used: %zu\n", cbuf_used(&cb));

    #ifdef WIN_THREADS
    full_event = CreateEventA(/*security_attrs*/ NULL, /*manual_reset*/ FALSE,
        /*initial_state*/ FALSE, /*name*/ NULL);
    empty_event = CreateEventA(/*security_attrs*/ NULL,/*manual_reset*/ FALSE,
        /*initial_state*/ FALSE, /*name*/ NULL);

    HANDLE thd[2]; // Handles for created threads

    thd[0] = (HANDLE)_beginthreadex(/*security*/ NULL, /*stack_size*/ 0,
        &thd_wr, &cb, /*initflag*/ 0, /*thrdaddr*/ NULL);
    thd[1] = (HANDLE)_beginthreadex(/*security*/ NULL, /*stack_size*/ 0,
        &thd_rd, &cb, /*initflag*/ 0, /*thrdaddr*/ NULL);

    WaitForSingleObject(thd[0], INFINITE);
    WaitForSingleObject(thd[1], INFINITE);
    CloseHandle(thd[0]);
    CloseHandle(thd[1]);
    CloseHandle(full_event);
    CloseHandle(empty_event);

    #else
    pthread_t thd[2];
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_create(&thd[0], &attr, &thd_wr, &cb);
    pthread_create(&thd[1], &attr, &thd_rd, &cb);
    pthread_join(thd[0], NULL);
    pthread_join(thd[1], NULL);
    #endif


    printf("CBUF reader/writer:\n");
    printf("The free counter:  %d\n", freecnt);
    printf("The wcnt counter:  %zu, full count:  %zu\n", wcnt, full);
    printf("The rcnt counter:  %zu, empty count: %zu\n", rcnt, empty);
    printf("The rderr counter: %zu\n", rderr);


    cbuf_destroy(&cb);

    return 0;
}
// main()
