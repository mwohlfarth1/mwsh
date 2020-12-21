#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <regex.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>

#include "command.h"
#include "single_command.h"
#include "shell.h"

command_t *g_current_command = NULL;
single_command_t *g_current_single_command = NULL;
char *g_shell_relative_path = NULL;
pid_t g_last_background_pid = 0;
int g_last_command_exit_code = 0;
char *g_last_argument_of_last_command = NULL;
int *g_command_is_being_run = NULL;

int yyparse(void);

/*
 *  Prints shell prompt
 */

void print_prompt() {
  if (isatty(0)) {
    printf("myshell>");
    fflush(stdout);
  }
} /* print_prompt() */

/*
 *  This main is simply an entry point for the program which sets up
 *  memory for the rest of the program and the turns control over to
 *  yyparse and never returns
 */

int main(int argc, char **argv) {
  if (argc == 1) {
    g_shell_relative_path = argv[0];
  }

  /* set up the signal handler for ctrl_c */

  struct sigaction sa = {0};
  sa.sa_handler = ctrl_c;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGINT, &sa, NULL)) {
    perror("sigaction");
    exit(2);
  }

  /* set up the signal handler for zombie */
  struct sigaction sa_zombie = {0};
  sa_zombie.sa_handler = handle_zombies;
  sigemptyset(&sa_zombie.sa_mask);
  sa_zombie.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa_zombie, NULL)) {
    perror("sigaction");
    exit(2);
  }

  print_prompt();
  yyparse();
} /* main() */

/*
 * This function expands any wildcards in a WORD before inserting any
 * arguments into the argument list if it is necessary.
 */

void expand_wildcards(char *in_str) {

  if ((!strchr(in_str, '*')) && (!strchr(in_str, '?'))) {
    /* if the string does not contain the '*' or '?' characters */
    /* there is nothing to expand                               */

    if (g_current_single_command == NULL) {
      g_current_single_command = malloc(sizeof(single_command_t));
      assert(g_current_single_command != NULL);
      create_single_command(g_current_single_command);
    }

    insert_argument(g_current_single_command, in_str);
    return;
  }
  else {
    /* allocate memory for the regular expression */

    char *regex = (char *) malloc(2 * strlen(in_str) + 3);
    assert(regex != NULL);

    /* convert the wildcard expression into an actual regular expression */

    char *in_pos = in_str;
    char *regex_pos = regex;
    *regex_pos++ = '^'; while (*in_pos) {
      if (*in_pos == '*') {
        *regex_pos++ = '.';
        *regex_pos++ = '*';
      }
      else if (*in_pos == '?') {
        *regex_pos++ = '.';
      }
      else if (*in_pos == '.') {
        *regex_pos++ = '\\';
        *regex_pos++ = '.';
      }
      else {
        *regex_pos++ = *in_pos;
      }
      in_pos++;
    }
    *regex_pos++ = '$';
    *regex_pos++ = '\0';

    /* fix the regex and dir_string if necessary */

    char *dir_string = NULL;
    if (!strchr(regex, '/')) {
      /* if there are no forward slashes, the directory is "." */

      dir_string = malloc(sizeof(char) * 2);
      assert(dir_string != NULL);
      dir_string = ".";
    }
    else {
      /* find the last forward slash */

      int last_forward_slash_index = 0;
      for (int i = 0; i < (int)strlen(regex); i++) {
        if (regex[i] == '/') {
          last_forward_slash_index = i;
        }
      }

      dir_string = malloc(sizeof(char) * last_forward_slash_index);
      assert(dir_string != NULL);
      if (in_str[0] == '.') {
        dir_string = strndup((regex + 2), last_forward_slash_index - 1);
      }
      else {
        dir_string = strndup((regex + 1), last_forward_slash_index);
      }

      /* fix the regex string */

      char *temp = malloc(sizeof(char) *
                         (strlen(regex) - last_forward_slash_index));
      assert(temp != NULL);

      temp = strdup((regex + last_forward_slash_index));
      temp[0] = '^';
      free(regex);
      regex = temp;
    }

    /* compile the regular expression */

    regex_t re;
    int status = regcomp(&re, regex, REG_EXTENDED|REG_NOSUB);
    if (status != 0) {
      perror("compile");
      return;
    }

    /* list directory and check entries to see if we need to add them */

    DIR *dir = opendir(dir_string);
    if (dir == NULL) {
      perror("opendir");
      return;
    }
    struct dirent *entry;
    int max_entries = 20;
    int num_entries = 0;
    char **entries = (char **) malloc(max_entries * sizeof(char *));
    assert(entries != NULL);

    while ((entry = readdir(dir)) != NULL) {
      /* check if this directory entry matches the regex */

      regmatch_t match;
      if (regexec(&re, entry->d_name, 1, &match, 0) == 0) {
        /* add this entry to the list of arguments */

        if (num_entries == max_entries) {
          /* double the size of the entries array if we need to */

          max_entries *= 2;
          entries = realloc(entries, max_entries * sizeof(char *));
          assert(entries != NULL);
        }
        entries[num_entries] = malloc(sizeof(char) * strlen(entry->d_name));
        assert(entries[num_entries] != NULL);
        entries[num_entries] = strdup(entry->d_name);
        num_entries++;
      }
    }
    closedir(dir);
    regfree(&re);


    /* sort the array of entries before inserting them as arguments */

    qsort(entries, (size_t) num_entries, sizeof(char *), compare_strings);

    for (int i = 0; i < num_entries; i++) {
      if (!(strcmp(entries[i], ".") == 0) &&
          !(strcmp(entries[i], "..") == 0)) {
        /* if the entry is not "." or "..", add it as an argument */

        int full_path_length = 0;
        if (strcmp(dir_string, ".") != 0) {
          full_path_length = strlen(dir_string) + strlen(entries[i]) + 1;
        }
        if (full_path_length == 0) {
          insert_argument(g_current_single_command, entries[i]);
        }
        else {
          char *entry_full_path = malloc(sizeof(char) * full_path_length);
          assert(entry_full_path != NULL);
          entry_full_path = strcpy(entry_full_path, dir_string);
          entry_full_path = strcat(entry_full_path, entries[i]);
          insert_argument(g_current_single_command, entry_full_path);

          /* free these entry_full_paths so there */
          /* isn't a memory leak                                          */
          free(entry_full_path);
        }
      }
    }

    free(regex);
  }
} /* expand_wildcards() */

/*
 * This function is used by the qsort() function used within expand_wildcards()
 * to determine if a string is greater than, equal to, or less than another
 * string.
 */

int compare_strings(const void *ptr_to_str_1, const void *ptr_to_str_2) {

#define GREATER 1
#define LESS -1
#define EQUAL 0

  char *string1 = *((char **) ptr_to_str_1);
  char *string2 = *((char **) ptr_to_str_2);

  if (strcmp(string1, string2) > 0) {
    return GREATER;
  }
  else if (strcmp(string1, string2) < 0) {
    return LESS;
  }
  else {
    return EQUAL;
  }

} /* compare_strings() */

/*
 * This function makes the shell ignore the ctrl-C input
 * and just print another prompt.
 */

void ctrl_c() {

  /* if ctrl+c is entered, then print a new prompt */

  printf("\n");
  print_prompt();

  return;
} /* ctrl_c() */

/*
 * This function kills a process if a zombie process is noticed
 */

void handle_zombies() {
  int status_value = 0;
  while (1) {
    pid_t ret = waitpid(-1, &status_value, WNOHANG);
    if (ret <= 0) {
      break;
    }
  }
} /* handle_zombies() */
