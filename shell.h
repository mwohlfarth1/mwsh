#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>

#include "command.h"
#include "single_command.h"

void print_prompt();
void expand_wildcards(char *);
int compare_strings(const void *, const void *);
void ctrl_c();
void handle_zombies();

extern command_t *g_current_command;
extern single_command_t *g_current_single_command;
extern char * g_shell_relative_path;
extern pid_t g_last_background_pid;
extern int g_last_command_exit_code;
extern char * g_last_argument_of_last_command;
extern int * g_command_is_being_run;

#endif
