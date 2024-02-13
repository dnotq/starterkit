/**
 * @file cbuf.h
 *
 * @author Matthew Hagerty
 * @date Jan 10, 2023
 *
 * Requires:
 *
 *   Windows and MinGW builds: Win10.  This is due to needing VirtualAlloc2
 *   and MapViewOfFile3 to create the auto-wrapping buffer.
 *
 *   MINGW: -lmincore to access VirtualAlloc2 and MapViewOfFile3.
 *   MINGW: command line, compiler define -D, or before any other includes:
 *          #define _WIN32_WINNT 0x0A00
 *          #define NTDDI_VERSION 0x0A000005
 *
 *   MSVC: -experimental:c11atomics and -std:c11 compiler options, until such
 *   a time that atomic differences are resolved between C and C++, and they
 *   are not considered "experimental" by Microsoft.
 *
 *   *nix systems: mremap or memfd_create.  If neither are an option, modify
 *   the cbuf_create function to create a memory backing of your choice.
 *
 * Build options:
 *
 *   CBUF_IMPLEMENTATION
 *   MUST BE DEFINED prior to including cbuf.h in ONE source file in a
 *   project.  All other code can just include the cbuf.h as usual.
 *
 *   CBUF_HAVE_MREMAP
 *   Define in the same source file as CBUF_IMPLEMENTATION if the system has
 *   mremap, otherwise memfd_create will be used.  Usually defining _GNU_SOURCE
 *   is required to use mremap or memfd_create, however if Linux is detected the
 *   functions are accessed directly with syscall (defining _GNU_SOURCE is not
 *   required).  Mileage may vary, test or tweak as needed for your system.
 *
 *   _GNU_SOURCE
 *   Optionally define if wanted, ok if already being used.
 *
 *   CBUF_NO_ATOMIC
 *   Define if the non-atomic version of the code is desired.  Must be defined
 *   in all source files prior to including cbuf.h.  It is generally NOT
 *   recommended to define this and should only be considered in a system with
 *   one single-core CPU.
 *
 * ====
 *
 * Single-file header + source is a side effect of allowing the same header to
 * be used from C and C++ in the same code base.
 *
 * Circular buffer using virtual memory to facilitate automatic wrapping.
 *
 * Memory-consistency protection on a single-core CPU it is not needed.  On a
 * multi-core CPU or multi-processor system, some form of memory consistency is
 * still needed to ensure correct operation due to compilers and CPUs rearranging
 * memory accesses, CPU-local caches, etc..
 *
 * This concept is not new and there are many implementations around the
 * Internet.  The goal of this implementation is to compile (and work) across
 * Unix / Win / MinGW / MAC (eventually).
 *
 * This code is NOT a library and is not intended to be used as a library.  It
 * is compiled into the host code and all the internals are available for
 * tampering with.
 *
 * C and C++ atomics are not implemented the same way and ended up not really
 * being compatible (add snarky remark here).  If the link still works, here
 * is a summary of the problem:
 *
 * @see https://www.open-std.org/JTC1/SC22/WG14/www/docs/n2741.htm
 *
 * This is all supposed to be fixed in C++23, but so far only Clang "just works",
 * g++ and msvc need hacks.
 */


/*

# Linear Circular Buffer - Single Reader / Writer

This implementation uses virtual memory and mmap to provide a circular buffer
with hardware support for wrapping.  This is beneficial when working with
files or networks to avoid having to copy the data multiple times between
buffers.  Using virtual memory eliminates the need to manage wrapping when
reading or writing off the end of the buffer, and it works with the system's
standard C-API functions.


# Buffer Virtual Memory Mapping and Addressing

The buffer always looks like a flat array of the specified size, no matter
where in the buffer reading or writing is taking place.  This is set up
by mapping two consecutive virtual memory address ranges to the same
physical buffer in memory.

                                   Addresses
Physical|0                        max-1|0                        max-1|
Virtual |0                        max-1|max                    max*2-1|
        +------------------------------+------------------------------+
        |            buffer            |        buffer mirror         |
        +------------------------------+------------------------------+
        |012345678901234567890123456789|012345678901234567890123456789|
         r                             |
         w------------ free ---------->|
        Empty condition when w == r and free == max.

        +------------------------------+------------------------------+
        |012345678901234567890123456789|012345678901234567890123456789|
               r----- used ---->       |      r
         ----->                 w-free-|~~~~~>
        Write some data, read some data.

        +------------------------------+------------------------------+
        |012345678901234567890123456789|012345678901234567890123456789|
         ----->r------------ used -----|~~~~~>
               w                       |      w
        Full condition when w == r and free == 0.

        +------------------------------+------------------------------+
        |012345678901234567890123456789|012345678901234567890123456789|
         ----->              r-- used -|~~~~~>
               w--- free --->          |      w
        Read some data.


# Buffer Use

The cbuf_rb() and cbuf_wb() can be used to obtain read and write pointers
that can be used with standard C-API functions.  Reading and writing is linear
from these pointers, up to the maximum size of the buffer, or up to the used
(reading) or free (writing) space in the buffer.

The cbuf_used() indicates how much data is ready for reading, and
cbuf_free() indicates how much buffer is available for writing.

After data is read, cbuf_consume() is called to update the read index.
After data is written, cbuf_commit() is called to update the write index.

Example of writing to a file directly from the buffer:

   cbuf_s cbuf;
   cbuf_s *cb = &cbuf;    // convenience
   size_t def_pgsize = 0; // Use the system default VM page size
   size_t align = 4;      // 4-byte alignment requested

   s32 rtn = cbuf_create(cb, def_pgsize, align);
   assert(0 == rtn);

   size_t len = snlogfmt(cbuf_wb(cb), cbuf_free(cb),
     "Would you like to play a game?");
   cbuf_commit(cb, len);

   s32 fd = open("somefile.txt", O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
   assert(0 <= fd);

   len = cbuf_used(cb);
   ssize_t n = write(fd, cbuf_rb(cb), len);
   assert(n == (ssize_t)len);

   cbuf_consume(cb, n);

   close(fd);

   cbuf_destroy(cb);


Example of reading from a file directly into the buffer:

   cbuf_s cbuf;
   cbuf_s *cb = &cbuf;    // convenience
   size_t def_pgsize = 0; // Use the system default VM page size
   size_t align = 4;      // 4-byte alignment requested

   s32 rtn = cbuf_create(cb, def_pgsize, align);
   assert(0 == rtn);

   s32 fd = open("somefile.txt", O_RDONLY, 0);
   assert(0 <= fd);

   size_t len = cbuf_free(cb);
   ssize_t n = read(fd, cbuf_wb(cb), len);
   assert(0 <= n);
   assert(n <= (ssize_t)len);

   cbuf_commit(cb, n);

   close(fd);

   logfmt("%.*s\n", n, cbuf_rb(cb));
   cbuf_consume(cb, n);

   cbuf_destroy(cb);


# Concurrency

Atomics are used to ensure memory consistency for a single reader and writer
in their own thread.  It is up to the caller to coordinate any situation where
more than one reader or writer is accessing the buffer (a mutex or semaphore
would be suitable, for example).


# Errors

Buffer initialization might fail, and if errno is being used by the caller then
the failed system function (memfd_create, mmap, etc.) may have set an errno
value which the caller can check.

Otherwise no failure conditions are detected, i.e. passing a NULL pointer into
any of the helper functions, tampering with the buffer struct externally, etc..
The assumption is made that the caller is responsible, and if these checks are
needed then they will be implemented by the caller.

*/


#ifndef _CBUF_ATOMIC_H
#define _CBUF_ATOMIC_H

// Define CBUF_IMPLEMENTATION in a single C code compilation unit before
// including this header.

// Define CBUF_NO_ATOMIC in the calling code if memory protection is not
// required.  A single processes or a single thread that is both the reader and
// writer does not need to use the atomic variables.  If the reader and writer
// are in separate threads or the data structure is in shared memory, then
// leave the atomic option enabled.

#ifdef CBUF_NO_ATOMIC
#undef CBUF_USE_ATOMIC
#else
#define CBUF_USE_ATOMIC
#endif


#ifdef CBUF_USE_ATOMIC

#ifdef __cplusplus
#include <atomic>       // atomic types in C++
// This is required for C++ since all `atomic_*` type aliases are in
// namespace `std`, as in `std::atomic_bool`, for instance, instead of
// just `atomic_bool`.
// - clang++ "just works" without this hack (can just use the C include).
// - g++ requires this ifdef and hack.
// - msvc requires this ifdef and hack, along with specific flags to not break the C side.
using atomic_size_t = std::atomic_size_t;
#else
// https://devblogs.microsoft.com/cppblog/c11-atomics-in-visual-studio-2022-version-17-5-preview-2/
// msvc as of June 2023 requires compiler flags: -experimental:c11atomics -std:c11
// or it will blow up trying to include this header.
#include <stdatomic.h>  // atomic types
#endif

#endif /* Using atomics */


#include <stdbool.h>        // bool
#include "xyz.h"            // short types

// Avoid C++ name-mangling for C functions.
#ifdef __cplusplus
extern "C" {
#endif

/// Circular buffer structure.  Generally opaque, use helper functions.
/// Ok to direct-access values as read-only.
typedef struct t_cbuf_s {
    size_t          wr;         ///< Write index.
    size_t          rd;         ///< Read index.
    #ifdef CBUF_USE_ATOMIC
    atomic_size_t   fre;        ///< Available space in the buffer.
    #else
    size_t          fre;        ///< Available space in the buffer.
    #endif
    size_t          align;      ///< Read/write index alignment 0, 2, 4, or 8.
    size_t          amask;      ///< Pre-calculated alignment mask (align - 1).
    size_t          max;        ///< Buffer maximum, must be a multiple of the system memory page size.
    s32             lasterr;    ///< On *nix set to errno, or GetLastError() on Windows, if create fails.
    void           *buf;        ///< Pointer to double VM mapped buffer memory.
    void           *_winternal; ///< Used for Windows implementation.
} cbuf_s;


/**
 * Allocate and map a new circular buffer.
 *
 * Buffers must be a multiple of the system's VM page size.  The buffer will
 * never be less than minsize bytes, however it will be adjusted up to a
 * multiple of the system's VM page size.
 *
 * The align parameter can specify that the internal read and write indexes
 * always adjust by 1, 2, 4, or 8 bytes.  This generally allows the read
 * pointer of the buffer to be cast to a structure without causing memory
 * violations, and writes will always align on expected boundaries.
 *
 * If 2, 4, or 8 alignment is used and data writes are not exact multiples
 * of the alignment, holes will exist in the buffer where the read and
 * write indexes are adjusted based on the specified alignment.
 *
 * It is up to the caller to manage and track any needed framing in the data.
 *
 * @param[in] cb        Pointer to a circular buffer struct.
 * @param[in] minsize   Minimum size of the buffer, 0 for one system VM page.
 * @param[in] align     Align data on 1, 2, 4, or 8 byte boundaries.
 *
 * @return 0 on success, otherwise -1 (errno may contain a reason).
 */
s32 cbuf_create(cbuf_s *cb, size_t minsize, size_t align);

/**
 * Unmaps and frees the memory used by a buffer.
 *
 * After the buffer is destroyed, the structure can be reused with
 * cbuf_create() to allocate a new buffer.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 */
void cbuf_destroy(cbuf_s *cb);

/**
 * Check if the buffer is full.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return true if the buffer is full, otherwise false.
 */
bool cbuf_isfull(cbuf_s *cb);

/**
 * Check if the buffer is empty.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return true if the buffer is empty, otherwise false.
 */
bool cbuf_isempty(cbuf_s *cb);

/**
 * The number of bytes available for reading.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return The number of bytes available for reading.
 */
size_t cbuf_used(cbuf_s *cb);

/**
 * The number of bytes available for writing.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return The number of bytes available for writing.
 */
size_t cbuf_free(cbuf_s *cb);

/**
 * Returns a pointer for reading from the buffer.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return A pointer for reading from the buffer.
 */
void * cbuf_rb(cbuf_s *cb);

/**
 * Returns a pointer for writing to the buffer.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return A pointer for writing to the buffer.
 */
void * cbuf_wb(cbuf_s *cb);

/**
 * Commits data in the buffer, adjusting the write index and making the
 * data available for reading.
 *
 * The number of bytes to commit will be adjusted 'up' to the alignment
 * specified when the buffer was created.
 *
 * Commit() should be used after the data has been written by the caller.
 * For example to read data from a file directly to the buffer:
 *
 * size_t len = cbuf_free(cb);
 * ssize_t n = read(fd, cbuf_wb(cb), len);
 * assert(n == (ssize_t)len);
 * cbuf_commit(cb, n);
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 * @param[in] n     The number of bytes to commit.
 *
 * @return size_t   The number of aligned-bytes committed.
 */
size_t cbuf_commit(cbuf_s *cb, size_t n);

/**
 * Consumes data in the buffer, adjusting the read index and making the
 * data available for over-writing.
 *
 * The number of bytes to consume will be adjusted 'up' to the alignment
 * specified when the buffer was created.
 *
 * Consume() should be used after the data has been used by the caller.
 * For example to write all available data directly from the buffer to a file:
 *
 * size_t len = cbuf_used(cb);
 * ssize_t n = write(fd, cbuf_rb(cb), len);
 * assert(n == (ssize_t)len);
 * cbuf_consume(cb, n);
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 * @param[in] n     The number of bytes to consume.
 *
 * @return size_t   The number of aligned-bytes consumed.
 */
size_t cbuf_consume(cbuf_s *cb, size_t n);

/**
 * Effectively drains the buffer of all data.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return size_t   The number of aligned-bytes drained.
 */
size_t cbuf_drain(cbuf_s *cb);


#ifdef __cplusplus
}
#endif
#endif /* _CBUF_ATOMIC_H */


#ifdef CBUF_IMPLEMENTATION

#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__)
#define CBUF_IS_WIN

#if _WIN32_WINNT < 0x0A00 || NTDDI_VERSION < NTDDI_WIN10_RS4
// MinGW requires Win10 since this code needs the newer
// VirtualAlloc2 and MapViewOfFile3 functions.
#error "MSVC and MINGW require Win10, and _WIN32_WINNT >= _WIN32_WINNT_WIN10, and NTDDI_VERSION >= NTDDI_WIN10_RS4"
#endif

#else
#undef CBUF_IS_WIN
#endif


#ifdef CBUF_IS_WIN
#include <windows.h>       // Pretty much includes everything else.
#if !defined(__MINGW32__) && !defined(__MINGW64__)
// For VirtualAlloc2 and MapViewOfFile3, MSCV will use this pragma.
#pragma comment (lib, "mincore")
#endif

#else
#include <string.h>         // memset
#include <unistd.h>         // sysconf
#include <sys/mman.h>       // mmap, memfd_create
#include <sys/types.h>      // memfd_create
#include <fcntl.h>          // memfd_create
#include <sys/syscall.h>    // syscall
#include <errno.h>          // errno
#endif


// ==
//
// Internal functions.
//
//


#ifdef CBUF_IS_WIN
/**
 * Windows and MINGW specific buffer allocation.
 * Called by the external cbuf_create() function.
 */
static s32
cbuf_create_win(cbuf_s *cb, size_t bufsize) {

    void *addr_low  = NULL;
    void *addr_high = NULL;
    void *view_low  = NULL;
    void *view_high = NULL;
    HANDLE mapobj   = NULL;

    s32 rval = -1; // Default to error.

    // ====
    // Reserve a virtual address region only, no memory backing, where the
    // buffer will be mapped.
    addr_low = (PCHAR)VirtualAlloc2FromApp(
        /*process*/ NULL, /*base address*/ NULL,
        2 * bufsize,
        MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
        PAGE_NOACCESS,
        /*extended params*/ NULL, /*param count*/ 0
    );

    if ( NULL == addr_low ) {
        cb->lasterr = GetLastError();
        goto DONE;
    }

    // ====
    // Split the address region into two regions of equal size.
    if ( FALSE == VirtualFree(addr_low, bufsize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) ) {
        cb->lasterr = GetLastError();
        goto DONE;
    }

    // Compute the high region address for mapping a view.
    addr_high = ((PCHAR)addr_low + bufsize);

    // ====
    // Creates a file mapping object that is backed by the system paging file.
    mapobj = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        /*file mapping attrs*/ NULL,
        PAGE_READWRITE,
        (DWORD)(bufsize >> (sizeof(DWORD) * 8)),
        (DWORD)(bufsize & ((DWORD)-1)),
        /*name*/ NULL
    );

    if ( NULL == mapobj ) {
        cb->lasterr = GetLastError();
        goto DONE;
    }

    // ====
    // Create a view of the mapping object at the low address range.
    view_low = MapViewOfFile3FromApp(
        mapobj, /*process*/ NULL,
        addr_low, /*offset*/ 0,
        bufsize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE,
        /*extended params*/ NULL, /*param count*/ 0
    );

    if ( NULL == view_low ) {
        cb->lasterr = GetLastError();
        goto DONE;
    }

    // Low view created successfully, memory mapping now referenced by view_low.
    addr_low = NULL;

    // ====
    // Create a view of the mapping object at the high address range.
    view_high = MapViewOfFile3FromApp(
        mapobj, /*process*/ NULL,
        addr_high, /*offset*/ 0,
        bufsize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE,
        /*extended params*/ NULL, /*param count*/ 0
    );

    if ( NULL == view_high ) {
        cb->lasterr = GetLastError();
        goto DONE;
    }

    // High view created successfully, memory mapping now referenced by view_high.
    addr_high = NULL;


    // Check that the mappings are end-to-end.
    if ( ((PCHAR)view_low + bufsize) != view_high ) {
        goto DONE;
    }

    // Check that the mappings overlap the same memory.
    *(c8 *)view_low = 'x';
    if ( 'x' != *(c8 *)view_high ) {
        goto DONE;
    }

    *(c8 *)view_high = 'y';
    if ( 'y' != *(c8 *)view_low ) {
        goto DONE;
    }

    *(c8 *)view_low = '\0';

    // ====
    // Success, update the cbuf.
    cb->buf = view_low;
    cb->_winternal = view_high;
    cb->max = bufsize;
    cb->fre = bufsize;

    view_low  = NULL;
    view_high = NULL;

    rval = 0;

DONE:

    // Clean up.

    // Close the mapping file.  If the views were successful, they will maintain
    // references to the mapping until they are closed.
    if ( NULL != mapobj ) {
        CloseHandle(mapobj);
    }

    if ( NULL != addr_low ) {
        VirtualFree(addr_low, /*size*/ 0, MEM_RELEASE);
    }

    if ( NULL != addr_high ) {
        VirtualFree(addr_high, /*size*/ 0, MEM_RELEASE);
    }

    if ( NULL != view_low ) {
        UnmapViewOfFile(view_low);
    }

    if ( NULL != view_high) {
        UnmapViewOfFile(view_high);
    }

    return rval;
}
// cbuf_create_win()


#else
// ====
// *nix specific support for the virtual memory mapping.
//

// Define if the system has mremap support.
#ifdef CBUF_HAVE_MREMAP

// Potentially dangerous because the parameter needs to be the same as the
// system header (which may change).  Maybe better to test if using _GNU_SOURCE
// is actually acceptable for compiling this file, or don't use mremap().
#ifndef MREMAP_MAYMOVE
#define MREMAP_MAYMOVE  1
#endif

#ifndef MREMAP_FIXED
#define MREMAP_FIXED 2
#endif


/**
 * Wrapper for mremap() with 4 parameters.
 *
 * Allows calling mremap() on Linux systems without having to define
 * the _GNU_SOURCE macro before system header (which makes the code much
 * less portable).
 *
 * mremap() is variadic for the last parameter.  Using two functions
 * to wrap the system call makes for an easier wrapper.
 *
 * @param oldaddr same as mremap()
 * @param oldsize same as mremap()
 * @param newsize same as mremap()
 * @param flags   same as mremap()
 *
 * @return same as mremap()
 */
static void *
wrap_mremap4(void *oldaddr, size_t oldsize, size_t newsize, s32 flags) {
    #if defined(__linux__)
    // Access mremap() without having to define _GNU_SOURCE before
    // any system headers.
    // mremap is variadic for the 5th parameter.
    return (void *)syscall(__NR_mremap, oldaddr, oldsize, newsize, flags);
    #else
    return (void *)mremap(oldaddr, oldsize, newsize, flags);
    #endif
}
// wrap_mremap4()


/**
 * Wrapper for mremap() with 5 parameters.
 *
 * Allows calling mremap() on Linux systems without having to define
 * the _GNU_SOURCE macro before system header (which makes the code much
 * less portable).
 *
 * mremap() is variadic for the last parameter.  Using two functions
 * to wrap the system call makes for an easier wrapper.
 *
 * @param oldaddr same as mremap()
 * @param oldsize same as mremap()
 * @param newsize same as mremap()
 * @param flags   same as mremap()
 * @param newaddr same as mremap()
 *
 * @return same as mremap()
 */
static void *
wrap_mremap5(void *oldaddr, size_t oldsize, size_t newsize, s32 flags, void *newaddr) {
    #if defined(__linux__)
    // Access mremap() without having to define _GNU_SOURCE before
    // any system headers.
    // mremap is variadic for the 5th parameter.
    return (void *)syscall(__NR_mremap, oldaddr, oldsize, newsize, flags, newaddr);
    #else
    return (void *)mremap(oldaddr, oldsize, newsize, flags, newaddr);
    #endif
}
// wrap_mremap5()

#else

/**
 * Wrapper for memfd_create()
 *
 * Allows calling memfd_create() on Linux systems without having to define
 * the _GNU_SOURCE macro before system header (which makes the code much
 * less portable).
 *
 * @param name  same as memfd_create()
 * @param flags same as memfd_create()
 *
 * @return same as memfd_create()
 */
static s32
wrap_memfd_create(const c8* name, u32 flags) {
    #if defined(__linux__)
    // Access memfd_create() without having to define _GNU_SOURCE.
    return syscall(__NR_memfd_create, name, flags);
    #else
    return memfd_create(name, flags);
    #endif
}
// wrap_memfd_create()

#endif


/**
 * *nix specific buffer allocation.
 * Called by the external cbuf_create() function.
 */
static s32
cbuf_create_nix(cbuf_s *cb, size_t bufsize) {

    // Initialize for error detection.
    s32 memfd = -1;
    c8 *addr = MAP_FAILED;
    c8 *wrap = MAP_FAILED;

    s32 rval = -1; // Default to error.

    #ifdef CBUF_HAVE_MREMAP

    // Create an anonymous VM buffer twice the size of the requested buffer.
    // The kernel will choose the address range.
    addr = mmap(/*noaddr*/ NULL, (bufsize * 2)
        , PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS
        , /*nofile*/ -1, /*offset*/ 0);
    if ( MAP_FAILED == addr ) {
        goto DONE;
    }

    // Shrink the mapping to the actual buffer size.
    //     mremap(oldaddr, oldsize, newsize, flags, ...);
    addr = wrap_mremap4(addr, (bufsize * 2), bufsize, /*noflags*/ 0);
    if ( MAP_FAILED == addr ) {
        goto DONE;
    }

    // Create a second mapping of the same pages (indicated by oldsize=0) with
    // an address following the first mapping.
    //     mremap(oldaddr, oldsize, newsize, flags, newaddr);
    wrap = wrap_mremap5(addr, /*oldsize*/ 0, bufsize,
        MREMAP_FIXED | MREMAP_MAYMOVE, addr + bufsize);
    if ( MAP_FAILED == wrap ) {
        goto DONE;
    }

    // ==
    // MEMFD or SHM backed buffer
    //
    #else

    // If memfd_create() is not available, shm_open() can be used, or
    // a temp file could be created and unlinked.

    // Create a backing memory buffer and associated a descriptor.  The name is
    // not used by the kernel and does not have to be unique.
    memfd = wrap_memfd_create("cbuf", /*noflags*/ 0);
    if ( -1 == memfd ) {
        goto DONE;
    }

    // Set the memory buffer size.
    if ( -1 == ftruncate(memfd, bufsize) ) {
        goto DONE;
    }

    // Create an anonymous VM buffer twice the size of the buffer.  The kernel
    // will choose the address range.  Memory will be allocated by this call,
    // but it will be replaced/released in the next two mmap(MAP_FIXED) calls.
    addr = mmap(/*noaddr*/ NULL, (bufsize * 2)
        , PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, /*nofile*/ -1, /*offset*/ 0);
    if ( MAP_FAILED == addr ) {
        goto DONE;
    }

    // The MAP_FIXED parameter causes overlapping mappings to be released.

    // anonymous mapping to be released.
    // Create the first mapping and associate it with the descriptor created
    // previously.
    wrap = mmap(addr, bufsize
        , PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, memfd, /*offset*/ 0);
    if ( MAP_FAILED == wrap ) {
        goto DONE;
    }

    // Create the second mapping of the same descriptor with an address
    // following the first mapping.
    wrap = mmap(addr + bufsize, bufsize
        , PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, memfd, /*offset*/ 0);
    if ( MAP_FAILED == wrap ) {
        goto DONE;
    }

    #endif

    // Check that the mappings are end-to-end.
    if ( (addr + bufsize) != wrap ) {
        goto DONE;
    }

    // Check that the mappings overlap the same memory.
    *addr = 'x';
    if ( 'x' != *wrap ) {
        goto DONE;
    }

    *wrap = 'y';
    if ( 'y' != *addr ) {
        goto DONE;
    }

    *addr = '\0';

    // All good, initialize the buffer.
    cb->buf = addr;
    cb->max = bufsize;
    cb->fre = bufsize;

    rval = 0;

DONE:

    // The memory file descriptor is not needed once mapped, and can be closed.
    if ( 0 <= memfd ) {
        close(memfd);
    }

    // Unmap if anything failed.
    if ( NULL == cb->buf && MAP_FAILED != addr ) {
        munmap(addr, bufsize * 2);
    }

    return rval;
}
// cbuf_create_nix()
#endif


// ==
//
// External API
//
//


/**
 * Allocate and map a new circular buffer.
 *
 * Buffers must be a multiple of the system's VM page size.  The buffer will
 * never be less than minsize bytes, however it will be adjusted up to a
 * multiple of the system's VM page size.
 *
 * The align parameter can specify that the internal read and write indexes
 * always adjust by 1, 2, 4, or 8 bytes.  This generally allows the read
 * pointer of the buffer to be cast to a structure without causing memory
 * violations, and writes will always align on expected boundaries.
 *
 * If 2, 4, or 8 alignment is used and data writes are not exact multiples
 * of the alignment, holes will exist in the buffer where the read and
 * write indexes are adjusted based on the specified alignment.
 *
 * It is up to the caller to manage and track any needed framing in the data.
 *
 * @param[in] cb        Pointer to a circular buffer struct.
 * @param[in] minsize   Minimum size of the buffer, 0 for one system VM page.
 * @param[in] align     Align data on 1, 2, 4, or 8 byte boundaries.
 *
 * @return 0 on success, otherwise -1 (errno may contain a reason).
 */
s32
cbuf_create(cbuf_s *cb, size_t minsize, size_t align) {

    memset(cb, 0, sizeof(cbuf_s));

    // Make a default page size in case the system's page size cannot
    // be determined at runtime.  This many or many not work / cause
    // problems with the mapping, but defer any error until something fails.
    size_t pagesize = 1024 * 4;
    #ifdef CBUF_IS_WIN
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    size_t sys_pgsize = (size_t)sysInfo.dwPageSize;
    #else
    ssize_t sys_pgsize = sysconf(_SC_PAGESIZE);
    #endif
    if ( 0 < sys_pgsize ) {
        pagesize = (size_t)sys_pgsize;
    }

    // Round up the minimum buffer size to nearest multiple of a page size.
    size_t bufsize = (minsize & ~(pagesize - 1));
    if ( bufsize < pagesize || 0 != (minsize % pagesize) ) {
        bufsize += pagesize;
    }

    // Prevent overflow when doubling the buffer for mapping.
    // Shift the MSbit down to bit-0 and check that it is zero.
    if ( 0 != (bufsize >> ((sizeof(bufsize) * 8) - 1)) ) {
        bufsize /= 2;
    }

    // Precompute the alignment mask if alignment is specified.
    if ( 2 == align || 4 == align || 8 == align ) {
        cb->align = align;
        cb->amask = (align - 1);
    }

    #ifdef CBUF_IS_WIN
    return cbuf_create_win(cb, bufsize);
    #else
    return cbuf_create_nix(cb, bufsize);
    #endif
}
// cbuf_create()


/**
 * Unmaps and frees the memory used by a buffer.
 *
 * After the buffer is destroyed, the structure can be reused with
 * cbuf_create() to allocate a new buffer.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 */
void
cbuf_destroy(cbuf_s *cb) {

    #ifdef CBUF_IS_WIN
    if ( NULL != cb->buf ) {
        UnmapViewOfFile(cb->buf);
    }

    if ( NULL != cb->_winternal ) {
        UnmapViewOfFile(cb->_winternal);
    }

    #else
    if ( NULL != cb->buf ) {
        munmap(cb->buf, cb->max * 2);
    }
    #endif

    memset(cb, 0, sizeof(cbuf_s));
}
// cbuf_destroy()


/**
 * Check if the buffer is full.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return true if the buffer is full, otherwise false.
 */
bool
cbuf_isfull(cbuf_s *cb) {
    #ifdef CBUF_USE_ATOMIC
    return 0 == atomic_load(&cb->fre) ? true : false;
    #else
    return 0 == cb->fre ? true : false;
    #endif
}
// cbuf_isfull()


/**
 * Check if the buffer is empty.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return true if the buffer is empty, otherwise false.
 */
bool
cbuf_isempty(cbuf_s *cb) {
    #ifdef CBUF_USE_ATOMIC
    size_t fre = atomic_load(&cb->fre);
    return fre == cb->max ? true : false;
    #else
    return cb->fre == cb->max ? true : false;
    #endif
}
// cbuf_isempty()


/**
 * The number of bytes available for reading.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return The number of bytes available for reading.
 */
size_t
cbuf_used(cbuf_s *cb) {
    #ifdef CBUF_USE_ATOMIC
    size_t fre = atomic_load(&cb->fre);
    return cb->max - fre;
    #else
    return cb->max - cb->fre;
    #endif
}
// cbuf_used()


/**
 * The number of bytes available for writing.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return The number of bytes available for writing.
 */
size_t
cbuf_free(cbuf_s *cb) {
    #ifdef CBUF_USE_ATOMIC
    return atomic_load(&cb->fre);
    #else
    return cb->fre;
    #endif
}
// cbuf_free()


/**
 * Returns a pointer for reading from the buffer.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return A pointer for reading from the buffer.
 */
void *
cbuf_rb(cbuf_s *cb) {
    return (u8 *)cb->buf + cb->rd;
}
// cbuf_rb()


/**
 * Returns a pointer for writing to the buffer.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return A pointer for writing to the buffer.
 */
void *
cbuf_wb(cbuf_s *cb) {
    return (u8 *)cb->buf + cb->wr;
}
// cbuf_wb()


/**
 * Commits data in the buffer, adjusting the write index and making the
 * data available for reading.
 *
 * The number of bytes to commit will be adjusted 'up' to the alignment
 * specified when the buffer was created.
 *
 * Should be used after the data has been written by the caller.
 * For example to read data from a file directly to the buffer:
 *
 * size_t len = cbuf_free(cb);
 * ssize_t n = read(fd, cbuf_wb(cb), len);
 * assert(n == (ssize_t)len);
 * cbuf_commit(cb, n);
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 * @param[in] n     The number of bytes to commit.
 *
 * @return size_t   The number of aligned-bytes committed.
 */
size_t
cbuf_commit(cbuf_s *cb, size_t n) {

    if ( 0 < cb->align ) {
        size_t aerr = n & cb->amask;    // save any alignment error.
        n &= (~cb->amask);              // force alignment.
        if ( 0 != aerr ) {
            n += cb->align;             // adjust for alignment.
        }
    }

    // Because n will always be an aligned (as configured) offset,
    // fre will always have an aligned value.
    #ifdef CBUF_USE_ATOMIC
    size_t fre = atomic_load(&cb->fre);
    if ( n > fre  ) {
        n = fre;
    }

    atomic_fetch_sub(&cb->fre, n);

    #else
    if ( n > cb->fre ) {
        n = cb->fre;
    }

    cb->fre -= n;
    #endif

    cb->wr = (cb->wr + n) % cb->max;
    return n;
}
// cbuf_commit()


/**
 * Consumes data in the buffer, adjusting the read index and making the
 * data available for over-writing.
 *
 * The number of bytes to consume will be adjusted 'up' to the alignment
 * specified when the buffer was created.
 *
 * Should be used after the data has been used by the caller.
 * For example to write all available data directly from the buffer to a file:
 *
 * size_t len = cbuf_used(cb);
 * ssize_t n = write(fd, cbuf_rb(cb), len);
 * assert(n == (ssize_t)len);
 * cbuf_consume(cb, n);
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 * @param[in] n     The number of bytes to consume.
 *
 * @return size_t   The number of aligned-bytes consumed.
 */
size_t
cbuf_consume(cbuf_s *cb, size_t n) {

    if ( 0 < cb->align ) {
        size_t aerr = n & cb->amask;    // save any alignment error.
        n &= (~cb->amask);              // force alignment.
        if ( 0 != aerr ) {
            n += cb->align;             // adjust for alignment.
        }
    }

    // Because fre and max will always be aligned values,
    // 'used' will always have an aligned value.
    #ifdef CBUF_USE_ATOMIC
    size_t used =  cb->max - atomic_load(&cb->fre);
    if ( n > used ) {
        n = used;
    }

    atomic_fetch_add(&cb->fre, n);

    #else
    size_t used = cb->max - cb->fre;
    if ( n > used ) {
        n = used;
    }

    cb->fre += n;
    #endif

    cb->rd = (cb->rd + n) % cb->max;
    return n;
}
// cbuf_consume()


/**
 * Effectively drains (consumes) the buffer of all data.
 *
 * @note Called by the reader.
 *
 * @param[in] cb    Pointer to a circular buffer struct.
 *
 * @return size_t   The number of aligned-bytes drained.
 */
size_t
cbuf_drain(cbuf_s *cb) {
    #ifdef CBUF_USE_ATOMIC
    size_t fre = atomic_exchange(&cb->fre, cb->max);
    size_t drained = cb->max - fre;
    #else
    size_t drained = cb->max - cb->fre;
    cb->fre = cb->max;
    #endif

    cb->rd = (cb->rd + drained) % cb->max;
    return drained;
}
// cbuf_drain()


#endif /* CBUF_IMPLEMENTATION */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2023 Matthew Hagerty
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
