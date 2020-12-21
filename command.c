#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "command.h"
#include "shell.h"

/*
 *  Initialize a command_t
 */

void create_command(command_t *command) {
  command->single_commands = NULL;

  command->out_file = NULL;
  command->in_file = NULL;
  command->err_file = NULL;

  command->append_out = false;
  command->append_err = false;
  command->background = false;

  command->num_single_commands = 0;
} /* create_command() */

/*
 *  Insert a single command into the list of single commands in a command_t
 */

void insert_single_command(command_t *command, single_command_t *simp) {
  if (simp == NULL) {
    return;
  }

  command->num_single_commands++;
  int new_size = command->num_single_commands * sizeof(single_command_t *);
  command->single_commands = (single_command_t **)
                              realloc(command->single_commands,
                                      new_size);
  command->single_commands[command->num_single_commands - 1] = simp;
} /* insert_single_command() */

/*
 *  Free a command and its contents
 */

void free_command(command_t *command) {
  for (int i = 0; i < command->num_single_commands; i++) {
    free_single_command(command->single_commands[i]);
  }

  if (command->out_file) {
    free(command->out_file);
    command->out_file = NULL;
  }

  if (command->in_file) {
    free(command->in_file);
    command->in_file = NULL;
  }

  if (command->err_file) {
    free(command->err_file);
    command->err_file = NULL;
  }

  command->append_out = false;
  command->append_err = false;
  command->background = false;

  free(command);
} /* free_command() */

/*
 *  Print the contents of the command in a pretty way
 */

void print_command(command_t *command) {
  printf("\n\n");
  printf("              COMMAND TABLE                \n");
  printf("\n");
  printf("  #   single Commands\n");
  printf("  --- ----------------------------------------------------------\n");

  /* iterate over the single commands and print them nicely */

  for (int i = 0; i < command->num_single_commands; i++) {
    printf("  %-3d ", i );
    print_single_command(command->single_commands[i]);
  }

  printf( "\n\n" );
  printf( "  Output       Input        Error        Background\n" );
  printf( "  ------------ ------------ ------------ ------------\n" );
  printf( "  %-12s %-12s %-12s %-12s\n",
            command->out_file?command->out_file:"default",
            command->in_file?command->in_file:"default",
            command->err_file?command->err_file:"default",
            command->background?"YES":"NO");
  printf( "\n\n" );
} /* print_command() */

/*
 *  Execute a command
 */

void execute_command(command_t *command) {
  /* Don't do anything if there are no single commands */

  if (command->single_commands == NULL) {
    print_prompt();
    return;
  }

  /* save the default in, out, and err fd so that we can restore */
  /* them at the end                                             */

  int default_in = dup(0);
  int default_out = dup(1);
  int default_err = dup(2);

  /* set the initial input */

  int fdinput;
  if (command->in_file != NULL) {
    /* if there is an input file for the entire command, */
    /* then set the initial input to this file           */

    fdinput = open(command->in_file, O_RDONLY, 0664);
  }
  else {
    /* there is no input file */
    /* set the initial input to the default */

    fdinput = dup(default_in);
  }

  /* set the error output */

  int fderror;
  if (command->err_file != NULL) {
    if (command->append_err) {
      fderror = open(command->err_file, O_CREAT|O_WRONLY|O_APPEND, 0664);
    }
    else {
      fderror = open(command->err_file, O_CREAT|O_WRONLY, 0664);
    }
  }
  else {
    fderror = dup(default_err);
  }
  dup2(fderror, 2);
  close(fderror);

  int ret;
  int fdoutput;
  int i;
  for (i = 0; i < command->num_single_commands; i++) {
    /* redirect input */

    dup2(fdinput, 0);
    close(fdinput);

    /* setup output */

    if (i == command->num_single_commands - 1) { /* this is the last command */
      if (command->out_file != NULL) {
        if (command->append_out) {
          fdoutput = open(command->out_file, O_CREAT|O_WRONLY|O_APPEND, 0664);
        }
        else {
          fdoutput = open(command->out_file, O_CREAT|O_WRONLY, 0664);
        }
      }
      else {
        /* use default output */
        fdoutput = dup(default_out);
      }
    }
    else { /* this is not the last command */
      /* create a pipe */

      int fdpipe[2];
      pipe(fdpipe);
      fdoutput = fdpipe[1];
      fdinput = fdpipe[0];
    }

    /* redirect output */
    dup2(fdoutput, 1);
    close(fdoutput);

    if (!strcmp((command->single_commands[i]->arguments[0]), ("setenv"))) {
      /* if the command is "setenv", don't fork a child process */

      if (setenv(command->single_commands[i]->arguments[1],
                 command->single_commands[i]->arguments[2], 1)) {
        /* if the "setenv" function returns 1, there was an error */

        perror("setenv");
      }
    }
    else if (!strcmp(command->single_commands[i]->arguments[0], "unsetenv")) {
      /* if the command is "unsetenv", don't fork a child process */

      if (unsetenv(command->single_commands[i]->arguments[1])) {
        /* if the "unsetenv" function returns 1, there was an error */

        perror("unsetenv");
      }
    }
    else if (!strcmp(command->single_commands[i]->arguments[0], "cd")) {
      /* if the command is "cd", don't for a child process */

      if (command->single_commands[i]->arguments[1] == NULL) {
        /* if the first argument for cd is NULL, then navigate to $HOME */

        /* figure out what the home directory is */

        char *home_path = NULL;
        char **envvar = __environ;
        while (*envvar != NULL) {
          if (((*envvar)[0] == 'H') &&
              ((*envvar)[1] == 'O') &&
              ((*envvar)[2] == 'M') &&
              ((*envvar)[3] == 'E')) {

            /* if the current environment variable is HOME, set home */

            home_path = &((*envvar)[5]);
            break;
          }
          envvar++;
        }

        if (chdir(home_path)) {
          /* if the "chdir" function returns 1, there was an error */

          perror("cd");
        }
      }
      else {
        if (chdir(command->single_commands[i]->arguments[1])) {
          /* if the "chdir" function returns 1, there was an error */

          char error_message[512] = "cd: can't cd to ";
          strcat(error_message, command->single_commands[i]->arguments[1]);
          if(write(2, error_message, strlen(error_message)) == -1) {
            perror("write");
          }
        }
      }
    }
    else {
      /* create child process */

      ret = fork();
      if (ret == 0) {
        /* this is the child process */

        if (!strcmp(command->single_commands[i]->arguments[0], "printenv")) {
          char **envvar = __environ;
          while (*envvar != NULL) {
            printf("%s\n", *envvar++);
          }
          exit(0);
        }
        else {
          command->single_commands[i]->
                   arguments[command->single_commands[i]->num_args] = NULL;

          /* handle expansion of '_' */
          /*
          if (g_last_argument_of_last_command == NULL) {
            g_last_argument_of_last_command = malloc(sizeof(char) * 256);
          }
          g_last_argument_of_last_command = strdup(command->single_commands[i]->
            arguments[command->single_commands[i]->num_args - 1]);
          */

          /* execute the command */

          execvp(command->single_commands[i]->arguments[0],
                 command->single_commands[i]->arguments);
          perror("execvp");
          exit(1);
        }
      }
    }
  }

  /* restore the default in/out */

  dup2(default_in, 0);
  dup2(default_out, 1);
  dup2(default_err, 2);
  close(default_in);
  close(default_out);
  close(default_err);

  /* used to determine if we should print a prompt */
  /* (used in the case of ctrl_c)                  */

  bool should_print_prompt = true;

  /* if this isn't a background process, we need to wait for */
  /* the processes to finish                                 */

  if (!command->background) {
    int status_value = 0;
    if(ret == waitpid(ret, &status_value, 0)) {
      if (WIFEXITED(status_value)) {
        g_last_command_exit_code = WEXITSTATUS(status_value);
      }
      else {
        should_print_prompt = false;
      }
    }
  }
  else {
    g_last_background_pid = ret;
  }

  /* if we should print the prompt, print one */

  if (should_print_prompt) {
    print_prompt();
  }

} /* execute_command() */
