
/* * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 * cmd [arg]* [> filename]
 *
 */

%code requires
{

}

%union
{
  char * string;
}

%token <string> WORD PIPE
%token NOTOKEN NEWLINE STDOUT STDIN BACKGROUND STDERR STDERRTOSTDOUT
%token STDOUTANDSTDERR STDOUTAPPEND APPENDSTDOUTANDSTDERR EXIT SOURCE

%{

#include <stdbool.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "command.h"
#include "single_command.h"
#include "shell.h"

void yyerror(const char * s);
int yylex();
void source(char *);
%}

%%

goal:
  entire_command_list
  ;

entire_command_list:
     entire_command_list entire_command
  |  entire_command
  ;

entire_command:
  single_command_list io_modifier_list NEWLINE {
    execute_command(g_current_command);
    g_current_command = NULL;
  }
  |  NEWLINE
  ;

single_command_list:
  single_command_list PIPE single_command
  |  single_command
  ;

single_command:
  executable argument_list {
    if (g_current_command == NULL) {
      g_current_command = malloc(sizeof(command_t));
      create_command(g_current_command);
    }
    insert_single_command(g_current_command, g_current_single_command);
    g_current_single_command = NULL;
  }
  ;

argument_list:
  argument_list argument
  |  /* can be empty */
  ;

argument:
  WORD {
    expand_wildcards($1);

    /* handle ${_} */
    if (g_last_argument_of_last_command == NULL) {
      g_last_argument_of_last_command = malloc(sizeof(char) * 128);
    }
    g_last_argument_of_last_command = strdup(yylval.string);
  }
  ;

executable:
  WORD {
    if (g_current_single_command == NULL) {
      g_current_single_command = malloc(sizeof(single_command_t));
    }
    create_single_command(g_current_single_command);
    insert_argument(g_current_single_command, yylval.string);

    /* handle ${_} */
    /*
    if (g_last_argument_of_last_command == NULL) {
      g_last_argument_of_last_command = malloc(sizeof(char) * 128);
    }
    g_last_argument_of_last_command = strdup(yylval.string);
    */
  }
  |  EXIT {
       free(g_current_command);
       free(g_current_single_command);
       exit(0);
  }
  |  SOURCE WORD {
       source($2);
  }
  ;

io_modifier_list:
  io_modifier_list io_modifier
  |  io_modifier_list io_modifier BACKGROUND {
       g_current_command->background = true;
     }
  |  /* can be empty */
  ;

io_modifier:
  STDOUT WORD {
    int fdout;
    fdout = open($2, O_CREAT|O_WRONLY, 0664);
    close(fdout);

    if (g_current_command->out_file != NULL) {
      fprintf(stdout, "Ambiguous output redirect.\n");
    }
    g_current_command->out_file = $2;
  }
  |  STDIN WORD {
       g_current_command->in_file = $2;
  }
  |  STDERR WORD {
       g_current_command->err_file = $2;
  }
  |  STDOUTANDSTDERR WORD {
       g_current_command->out_file = $2;
       g_current_command->err_file = $2;
  }
  |  STDOUTAPPEND WORD {
       g_current_command->out_file = $2;
       g_current_command->append_out = true;
  }
  |  APPENDSTDOUTANDSTDERR WORD {
       g_current_command->out_file = $2;
       g_current_command->err_file = $2;
       g_current_command->append_out = true;
       g_current_command->append_err = true;
  }
  |  BACKGROUND {
       g_current_command->background = true;
  }
  ;

%%

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

#if 0
main()
{
  yyparse();
}
#endif
