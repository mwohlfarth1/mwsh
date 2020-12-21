#include <stdio.h>
#include <stdlib.h>

#include "shell.h"
#include "single_command.h"

/*
 *  Initialize a single command
 */

void create_single_command(single_command_t *simp) {
  simp->arguments = NULL;
  simp->num_args = 0;
} /* create_single_command() */

/*
 *  Free everything in a single command
 */

void free_single_command(single_command_t *simp) {
  for (int i = 0; i < simp->num_args; i++) {
    free(simp->arguments[i]);
    simp->arguments[i] = NULL;
  }

  free(simp);
} /* free_single_command() */

/*
 *  Insert an argument into the list of arguments in a single command
 */

void insert_argument(single_command_t *simp, char *argument) {
  if (argument == NULL) {
    return;
  }

  simp->num_args++;
  simp->arguments = (char **)realloc(simp->arguments,
                                    simp->num_args * sizeof(char *));
  simp->arguments[simp->num_args - 1] = argument;
} /* insert_argument() */

/*
 *  Print a single command in a pretty format
 */

void print_single_command(single_command_t *simp) {
  for (int i = 0; i < simp->num_args; i++) {
    printf("\"%s\" \t", simp->arguments[i]);
  }

  printf("\n\n");
} /* print_single_command() */
