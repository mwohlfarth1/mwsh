/*
 *
 * CS-252
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

#include "shell.h"
#include "y.tab.h"
#include "read_line.h"

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

////////// Start added code ////////
/* i did not implement this, so it is commented out */
/*
extern char * read_line();

int mygetc(FILE * f) {
  static char *p;
  char ch;

  if (!isatty(0)) {
    // stdin is not a tty. call real getc
    return getc(f);
  }

  // stdin is a tty. call out read_line

  if ((p == NULL) || (*p == 0)) {
    char * s = read_line();
    p = s;
  }

  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)
*/
//////// End added code /////////

YY_BUFFER_STATE g_temp_buffer;

void source(char *filename) {

  g_temp_buffer = YY_CURRENT_BUFFER;
  yyin = fopen(filename, "r");
  if (!yyin) {
    perror("fopen");
  }
  
  yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
  BEGIN(0);
}

%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return STDOUT;
}

">>" {
  return STDOUTAPPEND;
}

">&" {
  return STDOUTANDSTDERR;
}

">>&" {
  return APPENDSTDOUTANDSTDERR;
}

"|" {
  return PIPE;
}

"<" {
  return STDIN;
}

"&" {
  return BACKGROUND;
}

"2>" {
  return STDERR;
}

"2>&1" {
  return STDERRTOSTDOUT;
}

"exit" {
  return EXIT;
}

"source" {
  /*
  g_temp_buffer = YY_CURRENT_BUFFER;
  yyin = fopen(yytext, "r");
  if (!yyin) {
    perror("fopen");
  }
  
  yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
  BEGIN(INITIAL);
  */
  return SOURCE;
}

<<EOF>> {
    yy_delete_buffer(YY_CURRENT_BUFFER);
    yy_switch_to_buffer(g_temp_buffer);
}

[$][{][^\t\n}]*[}] {
  /* environment variable expansion */

  if (!strcmp(yytext, "${$}")) {
    /* substitute ${$} with the PID of the shell process */

    pid_t current_pid = getpid();
    char * temp = malloc(sizeof(char) * 128);
    sprintf(temp, "%d", (int) current_pid);

    int temp_index = strlen(temp) - 1;
    for (temp_index; temp_index >= 0; temp_index--) {
      unput(temp[temp_index]);
    }
    free(temp);
    temp = NULL;
  }
  else if (!strcmp(yytext, "${SHELL}")) {
    /* substitute ${SHELL} with the path of the shell executable */

    char * resolved_path = malloc(sizeof(char) * 512);
    char * realpath_ret = realpath(g_shell_relative_path, resolved_path);
    if (realpath_ret != resolved_path) {
      exit(1);
    }

    int path_index = strlen(realpath_ret) - 1;
    for (path_index; path_index >= 0; path_index--) {
      unput(realpath_ret[path_index]);
    }
    free(resolved_path);
    resolved_path = NULL;
  }
  else if (!strcmp(yytext, "${?}")) {
    /* substitute ${?} with the exit code of the last simple command */

    char * temp = malloc(sizeof(char) * 128);
    sprintf(temp, "%d", g_last_command_exit_code);

    int temp_index = strlen(temp) - 1;
    for (temp_index; temp_index >= 0; temp_index--) {
      unput(temp[temp_index]);
    }
    free(temp);
    temp = NULL;
  }
  else if (!strcmp(yytext, "${!}")) {
    /* substitute ${!} with the PID of the last background process */

    char * temp = malloc(sizeof(char) * 128);
    sprintf(temp, "%d", (int) g_last_background_pid);

    int temp_index = strlen(temp) - 1;
    for (temp_index; temp_index >= 0; temp_index--) {
      unput(temp[temp_index]);
    }
    free(temp);
    temp = NULL;
  }
  else if (!strcmp(yytext, "${!!}")) {
    /* run the last command again */

  }
  else if (!strcmp(yytext, "${_}")) {
    /* substitute ${_} with the last argument of the fully expanded previous */
    /* command                                                               */

    if (g_last_argument_of_last_command != NULL) {
      int temp_index = strlen(g_last_argument_of_last_command) - 1;
      for (temp_index; temp_index >= 0; temp_index--) {
        unput(g_last_argument_of_last_command[temp_index]);
      }
    }
  }
  else {
    /* replace ${...} with the correct environment variable */

    /* extract the environment variable name from the brackets */

    yytext[strlen(yytext) - 1] = '\0';
    char *env_var_name = &(yytext[2]);

    /* find the environment variable with that name */

    char *env_var_contents = NULL;
    env_var_contents = getenv(env_var_name);
    if (env_var_contents == NULL) {
      perror("getenv");
      exit(1);
    }

    int contents_index = strlen(env_var_contents) - 1;
    for (contents_index; contents_index >= 0; contents_index--) {
      unput(env_var_contents[contents_index]);
    }
  }
}

[$][(][^\t\n)]*[)] {
  /* if &(...) is seen, generate a subshell */

  yytext[strlen(yytext) - 1] = '\0';
  char *subshell_command = &(yytext[2]);

  /* save stdin and stdout so that they can be restored later */

  int default_in = dup(0);
  int default_out = dup(1);

  /* create pipes for input and output to and from subshell, respectively */

  int fdpipeto[2];
  int fdpipefrom[2];
  pipe(fdpipeto);
  pipe(fdpipefrom);

  /* send the input through the pipe to the subshell */

  write(fdpipeto[1], subshell_command, strlen(subshell_command));

  /* send a newline so that it actually runs that command */

  write(fdpipeto[1], "\n", 1);

  /* tell the subshell to exit so that it doesn't keep running */

  write(fdpipeto[1], "exit\n", 5);
  close(fdpipeto[1]);

  /* change stdin to be the read end of the input pipe and stdout to be the */
  /* write end of the output pipe. this may not be the best way to send     */
  /* input and output to the shell, but it is certainly the easiest to      */
  /* figure out                                                             */

  dup2(fdpipeto[0], 0);
  dup2(fdpipefrom[1], 1);
  close(fdpipeto[0]);
  close(fdpipefrom[1]);

  /* fork the child and run the command */

  int ret = fork();
  if (ret == 0) {
    /* this is the child process */

    /* create a pointer for execvp's second argument so we don't get a */
    /* warning on compilation by specifying "NULL" as the second       */
    /* argument                                                        */

    char **null_pointer = NULL;

    /* run the subshell */

    execvp("/proc/self/exe", null_pointer);
    perror("execvp");
    exit(1);
  }

  /* change in and out back to their defaults */

  dup2(default_in, 0);
  dup2(default_out, 1);
  close(default_in);
  close(default_out);

  /* put the output of the subshell command into a buffer */

  char ch;
  char * buffer = malloc(sizeof(char) * 5096);
  int buffer_index = 0;
  while (read(fdpipefrom[0], &ch, 1)) {
    if (ch == '\n') {
      buffer[buffer_index] = ' ';
      buffer_index++;
    }
    else {
      buffer[buffer_index] = ch;
      buffer_index++;
    }
  }

  buffer[buffer_index] = '\0';

  /* put that buffer into the input for the original command which spawned */
  /* the subshell command                                                  */

  buffer_index = strlen(buffer) - 1;
  for (buffer_index; buffer_index >= 0; buffer_index--) {
    unput(buffer[buffer_index]);
  }
}

["][^\n\"]*["] {
  /* adds support for quotes */
  /* if a quote, followed by (not a newline or quote) 0 or more times, */
  /* followed by a quote is found, we know we need to deal with quotes */


  /* yytext is already big enough to fit the entire string with quotes */
  /* so we don't need to make another string, only shorten the string  */
  /* that is in yytext                                                 */

  int index = 0;
  while (1) {
    if (yytext[index + 1] == '\0') {
      yytext[index - 1] = yytext[index + 1];
      break;
    }
    else {
      yytext[index] = yytext[index + 1];
      index++;
    }
  }

  yylval.string = strdup(yytext);
  return WORD;
}

[^ \t\n|<>&]*\\[^ \t\n]* {
  /* account for escape characters */

  /* if something valid, followed by a backslash, followed by something */
  /* valid: escape the escaped characters and pass on the string        */

  char * temp_string = malloc(sizeof(char) * strlen(yytext));
  if (temp_string == NULL) {
    return 1;
  }

  /* go through the entire string of yytext and put the correct string into */
  /* temp_string                                                            */

  int yytext_index = 0;
  int temp_index = 0;
  while (1) {
    if (yytext[yytext_index] == '\0') {
      temp_string[temp_index] = '\0';
      break;
    }
    else if (yytext[yytext_index] == '\\') {
      temp_string[temp_index] = yytext[yytext_index + 1];
      yytext_index++;
    }
    else {
      temp_string[temp_index] = yytext[yytext_index];
    }
    yytext_index++;
    temp_index++;
  }

  yylval.string = strdup(temp_string);

  /* free the temp_string */

  free(temp_string);
  temp_string = NULL;

  return WORD;
}

[^ \t\n|<>&][^ \t\n|<>&]* {
  /* Assume that file names have only alpha chars */

  yylval.string = strdup(yytext);
  return WORD;
}
