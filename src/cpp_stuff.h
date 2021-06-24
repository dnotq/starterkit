/**
 * @file cpp_stuff.h
 * @date Jun 5, 2021
 * @author Matthew Hagerty
 */

#ifndef SRC_CPP_STUFF_H_
#define SRC_CPP_STUFF_H_

#include "program.h"

// Avoid C++ name-mangling to allow calling from C.
#ifdef __cplusplus
extern "C" {
#endif


void imgui_draw(progdata_s *pd);

#ifdef __cplusplus
}
#endif

#endif /* SRC_CPP_STUFF_H_ */
