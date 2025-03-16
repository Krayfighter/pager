

#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "errno.h"
#include "string.h"
#include "unistd.h"
#include "sys/socket.h"
#include "sys/fcntl.h"
#include <fcntl.h>
#include <pthread.h>
#include "signal.h"
#include "sys/wait.h"
#include "errno.h"

#include "interface.h"

#include "plustypes.h"
#include "pt_error.h"


enum TokenType {
  TOKEN_HELP,
  TOKEN_SPAWN,
  TOKEN_STRING,
};

typedef struct {
  enum TokenType type;
  void *option_content;
} Token;

declare_List(Token)
define_List(Token)

// NOTE this function does NOT take ownership of the array passed
// as it assumes that it is handled by the OS (as in argv passed to main)
List_Token lex_command_line_args(char **args, size_t arg_count) {
  List_Token tokens = List_Token_new(arg_count);

  // skip the first arg which is always the executable's filename
  for_range(size_t, arg_index, 1, arg_count) {
    size_t arg_length = strlen(args[arg_index]);
    if (arg_length == 0) { continue; }
    else if (args[arg_index][0] == '-') {
      if (!strcmp(args[arg_index], "--spawn")) {
        List_Token_push(&tokens, (Token){ .type = TOKEN_SPAWN, .option_content = NULL });
        continue;
      }
      else if (!strcmp(args[arg_index], "--help")) {
        List_Token_push(&tokens, (Token) { .type = TOKEN_HELP, .option_content = NULL });
        continue;
      }
      else {
        fprintf(stderr, "unrecognized option %s\n", args[arg_index]);
        List_Token_free(&tokens);
        // return (Option_List_Token){ .none = 0x0 };
        return (List_Token){ 0 };
      }
    }
    else {
      List_Token_push(&tokens, (Token){ .type = TOKEN_STRING, .option_content = args[arg_index] });
      continue;
    }
  }

  return tokens;
}

struct socket_fds {
  int stdin;
  int stdout;
  int stderr;
  pid_t child_id;
};

struct socket_fds spawn_shell_command(char *command) {
  int subproc_stdin_pair[2];
  int subproc_stdout_pair[2];
  int subproc_stderr_pair[2];

  expect(
    (socketpair(PF_LOCAL, SOCK_STREAM, 0, subproc_stdin_pair) >= 0),
    "failed to create local domain socket"
  );
  expect(
    (socketpair(PF_LOCAL, SOCK_STREAM, 0, subproc_stdout_pair) >= 0),
    "failed to create local domain socket"
  );
  expect(
    (socketpair(PF_LOCAL, SOCK_STREAM, 0, subproc_stderr_pair) >= 0),
    "failed to create local domain socket"
  )

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
  fprintf(stderr, "DBG: Main process id %i, child id %i\n", getpid(), process_id);
  // the sockets don't work if you don't close the other end
  close(subproc_stdin_pair[1]);
  close(subproc_stdout_pair[1]);
  close(subproc_stderr_pair[1]);

  struct socket_fds structure = {
    .stdin = subproc_stdin_pair[0],
    .stdout = subproc_stdout_pair[0],
    .stderr = subproc_stderr_pair[0],
    .child_id = process_id
  };

  return structure;
}

declare_List(int)
define_List(int)

declare_List(pid_t)
define_List(pid_t)

typedef struct {
  List_int file_descriptors;
  List_pid_t children;
} Invocation;

Invocation parse_command_line_arguments(List_Token arg_tokens) {
  Invocation state;
  state.file_descriptors = List_int_new(4);
  state.children = List_pid_t_new(4);

  for_range(size_t, index, 0, arg_tokens.item_count) {
    if (arg_tokens.items[index].type == TOKEN_HELP) {
      int helptxt_fd = open("help.txt", O_NONBLOCK);
      if (helptxt_fd < 0) {
        fprintf(stderr, "Error: Failed to open help.txt -> %s\
(you may want to downlaod or view help.txt in the github repor at https://github.com/Krayfighter/pager)",
          strerror(errno)
        );
      }else { List_int_push(&state.file_descriptors, helptxt_fd); }
      break;
    }
  }
  for_range(size_t, token_index, 0, arg_tokens.item_count) {
    if (arg_tokens.items[0].type == TOKEN_SPAWN) {
      token_index += 1;
      Token *command_token = List_Token_get(&arg_tokens, token_index);
      if (command_token == NULL) {
        fprintf(stderr, "Error: expected command string after --spawn\n");
        exit(-1);
      }else if (command_token->type != TOKEN_STRING) {
        fprintf(stderr, "Error: expected command string after --spawn but got another token type\n");
        exit(-1);
      }
      char *command_str = command_token->option_content;

      struct socket_fds child_streams = spawn_shell_command(command_str);

      List_pid_t_push(&state.children, child_streams.child_id);

      if (child_streams.stdout < 0 || child_streams.stderr < 0) {
        fprintf(stderr, "Error: failed to spawn child shell command -> %s\n", strerror(errno));
        exit(-1);
      }
      List_int_push(&state.file_descriptors, child_streams.stdout);
      List_int_push(&state.file_descriptors, child_streams.stderr);
    }
    else if (arg_tokens.items[0].type == TOKEN_STRING) {
      Token *filename_token = List_Token_get(&arg_tokens, token_index);
      char *filename = filename_token->option_content;

      int file_fd = open(filename, O_NONBLOCK);
      if (file_fd < 0) {
        fprintf(stderr, "Error: Failed to open file -> %s\n", strerror(errno));
        exit(-1);
      }
      List_int_push(&state.file_descriptors, file_fd);
    }
  }
  List_Token_free(&arg_tokens);
  return state;
}


int main(int32_t argc, char **argv) {

  // save terminal and restore it after main exits
  save_terminal();
  atexit(restore_terminal);

  List_Token tokens = lex_command_line_args(argv, argc);
  if (tokens.items == NULL) {
    fprintf(stderr, "Error: Failed to parse command line args (see previous)\n");
    return -1;
  }

  Invocation appstate = parse_command_line_arguments(tokens);

  if (!isatty(STDIN_FILENO)) {
    int piped_input_fd = dup(STDIN_FILENO);
    expect((piped_input_fd >= 0), "Failed to duplicate STDIN_FILENO which is NOT a tty");
    int controlling_tty_input = open("/dev/tty", 0x0);
    expect((controlling_tty_input >= 0), "Failed to open controlling terminal from /dev/tty");
    expect(
      (dup2(controlling_tty_input, STDIN_FILENO) >= 0),
      "Failed to duplicate fd to replace STDIN with the controlling terminal"
    );

    List_int_push(&appstate.file_descriptors, piped_input_fd);
  }

  expect(
    (appstate.file_descriptors.item_count > 0),
    "no input to page over"
  );


  List_Window windows = List_Window_new(appstate.file_descriptors.item_count);
  List_foreach(int, appstate.file_descriptors, {
    List_Window_push(&windows, Window_new(*item));
  });
  List_foreach(Window, windows, {
    Window_spawn_reader(item);
  });


  // TODO implement window selector

  Screen screen = (Screen){
    .windows = windows,
    .top_window = 0,
    .focus = 0
  };


  enter_raw_mode();

  Screen_render(&screen);

  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

  // MAINLOOP
  while (1) {
    // bool needs_redraw = false;
    screen.needs_redraw = false;

    if (Screen_read_stdin(&screen) == INTERFACE_RESULT_QUIT) {
      break;
    }

    const size_t MAX_EXPECTED_TERMINAL_ROWS = 200;
    List_foreach(Window, windows, {
      size_t window_lines = item->lines.item_count;
      screen.needs_redraw |= (Window_update(item) && window_lines < 200);
    });

    if (screen.needs_redraw) {
      Screen_render(&screen);
    }


    const uint32_t MILLISECOND = 1000;
    const uint32_t SECOND = 1000 * MILLISECOND;
    const uint32_t FRAME_TIME = 10 / SECOND;
    usleep(FRAME_TIME);
  }


  // CLEANUP

  List_foreach(int, appstate.file_descriptors, { close(*item); });
  List_int_free(&appstate.file_descriptors);

  if (appstate.children.item_count != 0) {
    List_foreach(pid_t, appstate.children, {
      fprintf(stderr, "DBG: killing child process (is)%i\n", *item);
      kill(*item, SIGQUIT);
    });

    for (size_t i = 0; i < 100; i += 1) {
      List_foreach(pid_t, appstate.children, {
        // int return_state;
        if (waitpid(*item, NULL, WNOHANG) == *item) {
          List_pid_t_swapback_delete(&appstate.children, index);
          index -= 1;
        }
      });
      if (appstate.children.item_count == 0) { goto after_children_killed; }
      usleep(10000);
    }

    List_foreach(pid_t, appstate.children, {
      kill(*item, SIGKILL);
    });
  }
  after_children_killed: {};

  for (uint8_t window = 0; window < screen.windows.item_count; window += 1) {
    // int result = pthread_kill(screen.windows.items[window].reader_thread, SIGQUIT);
    // if (result != 0) {
    //   fprintf(stderr, "WARN: failed to send signal to thread, %s\n", strerror(errno));
    // }
    pthread_t thread_id = screen.windows.items[window].reader_thread;
    pthread_cancel(thread_id);
    pthread_join(thread_id, NULL);
  }

  List_foreach(Window, windows, { Window_free(item); });
  List_Window_free(&windows);

  free_residuals();

  List_pid_t_free(&appstate.children);

}


