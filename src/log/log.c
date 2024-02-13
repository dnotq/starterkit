/**
 * @file log.c
 *
 * Logging support.
 *
 * @date Nov 3, 2022
 * @version 1.0
 * @author Matthew Hagerty
 *
 * Logging support (expand description...)
 *
 * Font check: 0O1lLi
 *
 * Change log (initials yyyymmdd description)
 * MH   20221104    Initial creation.
 * MH   20230314    Reorganize to support no stdout and allow caller to
 *                  implement a logging call-back.
 */

#include <stdio.h>      // FILE, stdin, stdout, fwrite, fputc, fflush
#include <stddef.h>     // NULL
#include <stdint.h>     // uintXX_t, intXX_t, UINTXX_MAX, INTXX_MAX, etc.
#include <stdarg.h>     // va_args macros

// Include the stb_sprintf code if it has not been included elsewhere (i.e.
// also being used by IMGUI).
#if !defined(STB_SPRINTF_IMPLEMENTATION) && !defined(IMGUI_USE_STB_SPRINTF)
#define STB_SPRINTF_IMPLEMENTATION
#endif

// If float and double support is not needed, define STB_SPRINTF_NOFLOAT
// which will save about 4K of code space.
//#define STB_SPRINTF_NOFLOAT
#include "stb_sprintf.h"

#include "log.h"


/// Provide a non-NULL default for the global caller-callback.
static void
dummy_cout_fn(const char *buf, void *cdata, int32_t len) {

    (void)(buf);
    (void)(cdata);
    (void)(len);
}
// dummy_cout_fn()


/// Caller defined log output function, set to a non-NULL address so NULL
/// can be used by the caller to disable logging if desired.
static t_log_output_fn *g_cout_fn = dummy_cout_fn;

/// Caller defined data to pass to the specified log output call-back.
static void *g_cdata = NULL;

/// Log data struct for stbsp_vsprintfcb callback use.
typedef struct t_log_s {
    FILE   *fp;                     ///< File pointer to use when applicable.
    char    buf[STB_SPRINTF_MIN];   ///< Working buffer for stbsp_vsprintfcb.
} log_s;


/**
 * stbsp_vsprintfcb call-back wrapper for a caller-provided call-back.
 *
 * @param[in]   buf     Formatted data to write.
 * @param[in]   user    Pointer to a log_s struct.
 * @param[in]   len     Length of data in buf to write.
 *
 * @return char *   Pointer to a buffer for stbsp_vsprintfcb to use,
 *          or NULL to terminate the printing.
 */
static char *
log_callback(const char *buf, void *user, int32_t len) {

    if ( NULL == user || NULL == buf ) {
        return NULL;
    }

    log_s *log = (log_s *)user;

    if ( 0 < len ) {
        g_cout_fn(buf, g_cdata, (uint32_t)len);
    }

    return log->buf;
}
// log_callback()


/**
 * stb_sprintf call-back wrapper for output to a file stream.
 *
 * @param[in]   buf     Formatted data to write.
 * @param[in]   user    Pointer to a log_s struct.
 * @param[in]   len     Length of data in buf to write.
 *
 * @return char *   Pointer to a buffer for stbsp_vsprintfcb to use,
 *          or NULL to terminate the printing.
 */
static char *
log_callback_fp(const char *buf, void *user, int32_t len) {

    if ( NULL == user || NULL == buf ) {
        return NULL;
    }

    log_s *log = (log_s *)user;

    if ( NULL != log->fp && 0 < len && 0 < fwrite(buf, len, 1, log->fp) ) {
        fflush(log->fp);
    }

    return log->buf;
}
// log_callback_fp()


/// Default logfmt callback, changed if caller sets a callback.
static STBSP_SPRINTFCB *g_logfmt_callback = log_callback_fp;


//
// External Functions
//


/**
 * Set the logging output function, default is stdout.
 *
 * Assign the function used to write log data.  The assignment cannot fail,
 * setting the call-back to NULL will result in disabling log output.
 *
 * @param[in] cdata         Pointer to caller data to pass to the call-back.
 * @param[in] log_cout_fn   Pointer to the log output function, or NULL to disable logging output.
 */
void
log_set_output_fn(void *cdata, t_log_output_fn *log_cout_fn) {

    g_cdata = cdata;
    g_cout_fn = log_cout_fn;

    // Change the callback for logfmt.
    g_logfmt_callback = log_callback;
    return;
}
// log_set_output_fn()


/**
 * Write a formatted string (like printf).
 *
 * Converts a formatted string and writes it to the system designated output,
 * just like printf.
 *
 * @param[in]   fmt     Format string.
 * @param[in]   ...     Argument list.
 *
 * @return int32_t  Length of the formatted string.
 */
int32_t
logfmt(const char *fmt, ...) {

    if ( NULL == g_cout_fn || NULL == fmt ) {
        return 0;
    }

    // Create a call-specific buffer to convert into.
    log_s log;
    // Use stdout if the caller has not set an output call-back.
    log.fp = stdout;

    va_list args;
    va_start(args, fmt);
    int32_t len = stbsp_vsprintfcb(g_logfmt_callback, &log, log.buf, fmt, args);
    va_end(args);

    return len;
}
// logfmt()


/**
 * Write a formatted string to a file pointer (like fprintf).
 *
 * Converts a formatted string and writes it to the specified file pointer,
 * just like fprintf.
 *
 * @param[in]   fp      File pointer to write to.
 * @param[in]   fmt     Format string.
 * @param[in]   ...     Argument list.
 *
 * @return int32_t  Length of the formatted string.
 */
int32_t
flogfmt(FILE *fp, const char *fmt, ...) {

    if ( NULL == fp || NULL == fmt ) {
        return 0;
    }

    log_s log;
    log.fp = fp;

    va_list args;
    va_start(args, fmt);
    int32_t len = stbsp_vsprintfcb(log_callback_fp, &log, log.buf, fmt, args);
    va_end(args);

    return len;
}
// flogfmt()


/**
 * Convert a formatted string into a buffer (like snprintf).
 *
 * Converts a formatted string into the specified buffer (like snprintf).
 * Always zero-terminates the buffer (unlike snprintf).
 *
 * @note The return value is the full length of the converted string, however
 * the length of data written to buf will always be limited to len, and will
 * always be zero-terminated.
 *
 * @param[in]   buf     Buffer to convert fmt into.
 * @param[in]   len     Length of buf.
 * @param[in]   fmt     Format string.
 * @param[in]   ...     Argument list.
 *
 * @return int32_t  Total length of the formatted string, which may be larger
 *          than the specified buffer length.
 */
int32_t
snlogfmt(char *buf, size_t len, const char *fmt, ...) {

    if ( NULL == buf || NULL == fmt || 0 == len ) {
        return 0;
    }

    // Limit the length to a positive signed value since the len input to
    // the stbsp_vsnprintf function is a signed int.
    if ( INT32_MAX < len ) {
        len = INT32_MAX;
    }

    va_list args;
    va_start(args, fmt);
    int32_t rtnlen = stbsp_vsnprintf(buf, (int32_t)len, fmt, args);
    va_end(args);

    return rtnlen;
}
// snlogfmt()
