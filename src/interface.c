
#include "stddef.h"
#include "stdint.h"
#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "stdbool.h"
#include "string.h"
#include "termios.h"
#include "sys/ioctl.h"

#include "typedef_macros.h"
#include "interface.h"


#define ANSI_ESCAPE = 0x1b;
const char *ANSI_ERASE_UNTIL_END = "\x1b[0J";
const char *ANSI_ERASE_LINE = "\x1b[2K";
const char *ANSI_ERASE_SCREEN = "\x1b[2J";
const char *ANSI_MOVE_CURSOR_TO_ORIGIN = "\x1b[H";

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

void move_cursor_to_position(FILE *stream, size_t row, size_t col) {
  fprintf(stream, "\x1b[%zu;%zuf", row, col);
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
void enter_raw_mode() {
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


void CharString_free(CharString *self) {
  free(self->internal);
}
define_heap_array(CharString);



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

define_heap_array(Window);

Window Window_new(int source) {
  return (Window) {
    .lines = Vec_CharString_new(8),
    .window_start = 0,
    .source_fd = source
  };
}

void Window_update(Window *self) {
  static char buffer[1024];
  static size_t buffer_index = 0;
  char nanobuffer;

  // int source_fileno = fileno(self->source_fd);
  while(read(self->source_fd, &nanobuffer, 1) > 0) {
    if (nanobuffer == '\n') {
      size_t buffer_len = buffer_index + 1;
      char *string_buf = malloc(buffer_len);
      memcpy(string_buf, buffer, buffer_len);
      string_buf[buffer_len-1] = '\0';
      Vec_CharString_push(&self->lines, (CharString){ .internal = string_buf });

      buffer_index = 0;
      continue;
    }
    buffer[buffer_index] = nanobuffer;
    buffer_index += 1;
  }

  // while (fgets(buffer, 1024, self->source)) {
  //   size_t buffer_len = strlen(buffer);
  //   char *new_buffer = malloc(buffer_len);
  //   memcpy(new_buffer, buffer, buffer_len);
  //   new_buffer[buffer_len-1] = '\0';

  //   Vec_CharString_push(&self->lines, (CharString){new_buffer} );
  // }
}

void Window_render(Window *self, size_t rows, size_t cols, bool focused) {
  if (self->window_start >= self->lines.buffer_len) { return; }
  // FILE *output_stream = fdopen(output_fd, "w");
  // fprintf(output_stream, "\x1b[H"); // move cursor to 0,0
  // struct winsize terminal_window_size;
  // ioctl(fileno(stdout), TIOCGWINSZ, &terminal_window_size);
  size_t line_number_max_digits = base_10_digits(self->lines.buffer_len);

  const char *COLOR;

  if (focused) {
    COLOR = "\x1b[44m";
  }else { COLOR = ""; }

  // for_range(size_t, i, self->window_start, self->window_start + terminal_window_size.ws_row) {
  for_range(size_t, i, self->window_start, self->window_start + rows) {
    if (i == self->lines.buffer_len) { break; }
    // fprintf(output_stream, "%s", ANSI_ERASE_UNTIL_END);
    if (i == self->window_start) { // do not push first line down
      fprintf(stdout, "%lu", i);
    }else { fprintf(stdout, "\n\r%lu", i); }

    move_cursor_to_col(stdout, line_number_max_digits + 1);
    fprintf(stdout, "%s|\x1b[0m %s", COLOR, self->lines.item_buffer[i].internal);
  }

  // fflush(output_stream);
}

void Window_move_up(Window *self, size_t count) {
  if (self->window_start < count) { self->window_start = 0; }
  else { self->window_start -= count; }
}

void Window_move_down(Window *self, size_t count) {
  if (self->lines.buffer_len - self->window_start < count) {
    self->window_start = self->lines.buffer_len - 1;
  } else { self->window_start += count; }
}



WindowControl Window_handle_input(Window *self) {
  KeyboardCode buffer = (KeyboardCode){ .integer = 0x0 };
  size_t read_size = read(STDIN_FILENO, buffer.buffer, 4);
  if (read_size != 0) {
    switch(buffer.integer) {
      case WINDOW_MOVE_UP: Window_move_up(self, 1); break;
      case WINDOW_PAGE_UP: Window_move_up(self, self->window_height); break;
      case WINDOW_MOVE_DOWN: Window_move_down(self, 1); break;
      case WINDOW_PAGE_DOWN: Window_move_down(self, self->window_height); break;
      case WINDOW_QUIT: return WINDOW_QUIT;
      case WINDOW_SWITCH_NEXT: return WINDOW_SWITCH_NEXT;
      case WINDOW_SWITCH_PREV: return WINDOW_SWITCH_PREV;
      default: return WINDOW_CONTROL_NONE;
    }
  }
  return WINDOW_CONTROL_NONE;
} 

void Window_free(Window *self) {
  Vec_CharString_foreach(&self->lines, CharString_free);
  Vec_CharString_free(&self->lines);
}

typedef struct winsize TTY_Dims;

InterfaceResult Screen_read_stdin(Screen *self) {
  Window *focused_window;
  if (self->frame1 != NULL && self->focus == 0) {
    focused_window = self->frame1;
  }
  else if (self->frame2!= NULL && self->focus == 1) {
    focused_window = self->frame2;
  }
  else {
    fprintf(stderr, "WARN: Screen focus does not refer to a valid window\n");
    return INTERFACE_RESULT_NONE;
  }
  WindowControl code = Window_handle_input(focused_window);
  if (code == WINDOW_QUIT) { return INTERFACE_RESULT_QUIT; }
  // if (code == WINDOW_SWITCH_NEXT || code == WINDOW_SWITCH_PREV) {
  //   if (self->focus == 0) { self->focus = 1; }
  //   else { self->focus = 0; }
  // }
  return INTERFACE_RESULT_NONE;
}

void Screen_render(Screen *self) {
  TTY_Dims tty_dims;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &tty_dims);

  bool split_mode = self->frame2 != NULL;

  size_t top_window_rows;
  size_t bottom_window_rows;
  if (split_mode) {
    top_window_rows = tty_dims.ws_row / 2;
    bottom_window_rows = tty_dims.ws_row - (top_window_rows + 1);
  }else { top_window_rows = tty_dims.ws_row; }
  
  // write(output_fd, ANSI_MOVE_CURSOR_TO_ORIGIN, strlen(ANSI_MOVE_CURSOR_TO_ORIGIN));
  // write(output_fd, ANSI_ERASE_SCREEN, strlen(ANSI_ERASE_SCREEN));

  if (self->frame1 != NULL) { Window_update(self->frame1); }
  if (self->frame2 != NULL) { Window_update(self->frame2 ); }


  // FILE *output_stream = fdopen(output_fd, "w");

  fprintf(stdout, "%s%s", ANSI_MOVE_CURSOR_TO_ORIGIN, ANSI_ERASE_SCREEN);
  
  Window_render(self->frame1, top_window_rows, tty_dims.ws_col, self->focus == 0);
  if (split_mode) {
    move_cursor_to_position(stdout, top_window_rows+1, 0);
    for_range(size_t, i, 0, tty_dims.ws_col) { fprintf(stdout, "="); }

    move_cursor_to_position(stdout, top_window_rows+2, 0);
    Window_render(self->frame2, bottom_window_rows, tty_dims.ws_col, self->focus == 1);
  }
  // fflush(output_stream);
  // fsync(output_fd);

}

