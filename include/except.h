#ifndef EXCEPT_H
#define EXCEPT_H

#include "types.h"

// Exception handler - called from assembly stubs
void exception_handler(uint32_t int_no, uint32_t err_code);

// Register all exception handlers
void exceptions_init(void);

// Test exceptions
void exception_test(void);

#endif // EXCEPT_H
