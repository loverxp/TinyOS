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

// Load and run the forktest user program (tests fork syscall)
void run_forktest_user(void);

// Load and run the uptime user program
void run_uptime_user(void);

// Load and run the date user program
void run_date_user(void);

// Load and run the rand user program
void run_rand_user(void);

// Load and run the meminfo user program
void run_meminfo_user(void);

// Load and run the diskinfo user program
void run_diskinfo_user(void);

// File management commands (Ring 3 user programs)
void run_ls_user(const char* args);
void run_cat_user(const char* args);
void run_more_user(const char* args);
void run_write_user(const char* args);
void run_rm_user(const char* args);
void run_mkdir_user(const char* args);
void run_rmdir_user(const char* args);

#endif // LOADER_H