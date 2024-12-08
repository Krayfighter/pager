

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
Option_Vec_Token lex_command_line_args(char **args, size_t arg_count) {
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

struct socket_fds {
  int stdin;
  int stdout;
  int stderr;
};

struct socket_fds spawn_shell_command(char *command) {
  int subproc_stdin_pair[2];
  int subproc_stdout_pair[2];
  int subproc_stderr_pair[2];

  if (socketpair(PF_LOCAL, SOCK_STREAM, 0, subproc_stdin_pair) < 0) {
    fprintf(stderr, "Error: failed to create local socket -> %s\n", strerror(errno));
    exit(-1);
  }
  if (socketpair(PF_LOCAL, SOCK_STREAM, 0, subproc_stdout_pair) < 0) {
    fprintf(stderr, "Error: failed to create local socket -> %s\n", strerror(errno));
    exit(-1);
  }
  if (socketpair(PF_LOCAL, SOCK_STREAM, 0, subproc_stderr_pair) < 0) {
    fprintf(stderr, "Error: failed to create local socket -> %s\n", strerror(errno));
    exit(-1);
  }

  pid_t process_id;
  process_id = fork();
  if (process_id < 0) {
    fprintf(stderr, "Error, failed to fork process -> %s\n", strerror(errno));
    exit(-1);
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

    execl("/bin/sh", "/bin/sh", "-c", command, NULL);
    exit(0);
  }
  // the sockets don't work if you don't close the other end
  close(subproc_stdin_pair[1]);
  close(subproc_stdout_pair[1]);
  close(subproc_stderr_pair[1]);

  struct socket_fds structure = {
    .stdin = subproc_stdin_pair[0],
    .stdout = subproc_stdout_pair[0],
    .stderr = subproc_stderr_pair[0]
  };

  return structure;
}

declare_heap_array(int);
define_heap_array(int);

typedef struct {
  Vec_int file_descriptors;
} Invocation;

Invocation parse_command_line_arguments(Vec_Token arg_tokens) {
  Invocation state;
  state.file_descriptors = Vec_int_new(4);

  for_range(size_t, index, 0, arg_tokens.buffer_len) {
    if (arg_tokens.item_buffer[index].type == TOKEN_HELP) {
      int helptxt_fd = open("help.txt", O_NONBLOCK);
      if (helptxt_fd < 0) {
        fprintf(stderr, "Error: Failed to open help.txt -> %s\
(you may want to downlaod or view help.txt in the github repor at https://github.com/Krayfighter/pager)",
          strerror(errno)
        );
      }else { Vec_int_push(&state.file_descriptors, helptxt_fd); }
      break;
    }
  }
  for_range(size_t, token_index, 0, arg_tokens.buffer_len) {
    if (arg_tokens.item_buffer[0].type == TOKEN_SPAWN) {
      token_index += 1;
      Token *command_token = Vec_Token_get(&arg_tokens, token_index);
      if (command_token == NULL) {
        fprintf(stderr, "Error: expected command string after --spawn\n");
        exit(-1);
      }else if (command_token->type != TOKEN_STRING) {
        fprintf(stderr, "Error: expected command string after --spawn but got another token type\n");
        exit(-1);
      }
      char *command_str = command_token->option_content;

      struct socket_fds child_streams = spawn_shell_command(command_str);

      if (child_streams.stdout < 0 || child_streams.stderr < 0) {
        fprintf(stderr, "Error: failed to spawn child shell command -> %s\n", strerror(errno));
        exit(-1);
      }
      Vec_int_push(&state.file_descriptors, child_streams.stdout);
      Vec_int_push(&state.file_descriptors, child_streams.stderr);
    }
    else if (arg_tokens.item_buffer[0].type == TOKEN_STRING) {
      Token *filename_token = Vec_Token_get(&arg_tokens, token_index);
      char *filename = filename_token->option_content;

      int file_fd = open(filename, O_NONBLOCK);
      if (file_fd < 0) {
        fprintf(stderr, "Error: Failed to open file -> %s\n", strerror(errno));
        exit(-1);
      }
      Vec_int_push(&state.file_descriptors, file_fd);
    }
  }
  Vec_Token_free(&arg_tokens);
  return state;
}


int main(int32_t argc, char **argv) {

  // save terminal and restore it after main exits
  save_terminal();
  atexit(restore_terminal);

  Option_Vec_Token maybe_tokens = lex_command_line_args(argv, argc);
  if (!Option_Vec_Token_is_some(&maybe_tokens)) {
    fprintf(stderr, "Error: Failed to parse command line args (see previous)\n");
    return -1;
  }
  Vec_Token tokens = maybe_tokens.some;
  if (tokens.buffer_len == 0) {
    fprintf(stderr, "Error: missing required argument\n");
    return -1;
  }

  for_range(size_t, i, 0, argc) {
    fprintf(stderr, "DBG cli arg -> %s\n", argv[i]);
  }

  Invocation appstate = parse_command_line_arguments(tokens);

  if (appstate.file_descriptors.buffer_len == 0) {
    fprintf(stderr, "!LogicError!: input_stream should never be NULL after argument parser\n");
    return -1;
  }


  Vec_Window windows = Vec_Window_new(appstate.file_descriptors.buffer_len);
  foreach(
    int, filedes,
    appstate.file_descriptors.item_buffer,
    appstate.file_descriptors.buffer_len,
    {
      Vec_Window_push(&windows, Window_new(*filedes));
    }
  )

  Screen screen = (Screen){
    .windows = windows,
    .frame1 = Vec_Window_get(&windows, 0),
    .frame2= Vec_Window_get(&windows, 1),
    .focus = 0
  };

  enter_raw_mode();
  Screen_render(&screen);

  while (1) {
    if (Screen_read_stdin(&screen) == INTERFACE_RESULT_QUIT) {
      break;
    }

    Screen_render(&screen);

    const uint32_t MILLISECOND = 1000;
    const uint32_t SECOND = 1000 * MILLISECOND;
    const uint32_t FRAME_TIME = 60 / SECOND;
    usleep(FRAME_TIME);
  }

  foreach(
    Window, window,
    windows.item_buffer,
    windows.buffer_len,
    {
      Window_free(window);
    }
  )
  Vec_Window_free(&windows);

  foreach(
    int, filedes,
    appstate.file_descriptors.item_buffer,
    appstate.file_descriptors.buffer_len,
    {
      close(*filedes);
    }
  )
  Vec_int_free(&appstate.file_descriptors);

}


