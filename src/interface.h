
#include "unistd.h"
#include "stdio.h"
#include "typedef_macros.h"
#include <stdint.h>


void move_cursor_to_col(FILE *stream, size_t col);
void move_cursor_to_position(FILE *stream, size_t row, size_t col);

void save_terminal();
void restore_terminal();
void enter_raw_mode();

#define for_range(ItemT, ItemName, start, end) \
for (ItemT ItemName = start; ItemName < end; ItemName += 1)

typedef struct { char *internal; } CharString;
void CharString_free(CharString *self);


declare_heap_array(CharString);

typedef struct {
  Vec_CharString lines;
  size_t window_start;
  size_t window_height;
  FILE *source;
} Window;


typedef enum {
  WINDOW_MOVE_UP = 'k',
  WINDOW_PAGE_UP = 0x7e365b1b,
  WINDOW_MOVE_DOWN = 'j',
  WINDOW_PAGE_DOWN = 0x7e355b1b,
  WINDOW_QUIT = 'q',
  WINDOW_SWITCH_NEXT = 'h',
  WINDOW_SWITCH_PREV = 'l',
  WINDOW_CONTROL_NONE = 0x0,
} WindowControl;

typedef union {
  char buffer[4];
  uint64_t integer;
} KeyboardCode;

Window Window_new(FILE *source);
void Window_update(Window *self);
void Window_render(Window *self, FILE *output_stream, size_t rows, size_t cols, bool focused);
void Window_move_up(Window *self, size_t count);
void Window_move_down(Window *self, size_t count);
WindowControl Window_handle_input(Window *self);
void Window_free(Window *self);

// a conecetpual screen that consumes the entire tty with
// one or more Windows dividing it
typedef struct {
  Window *top_window;
  Window *bottom_window;
  size_t focus;
} Screen;

typedef enum {
  INTERFACE_RESULT_NONE = 0,
  INTERFACE_RESULT_QUIT,
} InterfaceResult;

InterfaceResult Screen_read_stdin(Screen *self);
void Screen_render(Screen *self, FILE *output_stream);



