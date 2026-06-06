#ifndef LOADER_H
#define LOADER_H

#include "types.h"

// Load and run a user program (embedded binary)
void run_loaded_user(void);

// Load and run the hello user program
void run_hello_user(void);

// Load and run the echo user program with text arguments
void run_echo_user(const char* text);

// Load and run the clear user program
void run_clear_user(void);

// Load and run the help user program
void run_help_user(void);

#endif // LOADER_H