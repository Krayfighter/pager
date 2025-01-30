
#include "stdio.h"
#include "stdbool.h"
#include <aio.h>
#include <stdint.h>

#include "plustypes.h"


void move_cursor_to_col(FILE *stream, size_t col);
void move_cursor_to_position(FILE *stream, size_t row, size_t col);

void save_terminal();
void restore_terminal();
void enter_raw_mode();

#define for_range(ItemT, ItemName, start, end) \
for (ItemT ItemName = start; ItemName < end; ItemName += 1)

typedef char * CharString;

declare_List(CharString)

typedef struct {
  List_CharString lines;
  size_t window_start;
  pthread_t reader_thread;
  int source_fd;
  char *next_line;

  // communication to reader thread
  _Atomic bool next_line_is_ready;
} Window;

declare_List(Window)


typedef enum {
  WINDOW_MOVE_UP = 'k',
  WINDOW_PAGE_UP = 0x7e355b1b,
  WINDOW_MOVE_DOWN = 'j',
  WINDOW_PAGE_DOWN = 0x7e365b1b,
  WINDOW_QUIT = 'q',
  WINDOW_SWITCH_NEXT = 'h',
  WINDOW_SWITCH_PREV = 'l',
  WINDOW_CONTROL_NONE = 0x0,
} WindowControl;

typedef union {
  char buffer[4];
  uint64_t integer;
} KeyboardCode;

Window Window_new(int source_fd);
void Window_spawn_reader(Window *self);
// returns whether the window has been updated
bool Window_update(Window *self);
void Window_render(Window *self, uint16_t offset_x, uint16_t offset_y, uint16_t width, uint16_t height, bool focused);
void Window_move_up(Window *self, size_t count);
void Window_move_down(Window *self, size_t count);
WindowControl Window_handle_input(Window *self, size_t tty_rows, bool *needs_redraw);
void Window_free(Window *self);

typedef enum {
  INTERFACE_RESULT_NONE = 0,
  INTERFACE_RESULT_QUIT,
} InterfaceCommand;

// a conecetpual screen that consumes the entire tty with
// one or more Windows dividing it
typedef struct {
  List_Window windows;
  uint8_t top_window;
  uint8_t focus;
} Screen;

InterfaceCommand Screen_read_stdin(Screen *self, bool *needs_redraw);
void Screen_spawn_stdin_reader(Screen *self);
void Screen_render(Screen *self);



