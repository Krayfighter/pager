

#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "errno.h"
#include "string.h"
#include "termios.h"
#include "unistd.h"
#include "sys/ioctl.h"
// #include "math.h"

#include "typedef_macros.h"


#define for_range(ItemT, ItemName, start, end) \
for (ItemT ItemName = start; ItemName < end; ItemName += 1)

typedef struct { char *internal; } CharString;
void CharString_free(CharString *self) {
  free(self->internal);
}
define_heap_array(CharString);


#define ANSI_ESCAPE = 0x1b;
const char *ANSI_ERASE_UNTIL_END = "\x1b[0J";

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

void Window_render(Window *self, FILE *output_stream) {
  if (self->window_start >= self->lines.buffer_len) { return; }
  fprintf(output_stream, "\x1b[H"); // move cursor to 0,0
  struct winsize terminal_window_size;
  ioctl(fileno(stdout), TIOCGWINSZ, &terminal_window_size);
  size_t line_number_max_digits = base_10_digits(self->lines.buffer_len);

  for_range(size_t, i, self->window_start, self->window_start + terminal_window_size.ws_row) {
    if (i == self->lines.buffer_len) { break; }
    if (i == self->window_start) { // do not push first line down
      fprintf(output_stream, "%lu", i);
    }else { fprintf(output_stream, "\n\r%lu", i); }

    move_cursor_to_col(output_stream, line_number_max_digits + 1);
    fprintf(output_stream, ": %s%s", ANSI_ERASE_UNTIL_END, self->lines.item_buffer[i].internal);
  }

  fflush(output_stream);
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
  int return_value = 0;

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

  FILE *input_stream = NULL;
  for_range(size_t, index, 0, tokens.buffer_len) {
    if (tokens.item_buffer[index].type == TOKEN_HELP) {
      input_stream = fopen("help.txt", "r");
      if (input_stream == NULL) {
        fprintf(stderr, "Error: Failed to open help.txt -> %s\n", strerror(errno));
        return -1;
      }
      break;
    }
  }
  if (!input_stream) {
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
      input_stream = popen(command_str, "r");
      if (!input_stream) {
        fprintf(stderr, "Error: failed to spawn subprocess -> %s\n", strerror(errno));
        return -1;
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
      input_stream = fopen(filename, "r");
      if (!input_stream) {
        fprintf(stderr, "Error: Failed to open file -> %s\n", strerror(errno));
        return -1;
      }
    }
    // }
  }
  Vec_Token_free(&tokens);

  if (!input_stream) {
    fprintf(stderr, "!LogicError!: input_stream should never be NULL after argument parser\n");
    return -1;
  }

  Vec_CharString string_vec = Vec_CharString_new(8);

  char *line_buffer = malloc(1000);
  while (fgets(line_buffer, 1000, input_stream)) {
    size_t buffer_len = strlen(line_buffer);
    char *new_buffer = malloc(buffer_len);
    memcpy(new_buffer, line_buffer, buffer_len);
    new_buffer[buffer_len-1] = '\0';

    Vec_CharString_push(&string_vec, (CharString){new_buffer} );
  }
  free(line_buffer);
  fclose(input_stream);

  Window view_window;
  view_window.lines = string_vec;
  view_window.window_start = 0;

  tc_enable_terminal_raw_mode();
  Window_render(&view_window, stdout);
  // char chr;

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
      if (input_buffer.buffer[0] == 'k' && view_window.window_start > 0) {
        view_window.window_start -= 1;
      } else if (
        input_buffer.buffer[0] == 'j' && view_window.window_start + 1 < view_window.lines.buffer_len
      ) {
        view_window.window_start += 1;
      }else if (input_buffer.buffer[0] == 'q') {
        break;
      // NOTE notice that the 0x1b byte is at the end. this byte is the ESCAPE
      // code in ASCII which actually comes first because the bytes in input_buffer
      // are stored in reverse order since they are on the stack
      }else if (input_buffer.integer == 0x7e365b1b) {
        struct winsize tty_dims;
        ioctl(fileno(stdout), TIOCGWINSZ, &tty_dims);
        view_window.window_start += tty_dims.ws_row;
        if (view_window.window_start >= view_window.lines.buffer_len) {
          view_window.window_start = view_window.lines.buffer_len - 1;
        }
      }else if (input_buffer.integer == 0x7e355b1b) {
        struct winsize tty_dims;
        ioctl(fileno(stdout), TIOCGWINSZ, &tty_dims);
        if (view_window.window_start > tty_dims.ws_row) {
          view_window.window_start -= tty_dims.ws_row;
        }else { view_window.window_start = 0; }
      }
      // fprintf(stderr, "DBG: char hex %lx\n", input_buffer.integer);
      input_buffer.integer = 0x0;
    }

    Window_render(&view_window, stdout);

    const uint32_t MILLISECOND = 1000;
    const uint32_t SECOND = 1000 * MILLISECOND;
    const uint32_t FRAME_TIME = 60 / SECOND;
    usleep(FRAME_TIME);
  }

  // free strings
  Vec_CharString_foreach(&string_vec, CharString_free);
  Vec_CharString_free(&string_vec);

}


