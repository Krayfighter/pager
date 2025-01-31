
#include "stddef.h"
#include "stdint.h"
#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "stdbool.h"
#include "string.h"
#include "termios.h"
#include "sys/ioctl.h"
#include "errno.h"
#include "pthread.h"

#include "interface.h"

#include "plustypes.h"


#define ANSI_ESCAPE = 0x1b;
const char *ANSI_ERASE_UNTIL_END = "\x1b[0J";
const char *ANSI_ERASE_LINE = "\x1b[2K";
const char *ANSI_ERASE_SCREEN = "\x1b[2J";
const char *ANSI_MOVE_CURSOR_TO_ORIGIN = "\x1b[H";
const char *ANSI_MOVE_CURSOR_DOWN_FMT = "\x1b[%luB";

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
    fprintf(stdout, "%s", ENTER_ALTERNATE_SCREEN);
  }else { fprintf(stderr, "WARN: stdout is not a tty\n"); }
}
void restore_terminal() {
  if (isatty(fileno(stdout))) {
    tcsetattr(fileno(stdout), TCSANOW, &original_terminal_state);
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


define_List(CharString)


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

define_List(Window)


#define suspend_cancelation(block) { \
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL); \
  block; \
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL); \
}

typedef enum {
  RESIDUAL_STREAM,
  RESIDUAL_HEAP,
} ResidualType;

typedef struct {
  void *residual;
  ResidualType type;
} Residual;

uint8_t residuals_buffer[1028]; // this should be more than large enough for now
FillAllocator residuals_alloc = {
  .buffer = residuals_buffer,
  .buffer_size = 1028,
  .next_space = 0
};
_Atomic bool allocator_locked = false;

void free_residuals() {
  lock_allocator: {
    if (allocator_locked) { usleep(100); goto lock_allocator; }
    allocator_locked = true;
  }
  Residual *current_residual = (Residual *)residuals_alloc.buffer;
  while (current_residual->residual != NULL) {
    switch (current_residual->type) {
      case RESIDUAL_STREAM: { fclose(current_residual->residual); }; break;
      case RESIDUAL_HEAP: { free(current_residual->residual); }; break;
    }
    current_residual += 1;
  }
}

void *Window_read_blocking(void *args) {
  Window *self = args;
  FILE *source_stream;
  source_stream = fdopen(self->source_fd, "r");
  try_unlock_alloc: {
    if (allocator_locked) { usleep(10); goto try_unlock_alloc; }
    allocator_locked = true;
    Residual *res = FillAllocator_talloc(&residuals_alloc, Residual, 1);
    *res = (Residual) {
      .type = RESIDUAL_STREAM,
      .residual = source_stream
    };
  }
  allocator_locked = false;

  char read_buffer[512];

  while (1) {
    if (self->next_line_is_ready == false) {
      char *reading_ended = fgets(read_buffer, 512, source_stream);
      suspend_cancelation({
        if (reading_ended == NULL) {
          if (errno != 0) {
            fprintf(stderr, "WARN: encountered a read error before EOF -> %s\n", strerror(errno));
          }
          return NULL;
        }

        size_t string_len = strlen(read_buffer);
        char *new_string = malloc(string_len);
        memcpy(new_string, read_buffer, string_len);
        new_string[string_len-1] = '\0';

        self->next_line = new_string;
        self->next_line_is_ready = true;
      });
    }
    else {
      usleep(10);
    }
  }

}

Window Window_new(int source) {
  Window self = {
    .lines = List_CharString_new(8),
    .window_start = 0,
    .source_fd = source,
  };
  return self;
}

// WARN self must outlive the lifetime of the spawned thread
void Window_spawn_reader(Window *self) {
  pthread_t reader_thread_id;
  pthread_create(&reader_thread_id, NULL, Window_read_blocking, self);
  self->reader_thread = reader_thread_id;
}

void push_buffer_line(char *buffer, size_t start_index, size_t buffer_index, List_CharString *line_buffer) {
  char *line_string = malloc(buffer_index+1);
  memcpy(line_string, buffer + start_index, buffer_index-1);

  line_string[buffer_index] = '\0';
  List_CharString_push(line_buffer, (CharString){ line_string });
}

bool Window_update(Window *self) {

  if (self->next_line_is_ready) {
    List_CharString_push(&self->lines, (CharString){ self->next_line });
    self->next_line_is_ready = false;
    return true;
  }
  return false;
}

void Window_render(
  Window *self,
  uint16_t offset_x, uint16_t offset_y,
  uint16_t width, uint16_t height,
  bool focused
) {
  if (self->window_start >= self->lines.item_count) { return; }

  uint8_t line_number_max_digits = base_10_digits(self->lines.item_count);

  const char *COLOR;

  if (focused) {
    COLOR = "\x1b[44m";
  }else { COLOR = ""; }


  // TODO use snprintf to a buffer to clip the formatting result
  for(
    uint16_t i = self->window_start;
    i < self->lines.item_count && (i - self->window_start) <= height;
    i += 1
  ) {
    move_cursor_to_position(stdout, offset_y + (i - self->window_start), offset_x);
    fprintf(stdout, "%u", i);

    move_cursor_to_col(stdout, line_number_max_digits + 1 + offset_x);
    fprintf(stdout, "%s|\x1b[0m %s", COLOR, self->lines.items[i]);
  }

}

void Window_move_up(Window *self, size_t count) {
  if (self->window_start < count) { self->window_start = 0; }
  else { self->window_start -= count; }
}

void Window_move_down(Window *self, size_t count) {
  if (self->lines.item_count - self->window_start < count) {
    self->window_start = self->lines.item_count;
  } else { self->window_start += count; }
}



WindowControl Window_handle_input(Window *self, size_t tty_rows, bool *needs_redraw) {
  KeyboardCode buffer = (KeyboardCode){ .integer = 0x0 };
  size_t read_size = read(fileno(stdin), buffer.buffer, 4);
  if (read_size != 0) {
    switch(buffer.integer) {
      case WINDOW_MOVE_UP: Window_move_up(self, 1); *needs_redraw = true; break;
      case WINDOW_PAGE_UP: Window_move_up(self, tty_rows); *needs_redraw = true; break;
      case WINDOW_MOVE_DOWN: Window_move_down(self, 1); *needs_redraw = true; break;
      case WINDOW_PAGE_DOWN: Window_move_down(self, tty_rows); *needs_redraw = true; break;
      case WINDOW_QUIT: return WINDOW_QUIT;
      case WINDOW_SWITCH_NEXT: *needs_redraw = true; return WINDOW_SWITCH_NEXT;
      case WINDOW_SWITCH_PREV: *needs_redraw = true; return WINDOW_SWITCH_PREV;
      default: return WINDOW_CONTROL_NONE;
    }
  }
  return WINDOW_CONTROL_NONE;
} 

void Window_free(Window *self) {
  List_foreach(CharString, self->lines, {
    free(*item);
  });
  List_CharString_free(&self->lines);
}

typedef struct winsize TTY_Dims;

InterfaceCommand Screen_read_stdin(Screen *self, bool *needs_redraw) {
  Window *focused_window = &self->windows.items[self->focus];
  TTY_Dims tty_dims;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &tty_dims);
  WindowControl code = Window_handle_input(focused_window, tty_dims.ws_row, needs_redraw);
  if (code == WINDOW_QUIT) { return INTERFACE_RESULT_QUIT; }
  // TODO change window_switching to operate on non-empty windows
  else if (code == WINDOW_SWITCH_NEXT && (self->focus+1) < self->windows.item_count) {
    self->focus += 1;
  }
  else if (code == WINDOW_SWITCH_PREV && self->focus != 0) {
    self->focus -= 1;
  }
  return INTERFACE_RESULT_NONE;
}

typedef struct {
  uint16_t a, b;
} uint16x2;

uint16x2 calculate_window_dims(TTY_Dims dims) {
  uint16_t floor_win_height = (uint16_t)dims.ws_row >> 1;
  if ((uint16_t)dims.ws_row & 0b0000000000000001) {
    // odd number of rows
    return (uint16x2){ .a = floor_win_height + 1, .b = floor_win_height };
  }else {
    return (uint16x2){ .a = floor_win_height, .b = floor_win_height };
  }
}

void Screen_render(Screen *self) {
  TTY_Dims tty_dims;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &tty_dims);

  Window *frame1, *frame2;
  frame1 = frame2 = NULL;
  for (size_t i = 0; i < self->windows.item_count; i += 1) {
    if (self->windows.items[i].lines.item_count == 0) { continue; }
    if (frame1 == NULL) { frame1 = &self->windows.items[i]; }
    else if (frame2 == NULL) { frame2 = &self->windows.items[i]; }
  }

  if (frame1 == NULL) {
    fprintf(stderr, "WARN: invalid condition, no windows registered\n");
    return;
  }
  bool split_mode = frame2 != NULL;

  size_t top_window_rows;
  size_t bottom_window_rows;
  if (split_mode) {
    uint16x2 window_sizes = calculate_window_dims(tty_dims);
    top_window_rows = window_sizes.a;
    bottom_window_rows = window_sizes.b;
  }else { top_window_rows = tty_dims.ws_row; }
  

  if (frame1 != NULL) { Window_update(frame1); }
  if (frame2 != NULL) { Window_update(frame2 ); }



  fprintf(stdout, "%s%s", ANSI_MOVE_CURSOR_TO_ORIGIN, ANSI_ERASE_SCREEN);

  // NOTE that the first row or col is not position 0 but position 1 (like how lua indexes arrays)

  // left border
  move_cursor_to_position(stdout, 3, 0);
  for (uint16_t i = 1; i < tty_dims.ws_row; i += 1) { fprintf(stdout, "|\n\r"); }

  for (uint16_t i = 2; i < tty_dims.ws_row; i += 1) {
    move_cursor_to_position(stdout, i, tty_dims.ws_col);
    fprintf(stdout, "|");
  }

  fprintf(stdout, "%s", ANSI_MOVE_CURSOR_TO_ORIGIN);
  for (uint16_t i = 0; i < tty_dims.ws_col; i += 1) { fprintf(stdout, "="); }

  move_cursor_to_position(stdout, tty_dims.ws_row, 0);
  for (uint16_t i = 0; i < tty_dims.ws_col; i += 1) { fprintf(stdout, "="); }

  if (!split_mode) { top_window_rows -= 1; } // this is to fix sizing for the bottom border
  Window_render(frame1, 2, 2, tty_dims.ws_col - 2, top_window_rows - 2, self->focus == 0);
  if (split_mode) {
    // render divider
    move_cursor_to_position(stdout, top_window_rows+1, 0);
    for_range(size_t, i, 0, tty_dims.ws_col) { fprintf(stdout, "="); }

    Window_render(frame2, 2, top_window_rows + 2, tty_dims.ws_col - 2, bottom_window_rows - 3, self->focus == 1);
  }
  fflush(stdout);

}

