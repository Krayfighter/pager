

#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "errno.h"
#include "string.h"
#include "termios.h"
#include "unistd.h"
#include "sys/ioctl.h"
#include "sys/socket.h"
#include "sys/wait.h"
// #include "math.h"

#include "typedef_macros.h"
#include <asm-generic/ioctls.h>


#define for_range(ItemT, ItemName, start, end) \
for (ItemT ItemName = start; ItemName < end; ItemName += 1)

typedef struct { char *internal; } CharString;
void CharString_free(CharString *self) {
  free(self->internal);
}
define_heap_array(CharString);


#define ANSI_ESCAPE = 0x1b;
const char *ANSI_ERASE_UNTIL_END = "\x1b[0J";
const char *ANSI_ERASE_LINE = "\x1b[2K";

// NOTE these are NOT ANSI, but are defined by the xterm specification
// see https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Functions-using-CSI-_-ordered-by-the-final-character_s_
const char *MAKE_CURSOR_INVISIBLE_SEQUENCE = "\x1b[?25l";
const char *MAKE_CURSOR_VISIBLE_SEQUENCE = "\x1b[?25h";
// see https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-The-Alternate-Screen-Buffer
const char *ENTER_ALTERNATE_SCREEN = "\x1b[?1049h";
const char *LEAVE_ALTERNATE_SCREEN = "\x1b[?1049l";

void move_cursor_to_col(FILE *stream, size_t col_num) {
  fprintf(stream, "\x1b[%luG", col_num);
}


size_t base_10_digits(size_t number) {
  if (number < 9) { return 1; }
  else if (number < 99) { return 2; }
  else if (number < 999) { return 3; }
  else if (number < 9999) { return 4; }
  else if (number < 99999) { return 5; }
  else if (number < 999999) { return 6; }
  else if (number < 9999999) { return 7; }
  else if (number < 99999999) { return 8; }
  else if (number < 999999999) { return 9; }
  else {
    fprintf(stderr, "WARN: the number of lines is extremely high, and line numbers may not be display correctly\n");
    return 10;
  }
}
typedef struct {
  Vec_CharString lines;
  size_t window_start;
} Window;

void Window_render(Window *self, FILE *output_stream, size_t rows, size_t cols) {
  if (self->window_start >= self->lines.buffer_len) { return; }
  // fprintf(output_stream, "\x1b[H"); // move cursor to 0,0
  // struct winsize terminal_window_size;
  // ioctl(fileno(stdout), TIOCGWINSZ, &terminal_window_size);
  size_t line_number_max_digits = base_10_digits(self->lines.buffer_len);

  // for_range(size_t, i, self->window_start, self->window_start + terminal_window_size.ws_row) {
  for_range(size_t, i, self->window_start, self->window_start + rows) {
    if (i == self->lines.buffer_len) { break; }
    fprintf(output_stream, "%s", ANSI_ERASE_UNTIL_END);
    if (i == self->window_start) { // do not push first line down
      fprintf(output_stream, "%lu", i);
    }else { fprintf(output_stream, "\n\r%lu", i); }

    move_cursor_to_col(output_stream, line_number_max_digits + 1);
    fprintf(output_stream, "| %s", self->lines.item_buffer[i].internal);
  }

  fflush(output_stream);
}

// define_option(Window, uint64_t, 0x0);
typedef struct winsize TTY_Dims;

// a conecetpual screen that consumes the entire tty with
// one or more Windows dividing it
typedef struct {
  Window *top_window;
  Window *bottom_window;
} Screen;

void Screen_render(Screen *self, FILE *output_stream) {
  TTY_Dims tty_dims;
  ioctl(fileno(output_stream), TIOCGWINSZ, &tty_dims);

  bool split_mode = self->bottom_window != NULL;

  size_t top_window_rows;
  size_t bottom_window_rows;
  if (split_mode) {
    top_window_rows = tty_dims.ws_row / 2;
    bottom_window_rows = tty_dims.ws_row - top_window_rows;
  }else { top_window_rows = tty_dims.ws_row; }

  fprintf(output_stream, "\x1b[H"); // move cursor to 0,0
  Window_render(self->top_window, output_stream, top_window_rows, tty_dims.ws_col);
  if (split_mode) {
    Window_render(self->bottom_window, output_stream, bottom_window_rows, tty_dims.ws_col);
  }
}


struct termios original_terminal_state;
void save_terminal() {
  if (isatty(fileno(stdout))) {
    tcgetattr(fileno(stdout), &original_terminal_state);
    // fprintf(stdout, "%s", SAVE_SCREEN);
    fprintf(stdout, "%s", ENTER_ALTERNATE_SCREEN);
  }else { fprintf(stderr, "WARN: stdout is not a tty\n"); }
}
void restore_terminal() {
  if (isatty(fileno(stdout))) {
    tcsetattr(fileno(stdout), TCSANOW, &original_terminal_state);
    // fprintf(stdout, "%s%s", RESTORE_SCREEN, MAKE_CURSOR_VISIBLE_SEQUENCE);
    fprintf(stdout, "%s%s", LEAVE_ALTERNATE_SCREEN, MAKE_CURSOR_VISIBLE_SEQUENCE);
  }else { fprintf(stderr, "WARN: stdout is not a tty\n"); }
}
void tc_enable_terminal_raw_mode() {
  if (isatty(fileno(stdout))) {
    struct termios cfg;
    tcgetattr(fileno(stdout), &cfg);
    cfg.c_lflag &= ~(ICANON|ECHO);
    cfg.c_cc[VTIME] = 0;
    cfg.c_cc[VMIN] = 1;
    if (tcsetattr(fileno(stdout), TCSAFLUSH, &cfg)) {
      fprintf(stderr, "WARN: unable to set raw mode for terminal");
    }
    fprintf(stdout, "%s", MAKE_CURSOR_INVISIBLE_SEQUENCE);
  }else { fprintf(stderr, "WARN: stdout is not a tty"); }
}


enum TokenType {
  TOKEN_HELP,
  TOKEN_SPAWN,
  TOKEN_STRING,
};

typedef struct {
  enum TokenType type;
  void *option_content;
} Token;



define_heap_array(Token);
define_option(Vec_Token, uint64_t, 0x0);

// NOTE this function does NOT take ownership of the array passed
// as it assumes that it is handled by the OS (as in argv passed to main)
Option_Vec_Token parse_command_line_args(char **args, size_t arg_count) {
  Vec_Token tokens = Vec_Token_new(arg_count);

  // skip the first arg which is always the executable's filename
  for_range(size_t, arg_index, 1, arg_count) {
    size_t arg_length = strlen(args[arg_index]);
    if (arg_length == 0) { continue; }
    else if (args[arg_index][0] == '-') {
      if (!strcmp(args[arg_index], "--spawn")) {
        Vec_Token_push(&tokens, (Token){ .type = TOKEN_SPAWN, .option_content = NULL });
        continue;
      }
      else if (!strcmp(args[arg_index], "--help")) {
        Vec_Token_push(&tokens, (Token) { .type = TOKEN_HELP, .option_content = NULL });
        continue;
      }
      else {
        fprintf(stderr, "Error: unrecognized option %s\n", args[arg_index]);
        Vec_Token_free(&tokens);
        return (Option_Vec_Token){ .none = 0x0 };
      }
    }
    else {
      Vec_Token_push(&tokens, (Token){ .type = TOKEN_STRING, .option_content = args[arg_index] });
      continue;
    }
  }

  return (Option_Vec_Token){ .some = tokens };
}




int main(int32_t argc, char **argv) {

  // save terminal and restore it after main exits
  save_terminal();
  atexit(restore_terminal);

  Option_Vec_Token maybe_tokens = parse_command_line_args(argv, argc);
  if (!Option_Vec_Token_is_some(&maybe_tokens)) {
    fprintf(stderr, "Error: Failed to parse command line args (see previous)\n");
    return -1;
  }
  Vec_Token tokens = maybe_tokens.some;
  if (tokens.buffer_len == 0) {
    fprintf(stderr, "Error: missing required argument\n");
    return -1;
  }

  FILE *main_input_stream = NULL;
  FILE *secondary_input_stream = NULL;
  for_range(size_t, index, 0, tokens.buffer_len) {
    if (tokens.item_buffer[index].type == TOKEN_HELP) {
      main_input_stream = fopen("help.txt", "r");
      if (main_input_stream == NULL) {
        fprintf(stderr, "Error: Failed to open help.txt -> %s\n", strerror(errno));
        return -1;
      }
      break;
    }
  }
  if (!main_input_stream) {
    // for_range(size_t, token_index, 0, tokens.buffer_len) {
    if (tokens.item_buffer[0].type == TOKEN_SPAWN) {
      if (tokens.buffer_len < 2) {
        fprintf(stderr, "Error: expected a command after --spawn\n");
        return -1;
      }else if (tokens.buffer_len > 2) {
        fprintf(stderr, "Error: too many arguments after --spawn\n");
        return -1;
      }else if (tokens.item_buffer[1].type != TOKEN_STRING) {
        fprintf(stderr, "Error: expected command string, got unexpected argument type\n");
        return -1;
      }
      char *command_str = tokens.item_buffer[1].option_content;

      int subproc_stdin_pair[2];
      int subproc_stdout_pair[2];
      int subproc_stderr_pair[2];

      if (socketpair(PF_LOCAL, SOCK_STREAM, 0, subproc_stdin_pair) < 0) {
        fprintf(stderr, "Error: failed to create local socket -> %s\n", strerror(errno));
        return -1;
      }
      if (socketpair(PF_LOCAL, SOCK_STREAM, 0, subproc_stdout_pair) < 0) {
        fprintf(stderr, "Error: failed to create local socket -> %s\n", strerror(errno));
        return -1;
      }
      if (socketpair(PF_LOCAL, SOCK_STREAM, 0, subproc_stderr_pair) < 0) {
        fprintf(stderr, "Error: failed to create local socket -> %s\n", strerror(errno));
        return -1;
      }

      pid_t process_id;
      process_id = fork();
      if (process_id < 0) {
        fprintf(stderr, "Error, failed to fork process -> %s\n", strerror(errno));
        return -1;
      }else if (process_id == 0) {
        // replace stdin stdout and stderr file descriptors with the sockets
        close(subproc_stdin_pair[0]);
        close(subproc_stdout_pair[0]);
        close(subproc_stderr_pair[0]);

        dup2(subproc_stdin_pair[1], STDIN_FILENO);
        dup2(subproc_stdout_pair[1], STDOUT_FILENO);
        dup2(subproc_stderr_pair[1], STDERR_FILENO);

        stdin = fdopen(subproc_stdin_pair[1], "r");
        stdout = fdopen(subproc_stdout_pair[1], "w");
        stderr = fdopen(subproc_stderr_pair[1], "w");

        execl("/bin/sh", "/bin/sh", "-c", command_str, NULL);
        exit(0);
      }
      // the sockets don't work if you don't close the other end
      close(subproc_stdin_pair[1]);
      close(subproc_stdout_pair[1]);
      close(subproc_stderr_pair[1]);

      fprintf(stderr, "DBG: running in main process\n");
      // FILE *child_stdin = fdopen(subproc_stdin_pair[0], "w");
      FILE *child_stdout = fdopen(subproc_stdout_pair[0], "r");
      // FILE *child_stderr = fdopen(subproc_stderr_pair[0], "r");

      main_input_stream = child_stdout;
      secondary_input_stream = fdopen(subproc_stderr_pair[0], "r");
      if (!main_input_stream) {
        fprintf(stderr, "Error: failed to spawn subprocess -> %s\n", strerror(errno));
        return -1;
      }

      int return_status;
      waitpid(process_id, &return_status, 0x0);
      if (return_status != 0) {
        fprintf(stderr, "WARN: subprocess returned with non-zero return code (%i)\b", return_status);
      }
      
    }
    else if (tokens.item_buffer[0].type == TOKEN_STRING) {
      if (tokens.buffer_len > 1) {
        fprintf(stderr, "Error: unexpected arguments after filename\n");
        return -1;
      }else if (tokens.item_buffer[0].type != TOKEN_STRING) {
        fprintf(stderr, "Error: expected filename string, received unexpected argument type\n");
        return -1;
      }
      char *filename = tokens.item_buffer[0].option_content;
      main_input_stream = fopen(filename, "r");
      if (!main_input_stream) {
        fprintf(stderr, "Error: Failed to open file -> %s\n", strerror(errno));
        return -1;
      }
    }
    // }
  }
  Vec_Token_free(&tokens);

  if (!main_input_stream) {
    fprintf(stderr, "!LogicError!: input_stream should never be NULL after argument parser\n");
    return -1;
  }

  Vec_CharString main_window_strings = Vec_CharString_new(8);
  Vec_CharString second_window_strings = Vec_CharString_UNINIT;

  if (secondary_input_stream != NULL) {
    second_window_strings = Vec_CharString_new(8);
  }

  char *line_buffer = malloc(1000);
  while (fgets(line_buffer, 1000, main_input_stream)) {
    size_t buffer_len = strlen(line_buffer);
    char *new_buffer = malloc(buffer_len);
    memcpy(new_buffer, line_buffer, buffer_len);
    new_buffer[buffer_len-1] = '\0';

    Vec_CharString_push(&main_window_strings, (CharString){new_buffer} );
  }
  if (secondary_input_stream != NULL) {
    while (fgets(line_buffer, 1000, secondary_input_stream)) {
      size_t buffer_len = strlen(line_buffer);
      char *new_buffer = malloc(buffer_len);
      memcpy(new_buffer, line_buffer, buffer_len);
      new_buffer[buffer_len-1] = '\0';

      Vec_CharString_push(&second_window_strings, (CharString){new_buffer} );
    }
    if (second_window_strings.item_buffer[0].internal == NULL) {
      Vec_CharString_push(&second_window_strings, (CharString){ malloc(0) });
    }
  }
  free(line_buffer);
  fclose(main_input_stream);

  fprintf(stderr, "DBG: read lines from subprocess\n");

  Window window;
  window.lines = main_window_strings;
  window.window_start = 0;

  Window second_window;

  Screen screen = (Screen){ .top_window = &window, .bottom_window = NULL };

  if (second_window_strings.item_buffer != NULL && second_window_strings.buffer_len != 1) {
    second_window.lines = second_window_strings;
    second_window.window_start = 0;
    screen.bottom_window = &second_window;
  }

  tc_enable_terminal_raw_mode();
  // Window_render(&view_window, stdout);
  Screen_render(&screen, stdout);

  // NOTE this is stored on the stack, and so the bytes
  // in the buffer are reversed
  typedef union {
    char buffer[4];
    uint64_t integer;
  } IBuffer;

  IBuffer input_buffer;
  // Mainloop
  while (1) {
    size_t read_size = read(fileno(stdin), input_buffer.buffer, 4);
    if (read_size != 0) {
      if (input_buffer.buffer[0] == 'k' && screen.top_window->window_start > 0) {
        // view_window.window_start -= 1;
        screen.top_window->window_start -= 1;
      } else if (
        input_buffer.buffer[0] == 'j' && screen.top_window->window_start + 1 < window.lines.buffer_len
      ) {
        // view_window.window_start += 1;
        screen.top_window->window_start += 1;
      }else if (input_buffer.buffer[0] == 'q') {
        break;
      // NOTE notice that the 0x1b byte is at the end. this byte is the ESCAPE
      // code in ASCII which actually comes first because the bytes in input_buffer
      // are stored in reverse order since they are on the stack
      }else if (input_buffer.integer == 0x7e365b1b) {
        struct winsize tty_dims;
        ioctl(fileno(stdout), TIOCGWINSZ, &tty_dims);
        window.window_start += tty_dims.ws_row;
        if (window.window_start >= window.lines.buffer_len) {
          window.window_start = window.lines.buffer_len - 1;
        }
      }else if (input_buffer.integer == 0x7e355b1b) {
        struct winsize tty_dims;
        ioctl(fileno(stdout), TIOCGWINSZ, &tty_dims);
        if (window.window_start > tty_dims.ws_row) {
          window.window_start -= tty_dims.ws_row;
        }else { window.window_start = 0; }
      }
      // fprintf(stderr, "DBG: char hex %lx\n", input_buffer.integer);
      input_buffer.integer = 0x0;
    }

    // Window_render(&view_window, stdout);
    Screen_render(&screen, stdout);

    const uint32_t MILLISECOND = 1000;
    const uint32_t SECOND = 1000 * MILLISECOND;
    const uint32_t FRAME_TIME = 60 / SECOND;
    usleep(FRAME_TIME);
  }

  // free strings
  Vec_CharString_foreach(&main_window_strings, CharString_free);
  Vec_CharString_free(&main_window_strings);

  if (second_window_strings.item_buffer != NULL) {
    Vec_CharString_foreach(&second_window_strings, CharString_free);
    Vec_CharString_free(&second_window_strings);
  }

}


