/**
 * @file log.h
 * @brief Logging support.
 *
 * @date Nov 3, 2022
 * @version 1.0
 * @author Matthew Hagerty
 *
 * Font check: 0O1lLi
 *
 * Change log (initials yyyymmdd description)
 * MH   20221104    Initial creation.
 * MH   20230314    Added call-back support.
 */

#pragma once

#include <stdio.h>      // FILE, stdin, stdout, fwrite, fputc, fflush
#include <stdint.h>     // uintXX_t, intXX_t, UINTXX_MAX, INTXX_MAX, etc.

// Avoid C++ name-mangling for C functions.
#ifdef __cplusplus
extern "C" {
#endif

/*
See stb_sprintf.h for full details.

allowed types:  sc uidBboXx p AaGgEef n
lengths      :  hh h ll j z t I64 I32 I

Supports 64-bit integers with MSVC or GCC style indicators (%I64d or %lld).
Supports the C99 specifiers for size_t and ptr_diff_t (%jd %zd).

For integers and floats a ' (single quote) specifier will cause commas to be
inserted on the thousands:

  "%'d" for 12345 is "12,345"

For integers and floats "$" specifier converts the number to a float and then
divides to get kilo, mega, giga or tera and then printed:

  "%$d" for 1000 is "1.0 k"
  "%$.2d" for 2536000 is "2.53 M"

For byte values, use two $:s so:

  "%$$d" for 2536000 is "2.42 Mi"

If JEDEC suffixes are preferred, use three "$" as:

  $:s: "%$$$d" -> "2.42 M"

To remove the space between the number and the suffix, add "_" specifier:

  "%_$d" -> "2.53M"

In addition to octal and hexadecimal conversions, integers can be printed
in binary:

  "%b" for 256 is "100"
*/

/// Logging output call-back function prototype.  Implement this function
/// if stdout is not available, other log output is needed, or to disable
/// all logging output.
typedef void (t_log_output_fn)(const char *buf, void *cdata, int32_t len);

/// Set the logging output call-back function.
void log_set_output_fn(void *cdata, t_log_output_fn *log_cout_fn);

/// Write formatter output to the log, replaces printf.
int32_t logfmt(const char *fmt, ...);

/// Write formatted output to a file-stream, replaces fprintf.
int32_t flogfmt(FILE *fp, const char *fmt, ...);

/// Write formatted output to a buffer, replaces snprintf.
int32_t snlogfmt(char *buf, size_t len, const char *fmt, ...);

#ifdef __cplusplus
}
#endif
