
// // this library makes use of writer abstraction that
// // works out of the box but may be configured with
// // preprocessor macros before inclusion of this file

// #ifndef OUTPUT_STREAM
// #define OUTPUT_STREAM stdout
// #endif



// ESC
#define ESCAPE_CHAR 0x1b
#define ESCAPE_STRING "\x1b"

// CONTROL_SEQUENCE_INTRODUCER
#define CSI ESCAPE_STRING "["
// DEVICE_CONTROL_STRING
#define DCS ESCAPE_STRING "P"
// OPERATING_SYSTEM_COMMAND
#define OSC ESCAPE_STRING "]"

// terminal bell
#define BELL "\x07"


#define MOVE_CURSOR_HOME CSI "H"
// NOTE: the home position is (1,1) not (0,0)
#define MOVE_CURSOR_TO   CSI "%u;%uH"

#define MOVE_CURSOR_UP_ROWS    CSI "%uA"
#define MOVE_CURSOR_DOWN_ROWS  CSI "%uB"
#define MOVE_CURSOR_RIGHT_COLS CSI "%uC"
#define MOVE_CURSOR_LEFT_COLS  CSI "%uD"
#define MOVE_CURSOR_TO_COL     CSI "%uG"

#define SAVE_CURSOR_POSITION    ESCAPE_STRING "7"
#define RESTORE_CURSOR_POSITION ESCAPE_STRING "8"

#define ERASE_UNTIL_SCREEN_END   CSI "J"
#define ERASE_UNTIL_SCREEN_START CSI "1J"
#define ERASE_SCREEN             CSI "2J"
#define ERASE_UNTIL_LINE_END     CSI "K"
#define ERASE_UNTIL_LINE_START   CSI "1K"
#define ERASE_LINE               CSI "2K"


#define STYLE_AND_COLOR_RESET     "0"
#define STYLE_BOLD                "1"
#define STYLE_BOLD_RESET          "22"
#define STYLE_DIM                 "2"
#define STYLE_DIM_RESET           STYLE_BOLD_RESET
#define STYLE_ITALIC              "3"
#define STYLE_ITALIC_RESET        "23"
#define STYLE_UNDERLINE           "4"
#define STYLE_UNDERLINE_RESET     "24"
#define STYLE_BLINKING            "5"
#define STYLE_BLINKING_RESET      "25"
#define STYLE_REVERSE             "7"
#define STYLE_REVERSE_RESET       "27"
#define STYLE_HIDDEN              "8"
#define STYLE_HIDDEN_RESET        "28"
#define STYLE_STRIKETHROUGH       "9"
#define STYLE_STRIKETHROUGH_RESET "29"

// NOTE: setting a color is CSI <code> {optional ;} <..code> m

#define COLOR_SET_POSTFIX "m"

#define COLOR_BLACK_FG   "30"
#define COLOR_RED_FG     "31"
#define COLOR_GREEN_FG   "32"
#define COLOR_YELLOW_FG  "33"
#define COLOR_BLUE_FG    "34"
#define COLOR_MAGENTA_FG "35"
#define COLOR_CYAN_FG    "36"
#define COLOR_WHITE_FG   "37"
#define COLOR_DEFAULT_FG "39"

#define COLOR_BLACK_BG   "40"
#define COLOR_RED_BG     "41"
#define COLOR_GREEN_BG   "42"
#define COLOR_YELLOW_BG  "43"
#define COLOR_BLUE_BG    "44"
#define COLOR_MAGENTA_BG "45"
#define COLOR_CYAN_BG    "46"
#define COLOR_WHITE_BG   "47"
#define COLOR_DEFAULT_BG "49"


#define DEC_PRIVATE_RESET_HIDE_CURSOR  CSI "?25l"
#define DEC_PRIVATE_SET_SHOW_CURSOR    CSI "?25h"

#define DECSET_ENABLE_ALTERNATE_SCREEN CSI "?1046h"
#define DECSET_ENTER_ALTERNATE_SCREEN  CSI "?1049h"
#define DECRST_ENTER_NORMAL_SCREEN     CSI "?1049l"

