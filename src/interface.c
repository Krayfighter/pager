
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
define_heap_array(CharString)



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

define_heap_array(Window)


void *Window_read_blocking(void *args) {
  Window *self = args;
  FILE *source_stream = fdopen(self->source_fd, "r");
  char *read_buffer = malloc(512);

  while (1) {
    if (self->next_line_is_ready == false) {
      char *reading_ended = fgets(read_buffer, 512, source_stream);
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

        // size_t buffer_len = strlen(self->nex);
        // char *new_buffer = malloc(buffer_len);
        // memcpy(new_buffer, buffer, buffer_len);
        // new_buffer[buffer_len-1] = '\0';

      // Vec_CharString_push(&self->lines, (CharString){new_buffer} );
      // }
    }
    else {
      usleep(10);
    }
  }

  fclose(source_stream);
  
  return NULL; // IDK why this pthread want the entrypoint to return a void pointer
}

Window Window_new(int source) {
  Window self = {
    .lines = Vec_CharString_new(8),
    .window_start = 0,
    .source_fd = source,
    // .input_buffer = malloc(1024)
  };
  // memset(&self.aio_handle, 0, sizeof(struct aiocb));
  // // memset(&self.input_buffer, 0, 1024);
  // // char *buffer = calloc(1024, 1);
  // self.aio_handle.aio_buf = self.input_buffer;
  // self.aio_handle.aio_fildes = self.source_fd;
  // self.aio_handle.aio_nbytes = 1024;

  // aio_read(&self.aio_handle);
  return self;
}

// WARN self must outlive the lifetime of the spawned thread
void Window_spawn_reader(Window *self) {
  pthread_t reader_thread_id;
  pthread_create(&reader_thread_id, NULL, Window_read_blocking, self);
  self->reader_thread = reader_thread_id;
}

void push_buffer_line(char *buffer, size_t start_index, size_t buffer_index, Vec_CharString *line_buffer) {
  char *line_string = malloc(buffer_index+1);
  memcpy(line_string, buffer + start_index, buffer_index-1);

  line_string[buffer_index] = '\0';
  Vec_CharString_push(line_buffer, (CharString){ line_string });
}

bool Window_update(Window *self) {

  if (self->next_line_is_ready) {
    Vec_CharString_push(&self->lines, (CharString){ self->next_line });
    self->next_line_is_ready = false;
    return true;
  }
  return false;
  // // static bool is_first_call = true;
  // // if (is_first_call) {
  // //   aio_read(&self->aio_handle);
  // //   is_first_call = false;
  // // }

  // // the input buffer is NULL is the source_fd reached EOF
  // if (self->input_buffer == NULL) { return; }
  
  // int errcode = aio_error(&self->aio_handle);

  // if (errcode == 0) {
  //   ssize_t bytes_read = aio_return(&self->aio_handle);
  //   if (errno != 0) {
  //     fprintf(stderr, "WARN: error updating window -> %s", strerror(errno));
  //   }
  //   if (bytes_read <= 0) { // reached EOF
  //     if (bytes_read < 0) {
  //       fprintf(stderr, "WARN: aio_read return value indicates an error -> %s\n", strerror(errno));
  //     }
  //     free(self->input_buffer);
  //     self->input_buffer = NULL;
  //     return;
  //   }
  //   // ssize_t return_code = aio_return(&self->aio_handle);
  //   // if (errno != 0) {
  //   //   fprintf(stderr, "WARN: error updating window -> %s\n", strerror(errno));
  //   // }
  //   // if (return_code < 0) {
  //   //   fprintf(stderr, "WARN: nonzero asynchronous read return value -> %li", return_code);
  //   // }
  //   // // char *buffer_pointer = (char *)self->aio_handle.aio_buf;
  //   // size_t buffer_index = 0;
  //   // // size_t current_line_len = 1; // including null byte

  //   // size_t line_start = 0;
  //   // char *io_buffer = (char *)self->aio_handle.aio_buf;

  //   // for (size_t buffer_index = 0; buffer_index < 1024; buffer_index += 1) {
  //   //   if (io_buffer[buffer_index] == '\n') {
  //   //     push_buffer_line(io_buffer, line_start, buffer_index, &self->lines);

  //   //     line_start = buffer_index + 1;
  //   //     // current_line_len = 0;
  //   //   }
  //   //   // buffer_index += 1;
  //   //   // current_line_len += 1;
  //   // }
  //   // if (line_start < 1024) {
  //   //   push_buffer_line((char *) self->aio_handle.aio_buf, line_start, 1024 - line_start, &self->lines);
  //   // }

  //   // // self->aio_handle.aio_offset += 1024;
  //   // // aio_read(&self->aio_handle);
  // }
  // else if (errcode == EINPROGRESS) { return; }
  // else {
  //   fprintf(stderr, "WARN: error while waiting for IO -> %s\n", strerror(errcode));
  //   return;
  // }

  // // int source_fileno = fileno(self->source_fd);
  // while(read(self->source_fd, &nanobuffer, 1) > 0) {
  //   if (nanobuffer == '\n') {
  //     size_t buffer_len = buffer_index + 1;
  //     char *string_buf = malloc(buffer_len);
  //     memcpy(string_buf, buffer, buffer_len);
  //     string_buf[buffer_len-1] = '\0';
  //     Vec_CharString_push(&self->lines, (CharString){ .internal = string_buf });

  //     buffer_index = 0;
  //     continue;
  //   }
  //   buffer[buffer_index] = nanobuffer;
  //   buffer_index += 1;
  // }

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
    self->window_start = self->lines.buffer_len;
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

// TODO 
void Window_free(Window *self) {
  Vec_CharString_foreach(&self->lines, CharString_free);
  Vec_CharString_free(&self->lines);
}

typedef struct winsize TTY_Dims;

InterfaceCommand Screen_read_stdin(Screen *self, bool *needs_redraw) {
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
  TTY_Dims tty_dims;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &tty_dims);
  WindowControl code = Window_handle_input(focused_window, tty_dims.ws_row, needs_redraw);
  if (code == WINDOW_QUIT) { return INTERFACE_RESULT_QUIT; }
  if (code == WINDOW_SWITCH_NEXT || code == WINDOW_SWITCH_PREV) {
    if (self->focus == 0 && self->frame2 != NULL) { self->focus = 1; }
    else { self->focus = 0; }
  }
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
  fflush(stdout);
  // fflush(output_stream);
  // fsync(output_fd);

}

