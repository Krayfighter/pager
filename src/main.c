

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



typedef struct {
  Vec_CharString lines;
  size_t window_start;
} Window;

void Window_render(Window *self, FILE *output_stream) {
  if (self->window_start >= self->lines.buffer_len) { return; }
  fprintf(output_stream, "\x1b[H"); // move cursor to 0,0
  struct winsize terminal_window_size;
  ioctl(fileno(stdout), TIOCGWINSZ, &terminal_window_size);

  // Still a little buggy for very large line counts, but functional
    fprintf(output_stream, "%lu: %s%s", self->window_start, ANSI_ERASE_UNTIL_END, self->lines.item_buffer[self->window_start].internal);
  for_range(size_t, i, self->window_start + 1, self->window_start + terminal_window_size.ws_row) {
    if (i == self->lines.buffer_len) { break; }
    fprintf(output_stream, "\n\r%lu: %s%s", i, ANSI_ERASE_UNTIL_END, self->lines.item_buffer[i].internal);
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


int main(int32_t argc, char **argv) {
  int return_value = 0;

  // save terminal and restore it after main exits
  save_terminal();
  atexit(restore_terminal);

  FILE *input_stream = NULL;
  // system("test");
  if (argc == 0) { input_stream = stdin; }
  else {
    for (size_t i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "--spawn")) {
        if (argc == i) {
          fprintf(stderr, "Error: --spawn requires a shell command to execute");
          return -1;
        }
        i++;
        if (argc >= i + 2) {
          fprintf(stderr, "TODO: argument concatenation is not yet implemented");
          return -1;
        }
        char *command_string = argv[i];
        input_stream = popen(command_string, "r");
        break;
      }else {
        if (i+1 > argc) {
          fprintf(stderr, "Error: too many args after file name\n");
          return -1;
        }
        input_stream = fopen(argv[i], "r");
        if (!input_stream) {
          fprintf(stderr, "Error: failed to open file \"%s\", %s\n", argv[i], strerror(errno));
          return -1;
        }
      }
    }
  }
  if (!input_stream) {
    fprintf(stderr, "Error: arg parser did not set input\n");
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
  char chr;
  // Mainloop
  while (1) {
    if (read(fileno(stdin), &chr, 1)) {
      if (chr == 'k' && view_window.window_start > 0) {
        view_window.window_start -= 1;
      } else if (
        chr == 'j' && view_window.window_start + 1 < view_window.lines.buffer_len
      ) {
        view_window.window_start += 1;
      }else if (chr == 'q') {
        break;
      }
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


