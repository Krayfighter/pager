

#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "errno.h"
#include "string.h"
#include "unistd.h"
#include "sys/socket.h"
#include "sys/fcntl.h"

#include "typedef_macros.h"

#include "interface.h"
#include <fcntl.h>




enum TokenType {
  TOKEN_HELP,
  TOKEN_SPAWN,
  TOKEN_STRING,
};

typedef struct {
  enum TokenType type;
  void *option_content;
} Token;

declare_heap_array(Token);
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
      // fcntl(subproc_stdout_pair[1], F_SETFL, O_NONBLOCK);
      if (socketpair(PF_LOCAL, SOCK_STREAM, 0, subproc_stderr_pair) < 0) {
        fprintf(stderr, "Error: failed to create local socket -> %s\n", strerror(errno));
        return -1;
      }
      // fcntl(subproc_stderr_pair[1], F_SETFL, O_NONBLOCK);

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


      main_input_stream = fdopen(subproc_stdout_pair[0], "r");
      secondary_input_stream = fdopen(subproc_stderr_pair[0], "r");
      if (!main_input_stream) {
        fprintf(stderr, "Error: failed to spawn subprocess -> %s\n", strerror(errno));
        return -1;
      }
    }
    else if (tokens.item_buffer[0].type == TOKEN_STRING) {
      bool splitscreen_view = false;
      if (tokens.buffer_len > 1) {
        fprintf(stderr, "WARN: only one additional file is currently implemented for splitscreen mode\n");
        splitscreen_view = true;
        // fprintf(stderr, "Error: unexpected arguments after filename\n");
        // return -1;
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
      if (splitscreen_view) {
        secondary_input_stream = fopen(tokens.item_buffer[1].option_content, "r");
        if (!secondary_input_stream) {
          fprintf(stderr, "Error: Failed to open file -> %s\n", strerror(errno));
          return -1;
        }
      }
    }
    // }
  }
  Vec_Token_free(&tokens);

  if (!main_input_stream) {
    fprintf(stderr, "!LogicError!: input_stream should never be NULL after argument parser\n");
    return -1;
  }

  Window window;
  window = Window_new(main_input_stream);

  Window second_window;

  Screen screen = (Screen){ .top_window = &window, .bottom_window = NULL, .focus = 0 };

  if (secondary_input_stream != NULL) {
    second_window = Window_new(secondary_input_stream);
    screen.bottom_window = &second_window;
  }

  enter_raw_mode();
  Screen_render(&screen, stdout);

  while (1) {
    if (Screen_read_stdin(&screen) == INTERFACE_RESULT_QUIT) {
      break;
    }

    Screen_render(&screen, stdout);

    const uint32_t MILLISECOND = 1000;
    const uint32_t SECOND = 1000 * MILLISECOND;
    const uint32_t FRAME_TIME = 60 / SECOND;
    usleep(FRAME_TIME);
  }

  Window_free(&window);
  if (screen.bottom_window) {
    Window_free(screen.bottom_window);
  }
  if (main_input_stream != NULL) { fclose(main_input_stream); }
  if (secondary_input_stream != NULL) { fclose(secondary_input_stream); }

}


