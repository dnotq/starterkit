/**
 * @file wrap_c2cpp.h
 * @date Jun 5, 2021
 * @author Matthew Hagerty
 */

#ifndef SRC_WRAP_C2CPP_H_
#define SRC_WRAP_C2CPP_H_

#include "program.h"

// Avoid C++ name-mangling to allow calling from C.
#ifdef __cplusplus
extern "C" {
#endif


void imgui_ShowDemoWindow(u32 *show);


#ifdef __cplusplus
}
#endif

#endif /* SRC_WRAP_C2CPP_H_ */
