

#ifndef PT_ERROR_H
#define PT_ERROR_H

#define panic(err_msg, exit_code) \
fprintf(stderr, "Error: %s, in file %s on line %i\n", err_msg, __FILE__, __LINE__); \
exit(exit_code);

#define expect_errno(err_msg) \
if (errno != 0) { \
  fprintf(stderr, "Error: %s -> %s (occured in file %s, on line %i)\n", err_msg, strerror(errno), __FILE__, __LINE__); \
  exit(-1); \
}

#define expect(expr, err_msg) \
if (!expr) { \
  fprintf(stderr, "Error: %s, (occured in file %s, on line %i)\n", err_msg, __FILE__, __LINE__); \
  exit(-1); \
}
// fprintf(stderr, "Error: %s, in file %s on line %s\n")

#endif


