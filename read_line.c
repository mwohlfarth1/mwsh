#include "read_line.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "tty_raw_mode.h"

//extern void tty_raw_mode(void);

int g_line_length = 0;
char g_line_buffer[MAX_BUFFER_LINE];

// Simple history array
// This history does not change.
// Yours have to be updated.
int g_history_index = 0;
char * g_history[] = {
  "ls -al | grep x",
  "ps -e",
  "cat read-line-example.c",
  "vi hello.c",
  "make",
  "ls -al | grep xxx | grep yyy"
};
int g_history_length = sizeof(g_history) / sizeof(char *);

/*
 *  Prints usage for read_line
 */

void read_line_print_usage() {
  char * usage = "\n"
    " ctrl-?       Print usage\n"
    " Backspace    Deletes last character\n"
    " up arrow     See last command in the history\n";

  write(1, usage, strlen(usage));
} /* read_line_print_usage() */

/*
 * Input a line with some basic editing.
 */

char *read_line() {

  // Set terminal in raw mode
  tty_raw_mode();

  g_line_length = 0;

  // Read one line until enter is typed
  while (1) {

    // Read one character in raw mode.
    char ch = '\0';
    read(0, &ch, 1);

    if (ch >= 32) {
      // It is a printable character.

      // Do echo
      write(1, &ch, 1);

      // If max number of character reached return.
      if (g_line_length == (MAX_BUFFER_LINE - 2)) {
        break;
      }

      // add char to buffer.
      g_line_buffer[g_line_length] = ch;
      g_line_length++;
    }
    else if (ch == 10) {
      // <Enter> was typed. Return line
      // Print newline
      write(1, &ch, 1);

      break;
    }
    else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      g_line_buffer[0] = 0;
      break;
    }
    else if (ch == 8) {
      // <backspace> was typed. Remove previous character read.

      // Go back one character
      ch = 8;
      write(1, &ch, 1);

      // Write a space to erase the last character read
      ch = ' ';
      write(1, &ch, 1);

      // Go back one character
      ch = 8;
      write(1, &ch, 1);

      // Remove one character from buffer
      g_line_length--;
    }
    else if (ch == 27) {
      // Escape sequence. Read two chars more
      //
      // HINT: Use the program "keyboard-example" to
      // see the ascii code for the different chars typed.
      //
      char ch1 = '\0';
      char ch2 = '\0';
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if ((ch1 == 91) && (ch2 == 65)) {
        // Up arrow. Print next line in history.

        // Erase old line
        // Print backspaces
        int i = 0;
        for (i = 0; i < g_line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        // Print spaces on top
        for (i = 0; i < g_line_length; i++) {
          ch = ' ';
          write(1, &ch, 1);
        }

        // Print backspaces
        for (i = 0; i < g_line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        // Copy line from history
        strcpy(g_line_buffer, g_history[g_history_index]);
        g_line_length = strlen(g_line_buffer);
        g_history_index = (g_history_index + 1) % (g_history_length);

        // echo line
        write(1, g_line_buffer, g_line_length);
      }
    }
  }

  // Add eol and null char at the end of string
  g_line_buffer[g_line_length] = 10;
  g_line_length++;
  g_line_buffer[g_line_length] = 0;

  return g_line_buffer;
} /* read_line() */

