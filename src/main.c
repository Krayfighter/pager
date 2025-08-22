
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _XOPEN_SOURCE_EXTENDED
#define _FORTIFY_SOURCE 2

#include "stdint.h"
#include "stddef.h"
#include "assert.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "limits.h"
#include "stdarg.h"
#include "poll.h"
#include "errno.h"
#include "fcntl.h"
#include "time.h"
#include "termios.h"
#include "signal.h"

#include "sys/socket.h"
#include "sys/ioctl.h"
#include "sys/mman.h"
#include "sys/stat.h"

#include "termcodes.h"

#define TODO abort()

#ifdef TEST
const char *failure_message = NULL;
const char *failure_function = NULL;
size_t failure_line = 0;
uint8_t abort_on_test_failure = false;
#endif

#define str_size(str) (sizeof(str) - 1)
#define array_size(array) (sizeof(array) / sizeof(array[0]))

#define fail(message) { \
	failure_function = __func__; \
	failure_message = "In " __FILE_NAME__ ". " message; \
	failure_line = __LINE__; \
	if (abort_on_test_failure) { abort(); } \
	return 1; \
}

#define expect(condition, message) { \
	if (!(condition)) { fail(message); } \
}

#define expect_nonnul(pointer) expect(pointer != NULL, "Null member")

#define String_of(str) { .ptr = str, .len = sizeof(str) - 1 }



// TODO find a way to do this with fewer integer divisions
uint16_t digit_count(uint64_t number) {
  uint16_t count = 1;
  while (number > 9) {
    number /= 10;
    number = number - (number % 10);
    count += 1;
  }
  return count;
}

#ifdef TEST
uint8_t TEST_digit_count() {
	expect(digit_count(~(uint64_t)0) == 20, "Unexpted digit count result");
	expect(digit_count(0) == 1, "Unexpted digit count result");
	expect(digit_count(1340983457) == 10, "Unexpted digit count result");
	expect(digit_count(609872345723456) == 15, "Unexpted digit count result");
	return 0;
}
#endif


typedef struct {
	char *ptr;
	size_t len;
} String;

// returns the result of memcmp or the result of lhs.len > rhs.len
// lhs.len > rhs.len -> 1
// lhs.len < rhs.len -> -1
// if their lengths are not equal
int String_cmp(String lhs, String rhs) {
	if (lhs.len != rhs.len) {
		// return lhs.len > rhs.len;
		return (lhs.len > rhs.len)
			? 1
			: -1;
	}
	return memcmp(lhs.ptr, rhs.ptr, lhs.len);
}

#ifdef TEST
uint8_t TEST_String_cmp() {
	String test_strings[][2] = {
		{ String_of("different lengths "), String_of("different_lengths") },
		{ String_of("different lengths 2"), String_of("different lengths 2 ") },
		{ String_of("same length"), String_of("same lentgh") },
		{ String_of("string greaterb"), String_of("string greatera") },
		{ String_of("String lesserj"), String_of("String lesserk") }
	};
	int results[] = {
		1, -1, -13, 1, -1
	};
	const size_t test_count = sizeof(test_strings) / sizeof(test_strings[0]);
	assert(test_count == sizeof(results) / sizeof(results[0]));

	for (size_t i = 0; i < test_count; i += 1) {
		int cmp_result = String_cmp(test_strings[i][0], test_strings[i][1]);
		expect(cmp_result == results[i], "Unexpected String_cmp return value");
	}

	return 0;
}
#endif

String get_next_line(
	char *source,
	size_t *_line_start,
	size_t source_len
) {
	size_t line_start = *_line_start;
	size_t line_end = line_start;

	String return_string = (String) { 0 };

	while (line_end < source_len) {
		if (source[line_end] == '\n') {
			// String new_line;
			if (line_end - 1 == line_start) {
				return_string = (String) { .ptr = "", .len = 0 };
			}else {
				return_string = (String) {
					.ptr = source + line_start,
					.len = line_end - line_start
				};
			}

			goto NEW_LINE;
		}
		line_end += 1;
	}

	return (String){ 0 };
	NEW_LINE: {};
	*_line_start = line_end + 1;
	return return_string;
}

#ifdef TEST
uint8_t TEST_get_next_line() {
	{ // test 1
		char test_string[] =
			"This is the first line of my test string\n"
			", and this is the second line\n"
			"the next one is empty\n"
			"\n"
			"then one with a \0 null byte in the middle\n"
			"and a couple\n\nmultiples\n\n\n"
		;

		String next_line = (String){ 0 };
		size_t i = 0;
		size_t line_start = 0;
		while (true) {
			size_t next_newline =
				(uintptr_t)memchr(test_string + line_start, '\n', str_size(test_string) - line_start)
				- (uintptr_t)(test_string + line_start)
			;
			size_t old_line_start = line_start;

			next_line = get_next_line(test_string, &line_start, str_size(test_string));

			if (next_line.ptr == NULL) { break; }

			expect(next_line.ptr == test_string + old_line_start, "Mismatched string pointers");
			expect(next_line.len == next_newline, "String lengths did not match");
			i += 1;
		}
		// while (next_line.ptr != NULL);
		expect(i == 10, "Unexpected iteration count");
	}

	{ // test 2
		int file_fd = open("./src/main.c", O_RDONLY);

		struct stat file_stats;
		int fstat_result = fstat(file_fd, &file_stats);
		assert(fstat_result != -1);

		char *file_map = mmap(NULL, file_stats.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0x0);

		close(file_fd);
		
		String next_line = (String){ 0 };
		size_t line_start = 0;
		while (true) {
			size_t next_newline =
				(uintptr_t)memchr(file_map + line_start, '\n', str_size(file_map) - line_start)
				- (uintptr_t)(file_map + line_start)
			;
			size_t old_line_start = line_start;

			next_line = get_next_line(file_map, &line_start, str_size(file_map));

			if (next_line.ptr == NULL) { break; }

			expect(next_line.ptr == file_map + old_line_start, "Mismatched string pointers");
		expect(next_line.len == next_newline, "String lengths did not match");
		}

		munmap(file_map, file_stats.st_size);
	}

	return 0;
}
#endif


typedef struct {
	char *buffer;
	size_t buffer_size;
	size_t buffer_filled;
} IOBuffer;

static inline IOBuffer IOBuffer_create(size_t buffer_size) {
	char *buffer = malloc(buffer_size);
	assert(buffer != NULL);

	return (IOBuffer) {
		.buffer = buffer,
		.buffer_filled = 0,
		.buffer_size = buffer_size
	};
}

static inline IOBuffer IOBuffer_from(void *ptr, size_t bytes) {
	#ifdef DEBUG
	assert(ptr != NULL);
	#endif

	return (IOBuffer) {
		.buffer = ptr,
		.buffer_filled = 0,
		.buffer_size = bytes
	};
}

void IOBuffer_destroy(IOBuffer *self) {
	free(self->buffer);
	memset(self, 0x0, sizeof(IOBuffer));
}

// Write to `self` without flushing to a sink
// upon completely filling the buffer
//
// returns the number of bytes written
size_t IOBuffer_write(IOBuffer *self, char *bytes, size_t byte_count) {
	size_t available_bytes = (self->buffer_size - self->buffer_filled);
	if (available_bytes == 0 || byte_count == 0) { return 0; }

	size_t write_size = (byte_count >= available_bytes)
		? available_bytes
		: byte_count
	;
	memcpy(self->buffer + self->buffer_filled, bytes, write_size);
	self->buffer_filled += write_size;
	return write_size;
}

#define IOBuffer_write_string(buffer, string) IOBuffer_write(buffer, string.ptr, string.len)

#ifdef TEST
uint8_t TEST_IOBuffer_write() {
	IOBuffer buffer = IOBuffer_create(20);

	String string1 = String_of("first part");
	String string2 = String_of(". Then the clipped part");
	String buffer_string = String_of("first part. Then the");

	size_t write_size = IOBuffer_write(&buffer, string1.ptr, string1.len);
	expect(write_size == 10, "Unexpected write size");
	write_size = IOBuffer_write(&buffer, string2.ptr, string2.len);
	expect(write_size == 10, "Unexpected write size");

	expect(buffer.buffer_filled == buffer.buffer_size, "Buffer was not filled or was over filled");
	expect(buffer.buffer_filled == buffer_string.len, "Buffer size did not match buffer string size");
	expect(memcmp(buffer.buffer, buffer_string.ptr, buffer_string.len) == 0, "Unexpected buffer contents");

	free(buffer.buffer);

	return 0;
}
#endif

// returns number of bytes written or negative
// number of bytes written in case of error
ssize_t IOBuffer_flush_to(IOBuffer *self, int sink) {
	#ifdef DEBUG
	assert(sink >= 0);
	#endif

	size_t net_write_size = 0;

	DO_WRITE: {};
	ssize_t write_size = write(sink, self->buffer + net_write_size, self->buffer_filled - net_write_size);
	if (write_size == -1) {
		if (net_write_size != 0) { return -(ssize_t)net_write_size; }
		return -1;
	}
	net_write_size += write_size;
	if (net_write_size < self->buffer_filled) { goto DO_WRITE; }

	self->buffer_filled = 0;
	return net_write_size;
}

#ifdef TEST
uint8_t TEST_IOBuffer_flush_to() {
	char bufmem[256];
	IOBuffer buffer = IOBuffer_from(bufmem, sizeof(bufmem));

	String contents = String_of("Test string to flush\n\0\0\nmore lines after ?\n");
	size_t write_size = IOBuffer_write(&buffer, contents.ptr, contents.len);
	expect(write_size == contents.len, "Unexpected write size");

	int file_ends[2];
	socketpair(PF_LOCAL, SOCK_STREAM, AF_LOCAL, file_ends);

	ssize_t flush_size = IOBuffer_flush_to(&buffer, file_ends[0]);
	expect(flush_size > 0, "Failed to flush buffer contents to file");
	expect((size_t)flush_size == contents.len, "Failed to flush then correct number of bytes");

	char read_buffer[256];
	ssize_t read_size = read(file_ends[1], read_buffer, 256);
	expect(read_size > -1, "Failed to perform read syscall");
	expect(read_size != 0, "invalid read returned 0 (EOF ?)");
	expect((size_t)read_size == contents.len, "did not read expected number of bytes");

	expect(memcmp(contents.ptr, read_buffer, contents.len) == 0, "Contents did not match");

	close(file_ends[0]);
	close(file_ends[1]);
	return 0;
}
#endif

// write all of the bytes in `bytes`,
// flushing the buffer to `sink` if necessary
//
// returns write size or -(write size) on error
ssize_t IOBuffer_write_all(
	IOBuffer *self,
	char *bytes, size_t byte_count,
	int sink
) {
	#ifdef DEBUG
	assert(sink > -1);
	assert(bytes != NULL);
	#endif

	if (self->buffer_filled + byte_count <= self->buffer_size) {
		size_t write_size = IOBuffer_write(self, bytes, byte_count);
		return write_size;
	}

	size_t net_write_size = 0;

	DO_WRITE: {};
	size_t buffered_write_size = IOBuffer_write(self, bytes + net_write_size, byte_count - net_write_size);
	// if (buffered_write_size < 0) { return -net_write_size + buffered_write_size; }
	net_write_size += buffered_write_size;

	if (self->buffer_filled == self->buffer_size) {
		IOBuffer_flush_to(self, sink);
		goto DO_WRITE;
	}

	#ifdef DEBUG
	assert(net_write_size == byte_count);
	#endif

	return net_write_size;
}

#ifdef TEST
uint8_t TEST_IOBuffer_write_all() {
	{ // test 1
		char buf1mem[20];
		IOBuffer buffer = IOBuffer_from(buf1mem, sizeof(buf1mem));

		int file_ends[2];
		assert(socketpair(PF_LOCAL, SOCK_STREAM, AF_LOCAL, file_ends) == 0);

		char test1_string[] = "small test.";

		size_t write_size = IOBuffer_write_all(&buffer, test1_string, sizeof(test1_string), file_ends[0]);
		expect(write_size == sizeof(test1_string), "Unexpected write size");
		{
			size_t available_bytes = 0;
			ioctl(file_ends[1], FIONREAD, &available_bytes);
			expect(available_bytes == 0, "Unexpected flush");
		}
		IOBuffer_flush_to(&buffer, file_ends[0]);

		char result_buffer[256];
		size_t read_size = read(file_ends[1], result_buffer, sizeof(result_buffer));
		expect(read_size == sizeof(test1_string), "Unexpected flush size");
		expect(memcmp(test1_string, result_buffer, sizeof(test1_string)) == 0, "Unexpected flush results");

		close(file_ends[0]);
		close(file_ends[1]);
	}

	{ // test 2
		char buffer_mem[10];
		IOBuffer buffer = IOBuffer_from(buffer_mem, sizeof(buffer_mem));

		int file_ends[2];
		assert(socketpair(PF_LOCAL, SOCK_STREAM, AF_LOCAL, file_ends) == 0);

		char test_string[] = "This is a very long test string that will overflow the buffer multiple times.";

		size_t bufwrite_size = IOBuffer_write_all(&buffer, test_string, sizeof(test_string), file_ends[0]);
		expect(bufwrite_size == sizeof(test_string), "Unexpected write size");

		size_t flush_size = IOBuffer_flush_to(&buffer, file_ends[0]);
		expect(bufwrite_size % sizeof(buffer_mem) == flush_size, "Unexpected flush size");

		char read_buffer[256];
		size_t read_size = read(file_ends[1], read_buffer, 256);
		expect(read_size == sizeof(test_string), "Unexpected read size");
		expect(memcmp(test_string, read_buffer, read_size) == 0, "Read string did not match");

		close(file_ends[0]);
		close(file_ends[1]);
	}

	return 0;
}
#endif

// returns number of bytes written or negative
// number of bytes written if output was truncated
// or SSIZE_MAX if the buffer was already full
ssize_t IOBuffer_printf(IOBuffer *self, const char *format_string, ...) {
	#ifdef DEBUG
	assert(self->buffer_size > self->buffer_filled);
	#endif
	size_t remaining_bytes = self->buffer_size - self->buffer_filled;
	if (!remaining_bytes) { return SSIZE_MAX; }

	va_list varargs;
	va_start(varargs);
	int write_size = vsnprintf(self->buffer + self->buffer_filled, remaining_bytes, format_string, varargs);
	#ifdef DEBUG
	assert(write_size > -1);
	// assert((size_t)write_size < remaining_bytes);
	#endif

	if ((size_t)write_size >= remaining_bytes) {
		self->buffer_filled = self->buffer_size;
		return -remaining_bytes;
	}

	self->buffer_filled += write_size;
	return write_size;
}

#ifdef TEST
uint8_t TEST_IOBuffer_printf() {
	{ // test 1
		char mem[256];
		IOBuffer buffer = IOBuffer_from(mem, sizeof(mem));

		// const char format[] = ;
		#define format "Hello, from world %s-%u"
		ssize_t write_size = IOBuffer_printf(&buffer, format, "zenthon", 9);
		expect(write_size > 0, "Unexpected write size");
		expect((size_t)write_size == buffer.buffer_filled, "Write size did no match buffer filled");
		size_t expected_buffer_filled = (sizeof(format) - 1) + (sizeof("zenthon") - 1) - 4 + 1;
		expect(buffer.buffer_filled == expected_buffer_filled, "Unexpect format write size");

		// const char expected_buffer_contents[] = format " zenthon-9";
		const char expected_buffer_contents[] = "Hello, from world zenthon-9";
		expect(memcmp(buffer.buffer, expected_buffer_contents, buffer.buffer_filled) == 0, "Buffer contents did not match");
	}

	{ // test 2
		char mem[10];
		IOBuffer buffer = IOBuffer_from(mem, sizeof(mem));

		const char format2[] = "This line gets clipped";
		ssize_t write_size = IOBuffer_printf(&buffer, format2);
		expect(write_size == -(ssize_t)sizeof(mem), "Unexpected return value");
		expect(buffer.buffer_filled == buffer.buffer_size, "Expected buffer to be completely filled");
		// NOTE: 9 characters because of terminating null byte
		expect(memcmp(buffer.buffer, format2, 9) == 0, "Buffer contents did not match");
	}

	{ // test 3
		char mem[256];
		IOBuffer buffer = IOBuffer_from(mem, sizeof(mem));

		ssize_t write_size = IOBuffer_printf(&buffer, MOVE_CURSOR_HOME MOVE_CURSOR_TO, 240, 173);
		expect(write_size > -1, "Unexpected printf clipping");
		size_t expected_write_size = (sizeof(MOVE_CURSOR_HOME) - 1) + (sizeof(MOVE_CURSOR_TO) - 1) + 2;
		expect((size_t)write_size == expected_write_size, "Unexpected write size");
		expect(buffer.buffer_filled == (size_t)write_size, "Expected buffer filled to match write size");
		expect(
			memcmp(buffer.buffer, MOVE_CURSOR_HOME "\x1b[240;173H", buffer.buffer_filled) == 0,
			"Buffer contents did not match expected"
		);
	}

	return 0;
}
#endif

// returns the number of bytes read
// (0 may mean either EOF or no space left)
// or -1 on error
ssize_t IOBuffer_read_from(IOBuffer *self, int source) {
	#ifdef DEBUG
	assert(source > -1);
	#endif

	size_t available_bytes = (self->buffer_size - self->buffer_filled);
	if (available_bytes == 0) { return 0; }

	ssize_t read_size = read(source, self->buffer + self->buffer_filled, available_bytes);
	if (read_size == -1) { return -1; }
	self->buffer_filled += read_size;
	return read_size;
}

#ifdef TEST
uint8_t TEST_IOBuffer_read_from() {
	{ // test 1
		char mem[256];
		IOBuffer buffer = IOBuffer_from(mem, sizeof(mem));

		int file_ends[2];
		assert(socketpair(PF_LOCAL, SOCK_STREAM, AF_LOCAL, file_ends) != -1);

		String test_string = String_of("this test string should come out unmodified");
		ssize_t write_size = write(file_ends[0], test_string.ptr, test_string.len);
		expect(write_size != -1, "Failed write");
		expect((size_t)write_size == test_string.len, "Unexpected write size");

		ssize_t read_size = IOBuffer_read_from(&buffer, file_ends[1]);
		expect(read_size != -1, "Failed read");
		expect((size_t)read_size == test_string.len, "Unexpected read size");
		expect(test_string.len == buffer.buffer_filled, "Buffer filled did not match read size");
		expect(memcmp(mem, test_string.ptr, test_string.len) == 0, "Read string did not match reference");

		close(file_ends[0]);
		close(file_ends[1]);
	}

	{ // test 2
		char mem[20];
		IOBuffer buffer = IOBuffer_from(mem, sizeof(mem));

		int file_ends[2];
		assert(socketpair(PF_LOCAL, SOCK_STREAM, AF_LOCAL, file_ends) != -1);

		String test_string = String_of("This string should be clipped. This does not appear");

		ssize_t write_size = write(file_ends[0], test_string.ptr, test_string.len);
		expect(write_size != -1, "Failed write");
		// expect(write_size == sizeof(mem), "Write size did not match buffer size");

		ssize_t read_size = IOBuffer_read_from(&buffer, file_ends[1]);
		expect(read_size != -1, "Failed read");
		expect((size_t)read_size == buffer.buffer_filled, "Read size did not match buffer filled");
		expect(buffer.buffer_filled == sizeof(mem), "Buffer filled did not match sizeof mem");
		expect(buffer.buffer_filled == buffer.buffer_size, "Buffer filled did not match buffer size");
		expect(memcmp(test_string.ptr, mem, buffer.buffer_filled) == 0, "Read string did not match clipped test string");

		close(file_ends[0]);
		close(file_ends[1]);
	}

	return 0;
}
#endif

// read from source, reallocating if necessary
// BUG: may cause blocking if the available bytes
// in `source` equals the number of available bytes
//
// returns the number of bytes read, or negative number
// of bytes read on error
//
// if `old_address` is not null, then this funciton sets
// it to the old address if this function reallocates and
// the buffer location moves
ssize_t IOBuffer_read_from_realloc(IOBuffer *self, int source, uintptr_t *old_address) {
	#ifdef DEBUG
	assert(source > -1);
	#endif

	uintptr_t original_address = (uintptr_t)self->buffer;

	size_t net_bytes_read = 0;

	// minimum number of available bytes for read
	#define PADDING_SIZE 64

	DO_READ: {};
	size_t available_bytes = (self->buffer_size - self->buffer_filled);
	if (available_bytes < PADDING_SIZE) {
		self->buffer_size *= 2;
		// self->buffer = realloc(self->buffer, self->buffer_size);
		void *new_buffer = realloc(self->buffer, self->buffer_size);
		assert(new_buffer != NULL);

		if (old_address != NULL && new_buffer != self->buffer) {
			*old_address = original_address;
		}
		self->buffer = new_buffer;
		available_bytes = self->buffer_filled;
	}


	ssize_t read_size = read(source, self->buffer + self->buffer_filled, available_bytes);
	if (read_size == -1) { return (net_bytes_read == 0) ? -1 : -(ssize_t)net_bytes_read; }
	self->buffer_filled += read_size;
	net_bytes_read += read_size;
	if (self->buffer_size - self->buffer_filled < PADDING_SIZE) { goto DO_READ; }

	return net_bytes_read;
}

#ifdef TEST
uint8_t TEST_IOBuffer_read_from_realloc() {
	{ // test 1
		char mem[256];
		IOBuffer buffer = IOBuffer_from(mem, sizeof(mem));

		int file_ends[2];
		assert(socketpair(PF_LOCAL, SOCK_STREAM, AF_LOCAL, file_ends) != -1);

		String test_string = String_of("example test string");
		ssize_t write_size = write(file_ends[0], test_string.ptr, test_string.len);
		expect(write_size != -1, "Failed write");
		expect((size_t)write_size == test_string.len, "Did not write all bytes");

		ssize_t read_size = IOBuffer_read_from_realloc(&buffer, file_ends[1], NULL);
		expect(read_size != -1, "Failed read");
		expect((size_t)read_size == buffer.buffer_filled, "Read size did not match buffer filled");
		expect(buffer.buffer_filled == test_string.len, "Buffer filled did not match expected size");
		expect(memcmp(mem, test_string.ptr, test_string.len) == 0, "Read string did not match expected");

		close(file_ends[0]);
		close(file_ends[1]);
	}

	{ // test 2
		IOBuffer buffer = IOBuffer_create(10);

		int file_ends[2];
		assert(socketpair(PF_LOCAL, SOCK_STREAM, AF_LOCAL, file_ends) != -1);

		String test_string = String_of("This string will realloc");
		// expect(test_string.len == 20, "Unexpected test string size");

		ssize_t write_size = write(file_ends[0], test_string.ptr, test_string.len);
		expect(write_size != -1, "Failed write");
		expect((size_t)write_size == test_string.len, "Unexpected write size");

		ssize_t read_size = IOBuffer_read_from_realloc(&buffer, file_ends[1], NULL);
		expect(read_size > -1, "Failed read");
		expect((size_t)read_size == test_string.len, "Unexpected read size");
		expect(test_string.len == buffer.buffer_filled, "Buffer filled did not match read size");
		// expect(buffer.buffer_size == 20, "Unexpected buffer size (too many reallocs?)");
		// expect(buffer.buffer_filled  == buffer.buffer_size, "Buffer was not completely filled");
		expect(memcmp(buffer.buffer, test_string.ptr, test_string.len) == 0, "Buffer contents did not match reference");

		IOBuffer_destroy(&buffer);
	}

	// BUG: this is a working example of the blocking that can occur
	// with the function being tested
	// { // test 3
	// 	IOBuffer buffer = IOBuffer_create(10);

	// 	int file_ends[2];
	// 	assert(socketpair(PF_LOCAL, SOCK_STREAM, AF_LOCAL, file_ends) != -1);

	// 	String test_string = String_of("This bloc");

	// 	ssize_t write_size = write(file_ends[0], test_string.ptr, test_string.len);
	// 	expect(write_size != -1, "Failed write");
	// 	expect((size_t)write_size == test_string.len, "Unexpected write size");

	// 	ssize_t read_size = IOBuffer_read_from_realloc(&buffer, file_ends[0]);
	// 	expect(read_size != -1, "Failed read");
	// 	expect((size_t)read_size == test_string.len, "Unexpected read size");
	// 	expect(test_string.len = buffer.buffer_filled, "buffer length did not match reference");
	// 	expect(memcmp(buffer.buffer, test_string.ptr, test_string.len) == 0, "Buffer contents did not match reference");
	// }

	return 0;
}
#endif


typedef struct {
	size_t string_count;
	size_t line_count;
	size_t top_line;
} StringsData;

typedef struct {
	String **strings_col;
	StringsData *data_col;
	size_t row_count;
} StringTable;

// returns (StringTable){ 0 } on error
StringTable StringTable_create() {
	#define DEFAULT_STRINGTABLE_ROW_COUNT 8

	void *strings = malloc(sizeof(String *) * DEFAULT_STRINGTABLE_ROW_COUNT);
	if (strings == NULL) { goto FAILED_STRINGS_MALLOC; }

	void *data = malloc(sizeof(StringsData) * DEFAULT_STRINGTABLE_ROW_COUNT);
	if (data == NULL) { goto FAILED_DATA_MALLOC; }

	memset(strings, 0x0, sizeof(String *) * DEFAULT_STRINGTABLE_ROW_COUNT);
	memset(data, 0x0, sizeof(StringsData) * DEFAULT_STRINGTABLE_ROW_COUNT);

	return (StringTable) {
		.strings_col = strings,
		.data_col = data,
		.row_count = DEFAULT_STRINGTABLE_ROW_COUNT
	};

	FAILED_DATA_MALLOC: {};
	free(strings);
	FAILED_STRINGS_MALLOC: {};
	return (StringTable){ 0 };
}

#ifdef TEST
// returns 1 on failure
uint8_t TEST_StringTable_create() {
	StringTable table = StringTable_create();
	// if (table.row_count != 8) { return 1; }
	expect(table.row_count == 8, "Unexpect row count");
	// if (table.strings_col == NULL) { return 1; }
	expect(table.strings_col != NULL, "Null member");
	// if (table.data_col == NULL) { return 1; }
	expect(table.data_col != NULL, "Null member");

	char mem[sizeof(StringsData) * 8] = { 0 };
	expect(
		memcmp(table.strings_col, mem, sizeof(String *) * 8) == 0,
		"Malloc did not return zeroed memory"
	)
	expect(
		memcmp(table.data_col, mem, sizeof(StringsData) * 8) == 0,
		"Malloc did not return zeroed memory"
	)

	free(table.strings_col);
	free(table.data_col);
	return 0;
}
#endif

void StringTable_destroy(StringTable *self) {
	free(self->strings_col);
	free(self->data_col);
	memset(self, 0x0, sizeof(StringTable));
}

#ifdef TEST
uint8_t TEST_StringTable_destroy() {
	StringTable table = StringTable_create();
	assert(table.strings_col != NULL);

	StringTable_destroy(&table);

	char expected_mem[sizeof(table)] = { 0 };
	// if (memcmp(&table, expected_mem, sizeof(table)) != 0) { return 1; }
	expect(memcmp(&table, expected_mem, sizeof(table)) == 0, "Failed to zero out structure");
	return 0;
}
#endif

// returns the id of the inserted row
size_t StringTable_insert(
	StringTable *self,
	String *strings,
	size_t strings_max,
	size_t strings_count
) {
	#ifdef DEBUG
	assert(self->strings_col != NULL);
	assert(self->data_col != NULL);
	assert(self->row_count != 0);
	assert(strings != NULL);
	#endif

	for (size_t i = 0; i < self->row_count; i += 1) {
		if (self->strings_col[i] == NULL) {
			self->strings_col[i] = strings;
			self->data_col[i] = (StringsData) {
				.string_count = strings_max,
				.line_count = strings_count,
				.top_line = 0
			};
			return i;
		}
	}

	self->strings_col = realloc(self->strings_col, 2 * self->row_count * sizeof(String *));
	assert(self->strings_col != NULL);
	memset(self->strings_col + self->row_count, 0x0, self->row_count * sizeof(String *));

	self->data_col = realloc(self->data_col, 2 * self->row_count * sizeof(StringsData));
	assert(self->data_col != NULL);

	size_t row_id = self->row_count;
	self->row_count *= 2;

	self->strings_col[row_id] = strings;
	self->data_col[row_id] = (StringsData) {
		.string_count = strings_max,
		.line_count = strings_count,
		.top_line = 0
	};

	return row_id;
}

#ifdef TEST
uint8_t TEST_StringTable_insert() {
	StringTable table = StringTable_create();
	assert(table.strings_col != NULL);

	String example_strings[4] = {
		String_of("String one"),
		String_of("String two")
	};

	size_t row_count = table.row_count;
	for(size_t i = 0; i <= row_count; i += 1) {
		size_t id = StringTable_insert(&table, example_strings, 4, 2);
		expect(id == i, "Failed to perform insert");
	}
	// if (row_count != table.row_count) { return 1; }
	expect(row_count * 2 == table.row_count, "Unexpected row count");
	size_t row_id = StringTable_insert(&table, example_strings, 4, 2);
	// if (row_id != row_count) { return 1; }
	expect(row_id == row_count + 1, "Insert did not reallocate");
	// if (row_count * 2 != table.row_count) { return 1; }
	expect(row_count * 2 == table.row_count, "Unexpected reallocation size");

	for (size_t i = row_id + 1; i < row_count; i += 1) {
		// if (table.strings_col[i] != NULL) { return 1; }
		expect(table.strings_col[i] == NULL, "Reallocated space not zeroed properly");
	}

	StringTable_destroy(&table);
	return 0;
}
#endif

typedef struct {
	size_t strtable_row;
} StaticBuffer;

typedef struct {
	IOBuffer buffer;
	size_t strtable_row;
	size_t next_line_start;
} ReaderBuffer;

typedef struct {
	size_t stdout_reader_id;
	size_t stderr_reader_id;
	pid_t child_id;
} ChildBuffer;

typedef union {
	StaticBuffer static_buffer;
	ReaderBuffer reader_buffer;
	ChildBuffer child_buffer;
} BufferEntity;

typedef enum: uint8_t {
	BUFFER_INVALID = 0,
	BUFFER_STATIC,
	BUFFER_READER,
	BUFFER_CHILD,
} BufferEntityKind;

typedef struct {
	BufferEntity *entities;
	BufferEntityKind *entity_kinds;
	size_t entities_max;
} BufferEntityTable;

// returns (BufferEntityTable){ 0 } on error
BufferEntityTable BufferEntityTable_create() {
	#define DEFAULT_BUFFER_ENTITY_COUNT 8

	void *entities = malloc(sizeof(BufferEntity) * DEFAULT_BUFFER_ENTITY_COUNT);
	assert(entities != NULL);

	void *entity_kinds = malloc(sizeof(BufferEntityKind) * DEFAULT_BUFFER_ENTITY_COUNT);
	assert(entity_kinds != NULL);
	memset(entity_kinds, 0x0, sizeof(BufferEntityKind) * DEFAULT_BUFFER_ENTITY_COUNT);

	return (BufferEntityTable) {
		.entities = entities,
		.entity_kinds = entity_kinds,
		.entities_max = DEFAULT_BUFFER_ENTITY_COUNT
	};
}

#ifdef TEST
uint8_t TEST_BufferEntityTable_create() {
	BufferEntityTable table = BufferEntityTable_create();
	expect_nonnul(table.entities);
	expect_nonnul(table.entity_kinds);
	expect(table.entities_max == 8, "Unexpected row count");

	char mem[sizeof(BufferEntityKind) * 8] = { 0 };
	expect(memcmp(table.entity_kinds, mem, sizeof(BufferEntityKind) * 8) == 0, "Failed to zero memory");

	free(table.entities);
	free(table.entity_kinds);
	return 0;
}
#endif

void BufferEntityTable_destroy(BufferEntityTable *self) {
	free(self->entities);
	free(self->entity_kinds);
	memset(self, 0x0, sizeof(BufferEntityTable));
}

#ifdef TEST
uint8_t TEST_BufferEntityTable_destroy() {
	BufferEntityTable table = BufferEntityTable_create();
	assert(table.entities != NULL);
	BufferEntityTable_destroy(&table);

	char expect_mem[sizeof(table)] = { 0 };
	// if (memcmp(&table, expect_mem, sizeof(table)) != 0) { return 1; }
	expect(memcmp(&table, expect_mem, sizeof(table)) == 0, "Failed to zero out structure");
	return 0;
}
#endif

// returns the id of the inserted row
size_t BufferEntityTable_insert(
	BufferEntityTable *self,
	BufferEntity entity,
	BufferEntityKind entity_kind
) {
	#ifdef DEBUG
	assert(self->entities != NULL);
	assert(self->entity_kinds != NULL);
	assert(self->entities_max != 0);
	#endif

	for (size_t i = 0; i < self->entities_max; i += 1) {
		if (self->entity_kinds[i] == BUFFER_INVALID) {
			self->entities[i] = entity;
			self->entity_kinds[i] = entity_kind;
			return i;
		}
	}

	size_t entity_id = self->entities_max;
	self->entities_max *= 2;

	self->entities = realloc(self->entities, self->entities_max * sizeof(BufferEntity));
	assert(self->entities != NULL);

	self->entity_kinds = realloc(self->entity_kinds, self->entities_max * sizeof(BufferEntityKind));
	assert(self->entity_kinds != NULL);
	memset(self->entity_kinds + entity_id, 0x0, entity_id * sizeof(BufferEntityKind));

	self->entities[entity_id] = entity;
	self->entity_kinds[entity_id] = entity_kind;
	return entity_id;
}

#ifdef TEST
uint8_t TEST_BufferEntityTable_insert() {
	BufferEntityTable table = BufferEntityTable_create();
	assert(table.entity_kinds != NULL);

	size_t table_size = table.entities_max;
	for (size_t i = 0; i <= table_size; i += 1) {
		size_t entity_id = BufferEntityTable_insert(&table, (BufferEntity){ 0 }, BUFFER_STATIC);
		expect(entity_id == i, "Failed insert");
	}
	expect(table.entities_max == table_size * 2, "Unexpected row count (no realloc ?)");

	size_t entity_id = BufferEntityTable_insert(&table, (BufferEntity){ 0 }, BUFFER_STATIC);
	expect(entity_id == table_size + 1, "Unexpected row count (no realloc ?)");

	for (size_t i = entity_id + 1; i < table.entities_max; i += 1) {
		expect(table.entity_kinds[i] == 0, "Realloc did not properly zero memory");
	}

	BufferEntityTable_destroy(&table);
	return 0;
}
#endif


typedef struct {
	struct pollfd *fds;
	size_t *entity_ids;
	size_t fds_max;
} PollfdTable;

PollfdTable PollfdTable_create() {
	struct pollfd *pollfds = malloc(8 * sizeof(struct pollfd));
	assert(pollfds != NULL);

	size_t *entity_ids = malloc(8 * sizeof(size_t));
	assert(entity_ids != NULL);

	for (size_t i = 0; i < 8; i += 1) {
		pollfds[i] = (struct pollfd){ .fd = -1 };
	}

	return (PollfdTable) {
		.fds = pollfds,
		.entity_ids = entity_ids,
		.fds_max = 8
	};
}

#ifdef TEST
uint8_t TEST_PollfdTable_create() {
	PollfdTable table = PollfdTable_create();
	expect_nonnul(table.fds);
	expect_nonnul(table.entity_ids);
	expect(table.fds_max == 8, "Unexpected table size");

	for (size_t i = 0; i < 8; i += 1) {
		expect(table.fds[i].fd == -1, "Non-ignored fd in created table");
		expect(table.fds[i].events == 0, "Events flags not zeroed");
		expect(table.fds[i].revents == 0, "REvent falgs not zeroed");
	}

	free(table.fds);
	free(table.entity_ids);

	return 0;
}
#endif

void PollfdTable_destroy(PollfdTable *self) {
	free(self->fds);
	free(self->entity_ids);
	memset(self, 0x0, sizeof(PollfdTable));
}

#ifdef TEST
uint8_t TEST_PollfdTable_destroy() {
	PollfdTable table = PollfdTable_create();
	PollfdTable_destroy(&table);

	char mem[sizeof(table)] = { 0 };
	expect(memcmp(&table, mem, sizeof(mem)) == 0, "Failed to zero memory");

	return 0;
}
#endif

size_t PollfdTable_insert(PollfdTable *self, struct pollfd fd, size_t entity_id) {
	#ifdef DEBUG
	assert(fd.fd != -1);
	#endif

	for (size_t i = 0; i < self->fds_max; i += 1) {
		if (self->fds[i].fd == -1) {
			self->fds[i] = fd;
			self->entity_ids[i] = entity_id;
			return i;
		}
	}

	self->fds = realloc(self->fds, 2 * self->fds_max * sizeof(struct pollfd));
	assert(self->fds != NULL);

	self->entity_ids = realloc(self->entity_ids, 2 * self->fds_max * sizeof(size_t));
	assert(self->entity_ids != NULL);
	
	for (size_t i = self->fds_max; i < self->fds_max * 2; i += 1) {
		self->fds[i] = (struct pollfd) { .fd = -1 };
	}

	size_t pollfd_id = self->fds_max;
	self->fds_max *= 2;

	self->fds[pollfd_id] = fd;
	self->entity_ids[pollfd_id] = entity_id;
	return pollfd_id;
}

#ifdef TEST
uint8_t TEST_PollfdTable_insert() {
	PollfdTable table = PollfdTable_create();

	for (size_t i = 0; i < 8; i += 1) {
		size_t fd_id = PollfdTable_insert(&table, (struct pollfd){ .fd = -2 }, SIZE_MAX);
		expect(fd_id == i, "Unexpected fd id");
	}
	expect(table.fds_max == 8, "Unexpected realloc");

	size_t row_count = table.fds_max;
	size_t fd_id = PollfdTable_insert(&table, (struct pollfd){ .fd = -3 }, SIZE_MAX);
	expect(fd_id == row_count, "ID after realloc did not match previous row count");
	expect(table.fds_max == 2 * row_count, "Expected table to realloc");

	for (size_t i = 0; i < row_count; i += 1) {
		expect(table.fds[i].fd == -2, "Unexpected file descriptor");
		expect(table.fds[i].events == 0x0, "non-zeroed pollfd member");
		expect(table.fds[i].revents == 0x0, "non-zeroed pollfd member");
	}
	expect(table.fds[row_count].fd == -3, "Unexpected file descriptor");
	expect(table.fds[row_count].events == 0x0, "non-zeroed pollfd member");
	expect(table.fds[row_count].revents == 0x0, "non-zeroed pollfd member");

	for (size_t i = fd_id + 1; i < table.fds_max; i += 1) {
		expect(table.fds[i].fd == -1, "Unexpected file descriptor");
		expect(table.fds[i].events == 0x0, "non-zeroed pollfd member");
		expect(table.fds[i].revents == 0x0, "non-zeroed pollfd member");
	}

	PollfdTable_destroy(&table);

	return 0;
}
#endif


typedef struct {
	StringTable string_table;
	PollfdTable pollfd_table;
	BufferEntityTable entity_table;
} BufferTable;

// returns (BufferTable){ 0 } on error
BufferTable BufferTable_create() {
	StringTable string_table = StringTable_create();
	PollfdTable pollfd_table = PollfdTable_create();
	BufferEntityTable entity_table = BufferEntityTable_create();

	return (BufferTable) {
		.string_table = string_table,
		.pollfd_table = pollfd_table,
		.entity_table = entity_table,
	};
}

#ifdef TEST
uint8_t TEST_BufferTable_create() {
	BufferTable table = BufferTable_create();

	expect_nonnul(table.string_table.strings_col);
	expect_nonnul(table.string_table.data_col);
	expect(table.string_table.row_count == 8, "Unexpected row count");

	expect_nonnul(table.pollfd_table.fds);
	expect_nonnul(table.pollfd_table.entity_ids);
	expect(table.pollfd_table.fds_max == 8, "Unexpect row count");

	expect_nonnul(table.entity_table.entities);
	expect_nonnul(table.entity_table.entity_kinds);
	expect(table.entity_table.entities_max == 8, "Unexpected row count");

	StringTable_destroy(&table.string_table);
	PollfdTable_destroy(&table.pollfd_table);
	BufferEntityTable_destroy(&table.entity_table);
	return 0;
}
#endif

void BufferTable_destroy(BufferTable *self) {
	#ifdef DEBUG
	assert(self->string_table.strings_col != NULL);
	assert(self->string_table.data_col != NULL);
	assert(self->entity_table.entities != NULL);
	assert(self->entity_table.entity_kinds != NULL);
	#endif
	uint8_t has_children_to_kill = false;
	for (size_t i = 0; i < self->entity_table.entities_max; i += 1) {
		switch (self->entity_table.entity_kinds[i]) {
			case BUFFER_INVALID: continue;
			case BUFFER_STATIC: continue;
			case BUFFER_READER: {
				ReaderBuffer buffer = self->entity_table.entities[i].reader_buffer;
				String *strings_buffer = self->string_table.strings_col[buffer.strtable_row];
				free(strings_buffer);
				IOBuffer_destroy(&buffer.buffer);
			}; break;
			case BUFFER_CHILD: {
				ChildBuffer buffer = self->entity_table.entities[i].child_buffer;
				kill(buffer.child_id, SIGTERM);
				has_children_to_kill = true;
			}; break;
		}
	}
	// TODO investigate if its necessary to do a hard kill
	// if (has_children_to_kill) {
	// 	for (size_t i = 0; i < self->entity_table.entities_max; i += 1) {
	// 		if (self->entity_table.entity_kinds[i] == BUFFER_CHILD) {
	// 			wait()
	// 		}
	// 	}
	// }
	StringTable_destroy(&self->string_table);
	PollfdTable_destroy(&self->pollfd_table);
	BufferEntityTable_destroy(&self->entity_table);
}

#ifdef TEST
uint8_t TEST_BufferTable_destroy() {
	BufferTable table = BufferTable_create();
	assert(table.string_table.strings_col != NULL);
	BufferTable_destroy(&table);

	char mem[sizeof(table)] = { 0 };
	expect(memcmp(&table, mem, sizeof(table)) == 0, "Failed to zero out structure");
	return 0;
}
#endif

// returns the ID of the inserted buffer
size_t BufferTable_insert_StaticBuffer(
	BufferTable *self, String *strings, size_t string_count
) {
	size_t strings_rowid = StringTable_insert(&self->string_table, strings, string_count, string_count);

	BufferEntity entity = (BufferEntity) {
		.static_buffer = (StaticBuffer) { .strtable_row = strings_rowid }
	};

	return BufferEntityTable_insert(&self->entity_table, entity, BUFFER_STATIC);
}

#ifdef TEST
uint8_t TEST_BufferTable_insert_StaticBuffer() {
	BufferTable table = BufferTable_create();

	String test_strings[] = {
		String_of("first test string"),
		String_of("second test string")
	};
	size_t buffer_id = BufferTable_insert_StaticBuffer(&table, test_strings, 2);

	expect(buffer_id == 0, "Unexpected id");

	size_t strtable_id = table.entity_table.entities[buffer_id].static_buffer.strtable_row;
	expect(
		String_cmp(
			table.string_table.strings_col[strtable_id][0],
			(String)String_of("first test string")
		) == 0,
		"Unexpected string value"
	);
	expect(
		String_cmp(
			table.string_table.strings_col[strtable_id][1],
			(String)String_of("second test string")
		) == 0,
		"Unexpected string value"
	);
	expect(table.string_table.data_col[strtable_id].string_count == 2, "wrong string count");
	expect(table.string_table.data_col[strtable_id].line_count == 2, "mismatched line count");
	expect(table.string_table.data_col[strtable_id].top_line == 0, "Undefined behavior on top_line");

	BufferTable_destroy(&table);
	return 0;
}
#endif

size_t BufferTable_insert_ReaderBuffer(BufferTable *self, struct pollfd source) {
	#ifdef DEBUG
	assert(source.fd != -1);
	#endif

	IOBuffer buffer = IOBuffer_create(4096);

	#define READERBUFFER_DEFAULT_STRINGS_SIZE 64
	String *strings_buffer = malloc(READERBUFFER_DEFAULT_STRINGS_SIZE * sizeof(String));
	assert(strings_buffer != NULL);

	size_t strtable_row = StringTable_insert(
		&self->string_table, strings_buffer,
		READERBUFFER_DEFAULT_STRINGS_SIZE, 0
	);

	size_t entity_id = BufferEntityTable_insert(
		&self->entity_table,
		(BufferEntity){ .reader_buffer = (ReaderBuffer){
			.buffer = buffer,
			.strtable_row = strtable_row,
			.next_line_start = 0
		} },
		BUFFER_READER
	);

	size_t pollfd_id = PollfdTable_insert(&self->pollfd_table, source, entity_id);
	(void)pollfd_id;

	return entity_id;
}

#ifdef TEST
uint8_t TEST_BufferTable_insert_ReaderBuffer() {
	{ // test 1
		BufferTable table = BufferTable_create();

		size_t buffer_id = BufferTable_insert_ReaderBuffer(&table, (struct pollfd){ .fd = -2 });

		expect(buffer_id == 0, "Unexpected buffer id");
		expect(table.pollfd_table.entity_ids[0] == buffer_id, "pollfd table did not contain expected buffer id trigger");
		expect(table.string_table.strings_col[0] != NULL, "Did not allocate or otherwise assign strings entry in table");
		expect(table.entity_table.entity_kinds[0] == BUFFER_READER, "Unexpected buffer kind");
		expect(table.entity_table.entities[0].reader_buffer.strtable_row == 0, "Unexpected buffer strtable row");

		BufferTable_destroy(&table);
	}

	{ // test 2
		BufferTable table = BufferTable_create();

		String test_strings[] = {
			String_of("this is the first test string"),
			String_of("this is the second test string"),
			String_of(""),
			String_of("the previous was empty")
		};

		(void)BufferTable_insert_StaticBuffer(&table, test_strings, array_size(test_strings));
		(void)BufferTable_insert_StaticBuffer(&table, test_strings, array_size(test_strings));
		(void)BufferTable_insert_StaticBuffer(&table, test_strings, array_size(test_strings));
		size_t reader_buffer_id = BufferTable_insert_ReaderBuffer(&table, (struct pollfd){ .fd = -2 });
		(void)BufferTable_insert_StaticBuffer(&table, test_strings, array_size(test_strings));
		(void)BufferTable_insert_StaticBuffer(&table, test_strings, array_size(test_strings));
		(void)BufferTable_insert_StaticBuffer(&table, test_strings, array_size(test_strings));
		(void)BufferTable_insert_StaticBuffer(&table, test_strings, array_size(test_strings));

		expect(reader_buffer_id == 3, "Unexpected reader buffer id");
		expect(table.pollfd_table.entity_ids[0] == 3, "Unexpected pollfd trigger id");

		expect(table.entity_table.entities_max == 8, "Unexpected realloc");

		(void)BufferTable_insert_StaticBuffer(&table, test_strings, array_size(test_strings));

		expect(table.entity_table.entities_max == 16, "Expected realloc");

		BufferTable_destroy(&table);
	}

	return 0;
}
#endif

size_t BufferTable_insert_ChildBuffer(BufferTable *self, char *command, char  **out_error_string) {
	char *shell_path = getenv("SHELL");
	shell_path = (shell_path == NULL) ? "/bin/sh" : shell_path;

	int child_stdout_slave = -1; // parent
	int child_stdin_slave = -1;
	int child_stdout = -1; // child
	int child_stdin = -1;
	{
		// NOTE: the child gets the master end of the terminal
		child_stdout = posix_openpt(O_RDWR | O_NOCTTY);
		child_stdin = dup(child_stdout);
		if (child_stdout == -1) {
			*out_error_string = "failed to obtain an unused terminal";
			return SIZE_MAX;
		}
		int grant_result = grantpt(child_stdout);
		if (grant_result != 0) {
			*out_error_string = "failed to claim ownership of opbtained terminal";
			return SIZE_MAX;
		}
		int unlock_result = unlockpt(child_stdout);
		if (unlock_result != 0) {
			*out_error_string = "failed to unlock terminal master for opening slaves";
			return SIZE_MAX;
		}
		char *stdout_slave_name = ptsname(child_stdout);
		if (stdout_slave_name == NULL) {
			*out_error_string = "failed to get slave name of obtained master terminal";
			return SIZE_MAX;
		}
		child_stdout_slave = open(stdout_slave_name, O_WRONLY);
		child_stdin_slave = open(stdout_slave_name, O_RDONLY);
	}
	assert(child_stdout_slave != -1);
	assert(child_stdin_slave != -1);
	assert(child_stdout != -1);
	assert(child_stdin != -1);

	int child_stderr_slave = -1;
	int child_stderr = -1;
	{
		child_stderr = posix_openpt(O_RDWR | O_NOCTTY);
		if (child_stderr == -1) {
			*out_error_string = "Failed to obtain an unused terminal (for stderr)";
			return SIZE_MAX;
		}
		int grant_result = grantpt(child_stderr);
		if (grant_result != 0) {
			*out_error_string = "failed to claim ownership of obtained terminal (stderr)";
			return SIZE_MAX;
		}
		int unlock_result = unlockpt(child_stderr);
		if (unlock_result != 0) {
			*out_error_string = "failed to unlock terminal master for opening slave (stderr)";
			return SIZE_MAX;
		}
		char *stderr_slave_name = ptsname(child_stderr);
		if (stderr_slave_name == NULL) {
			*out_error_string = "failed to get slave name for master terminal (stderr)";
			return SIZE_MAX;
		}
		child_stderr_slave = open(stderr_slave_name, O_WRONLY);
	}
	assert(child_stderr != -1);
	assert(child_stderr_slave != -1);

	int fork_result = fork();
	if (fork_result < 0) {
		fprintf(stderr, "Failed to fork -> %s\n", strerror(errno));
		abort();
	}
	if (fork_result == 0) {
		dup2(child_stdin_slave, STDIN_FILENO);
		dup2(child_stdout_slave, STDOUT_FILENO);
		dup2(child_stderr_slave, STDERR_FILENO);
		close_range(STDERR_FILENO + 1, ~0, 0x0);

		execl(shell_path, shell_path, "-c", command, NULL);

		abort(); // for linting only
	}else {
		close(child_stdin_slave);
		close(child_stdout_slave);
		close(child_stderr_slave);

		// TODO stdin passthrough

		size_t child_stdout_id = BufferTable_insert_ReaderBuffer(
			self, (struct pollfd){ .fd = child_stdout, POLLIN }
		);
		size_t child_stderr_id = BufferTable_insert_ReaderBuffer(
			self, (struct pollfd){ .fd = child_stderr, POLLIN }
		);

		ChildBuffer child_buffer = {
			.stdout_reader_id = child_stdout_id,
			.stderr_reader_id = child_stderr_id,
			.child_id = fork_result
		};

		size_t child_buffer_id = BufferEntityTable_insert(
			&self->entity_table,
			(BufferEntity){ .child_buffer = child_buffer },
			BUFFER_CHILD
		);

		return child_buffer_id;
	}
}
static inline size_t BufferTable_get_strtable_id(BufferTable *self, size_t buffer_id);
void BufferTable_display(
	BufferTable *self, IOBuffer *buffer, size_t entity_id,
	uint16_t offset_cols, uint16_t offset_rows,
	uint16_t render_cols, uint16_t render_rows
) {

	size_t strtable_rowid = BufferTable_get_strtable_id(self, entity_id);
	// size_t strtable_rowid = SIZE_MAX;
	// switch (self->entity_table.entity_kinds[entity_id]) {
	// 	case BUFFER_INVALID: abort();
	// 	case BUFFER_STATIC: {
	// 		strtable_rowid = self->entity_table.entities[entity_id].static_buffer.strtable_row;
	// 	}; break;
	// 	case BUFFER_READER: {
	// 		strtable_rowid = self->entity_table.entities[entity_id].reader_buffer.strtable_row;
	// 	}; break;
	// 	case BUFFER_CHILD: {
	// 		fprintf(stderr, "Error: invalid state, cannot render child buffer directly\n");
	// 		abort();
	// 	}; break;
	// 	#ifdef DEBUG
	// 	default: abort();
	// 	#endif
	// };
	assert(strtable_rowid != SIZE_MAX);

	String *strings = self->string_table.strings_col[strtable_rowid];
	StringsData strings_data = self->string_table.data_col[strtable_rowid];

	if (strings_data.line_count == 0) {
		ssize_t format_result = IOBuffer_printf(buffer,
			MOVE_CURSOR_TO CSI COLOR_RED_FG "m<NO DATA>" CSI STYLE_AND_COLOR_RESET "m",
			offset_rows + 1,
			offset_cols + 1
		);
		#ifdef DEBUG
		assert(format_result > 0);
		#endif
	}

	size_t i_max = (strings_data.line_count - strings_data.top_line < render_rows)
		? strings_data.line_count - strings_data.top_line
		: render_rows
	;

	// TODO implement column clipping with escape counting
	// TODO implement IOBuffer escaped writing

	ssize_t format_result = 0;
	uint16_t line_number_max = digit_count(strings_data.line_count);
	for (size_t i = 0; i < i_max; i += 1) {
		format_result = IOBuffer_printf(
			buffer, MOVE_CURSOR_TO "%lu" MOVE_CURSOR_TO CSI COLOR_BLUE_FG "m|" CSI STYLE_AND_COLOR_RESET "m",
			offset_rows + i + 1,
			offset_cols + 1,
			strings_data.top_line + i,
			offset_rows + i + 1,
			offset_cols + line_number_max + 1
		);
		#ifdef DEBUG
		assert(format_result > 0);
		#endif

		IOBuffer_write_string(buffer, strings[i + strings_data.top_line]);

		#ifdef DEBUG
		assert(buffer->buffer_filled < buffer->buffer_size);
		#endif
	}

}

// if `current_buffer_id` is SIZE_MAX, then this
// function will only update background tasks
//
// returns 1 if the current buffer was updated
// returns 0 otherwise
uint8_t BufferTable_update(BufferTable *self, size_t current_buffer_id) {

	uint8_t current_buffer_updated = false;

	for (size_t i = 0; i < self->pollfd_table.fds_max; i += 1) {
		struct pollfd *poll_fd = &self->pollfd_table.fds[i];
		if (poll_fd->revents & POLLERR) {
			char buf;
			ssize_t read_size = read(poll_fd->fd, &buf, 1);
			assert(read_size == -1);
			fprintf(stderr, "WARN: error polling fd -> %s fd = %i\nNOTE: nullifying entry, but not closing fd\n", strerror(errno), poll_fd->fd);
			poll_fd->fd = -2;
		}
		else if (poll_fd->revents & POLLNVAL) {
			fprintf(stderr, "WARN: received POLLNVAL, fd = %i. continuing\n", poll_fd->fd);
			poll_fd->fd = -1;
		}
		else if (poll_fd->revents & POLLIN) {
			// assert(self->pollfd_table.fds[i].revents == POLLIN);

			size_t entity_id = self->pollfd_table.entity_ids[i];
			if (entity_id != SIZE_MAX) {
				ReaderBuffer *buffer = &self->entity_table.entities[entity_id].reader_buffer;
				uintptr_t old_buffer_address = (uintptr_t)NULL;
				ssize_t read_size = IOBuffer_read_from_realloc(&buffer->buffer, poll_fd->fd, &old_buffer_address);
				if (old_buffer_address != (uintptr_t)NULL) {
					intptr_t pointer_diff = (intptr_t)buffer->buffer.buffer - (intptr_t)old_buffer_address;
					String *strings = self->string_table.strings_col[buffer->strtable_row];
					StringsData *strings_data = &self->string_table.data_col[buffer->strtable_row];

					for (size_t i = 0; i < strings_data->line_count; i += 1) {
						// *strings[i] = (String){ .ptr = strings[i]->ptr + pointer_diff, .len = }
						strings[i] = (String){ .ptr = strings[i].ptr + pointer_diff, .len = strings[i].len };
					}
				}
				uint8_t first_bytes = buffer->buffer.buffer[0];
				if (read_size == -1) {
					fprintf(stderr, "Error: failed to perform read on reader buffer -> %s\n", strerror(errno));
					abort();
				}
				if (read_size == 0) {
					// set fd to not update after reaching EOF
					poll_fd->events = 0x0;
					poll_fd->revents = 0x0;
					continue;
				}

				String **strings = &self->string_table.strings_col[buffer->strtable_row];
				StringsData *strings_data = &self->string_table.data_col[buffer->strtable_row];
				String next_line = (String) { 0 };
				while (true) {
					next_line = get_next_line(buffer->buffer.buffer, &buffer->next_line_start, buffer->buffer.buffer_filled);
					if (next_line.ptr == NULL) { break; }

					if (strings_data->line_count >= strings_data->string_count) {
						strings_data->string_count *= 2;
						*strings = realloc(*strings, strings_data->string_count * sizeof(String));
						assert(strings != NULL);
					}

					(*strings)[strings_data->line_count] = next_line;
					strings_data->line_count += 1;
				}

				// // TODO add accessors to tables
				// self->string_table.strings_col[buffer->strtable_row] = strings;
				// self->string_table.data_col[buffer->strtable_row] = strings_data;

				// self->entity_table.entities[i].reader_buffer = *buffer;

				poll_fd->revents = 0x0;

				if (i == current_buffer_id) { current_buffer_updated = true; }
			}
		}else if (poll_fd->revents & POLLHUP) {
			close(poll_fd->fd);
			poll_fd->fd = -1;
			poll_fd->events = 0x0;
			poll_fd->revents = 0x0;
		}
	}

	if (current_buffer_id != SIZE_MAX) {
		switch (self->entity_table.entity_kinds[current_buffer_id]) {
			case BUFFER_INVALID: {
				fprintf(stderr, "Error: tried to update invalid (unused) buffer.\n");
				abort();
			};
			case BUFFER_STATIC: {}; break;
			case BUFFER_READER: {}; break; // work for this will have already been done
			case BUFFER_CHILD: {
				fprintf(stderr, "Error: unexpected child buffer as target for update. This is likely invalid\n");
				abort();
			}; break;
			#ifdef DEBUG
			default: abort();
			#endif
		}
	}

	return current_buffer_updated;
}

#ifdef TEST
uint8_t TEST_BufferTable_update() {
	{ // test 1
		BufferTable table = BufferTable_create();

		int file_ends[2];
		assert(socketpair(PF_LOCAL, SOCK_STREAM, AF_LOCAL, file_ends) != -1);

		size_t buffer_id = BufferTable_insert_ReaderBuffer(&table, (struct pollfd){ .fd = file_ends[1], .events = POLLIN });
		expect(buffer_id == 0, "Unexpected buffer id");
		expect(table.entity_table.entity_kinds[0] == BUFFER_READER, "Invalid buffer kind");
		(void)buffer_id;

		char test_string[] = "This test string should be read\nas two seperate lines\n";
		ssize_t write_size = write(file_ends[0], test_string, str_size(test_string));
		expect(write_size != -1, "Write failed");
		expect(write_size == str_size(test_string), "Wrote invalid number of bytes");
		
		int poll_result = poll(table.pollfd_table.fds, table.pollfd_table.fds_max, 100);
		expect(poll_result == 1, "Poll failed");

		BufferTable_update(&table, SIZE_MAX);

		ReaderBuffer buffer = table.entity_table.entities[0].reader_buffer;
		expect(buffer.strtable_row == 0, "Unexpected stringtable row id");
		expect(buffer.next_line_start == str_size(test_string), "Unexpected next line start");
		expect(buffer.buffer.buffer_filled == str_size(test_string), "Expected buffer filled to equal input size");
		expect(memcmp(buffer.buffer.buffer, test_string, str_size(test_string)) == 0, "Buffer contents did not match reference");

		String *strings = table.string_table.strings_col[0];
		StringsData strings_data = table.string_table.data_col[0];
		expect(strings_data.string_count == 64, "Unexpected string count");
		expect(strings_data.line_count == 2, "Unexpected line count");

		expect(strings[0].len == str_size("This test string should be read"), "Unexpected string len");
		expect(
			memcmp(strings[0].ptr, "This test string should be read", strings[0].len) == 0,
			"First string contents did not match reference"
		);
		expect(strings[1].len == str_size("as two seperate lines"), "Unexpected string len");
		expect(
			memcmp(strings[1].ptr, "as two seperate lines", strings[1].len) == 0,
			"First string contents did not match reference"
		);

		BufferTable_destroy(&table);
	}
	// TODO make this test more robust and cover more possible cases

	return 0;
}
#endif

static inline size_t BufferTable_get_strtable_id(BufferTable *self, size_t buffer_id) {
	switch (self->entity_table.entity_kinds[buffer_id]) {
		case BUFFER_INVALID: abort();
		case BUFFER_STATIC: {
			return self->entity_table.entities[buffer_id].static_buffer.strtable_row;
		};
		case BUFFER_READER: {
			return self->entity_table.entities[buffer_id].reader_buffer.strtable_row;
		};
		case BUFFER_CHILD: {
			fprintf(stderr, "Error: invalid state, unexpected child buffer as active buffer");
			abort();
		};
	}
	abort();
}

static inline size_t BufferTable_get_next_id(
	BufferTable *self, size_t current_id
) {
	for (size_t i = current_id + 1; i < self->entity_table.entities_max; i += 1) {
		if (
			self->entity_table.entity_kinds[i] != BUFFER_INVALID &&
			self->entity_table.entity_kinds[i] != BUFFER_CHILD
		) { return i; }
	}
	for (size_t i = 0; i < current_id; i += 1) {
		if (
			self->entity_table.entity_kinds[i] != BUFFER_INVALID &&
			self->entity_table.entity_kinds[i] != BUFFER_CHILD
		) { return i; }
	}
	return current_id;
}

#ifdef TEST
uint8_t TEST_BufferTable_get_next_id() {
	{ // test 1
		BufferTable table = BufferTable_create();
		String test_strings_1[] = {
			String_of("test one"),
			String_of("test two")
		};
		String test_strings_2[] = {
			String_of("second test first"),
			String_of("second test second")
		};
		size_t first_id = BufferTable_insert_StaticBuffer(&table, test_strings_1, array_size(test_strings_1));
		size_t second_id = BufferTable_insert_StaticBuffer(&table, test_strings_2, array_size(test_strings_2));

		size_t expected_id = BufferTable_get_next_id(&table, first_id);
		expect(expected_id == second_id, "Unexpected ID");
		size_t strtable_row = BufferTable_get_strtable_id(&table, expected_id);
		String *strings = table.string_table.strings_col[strtable_row];
		StringsData strings_data = table.string_table.data_col[strtable_row];
		expect(strings != NULL, "Failed to set strings row value");
		expect(strings_data.string_count == 2, "Unexpected string count");
		expect(strings_data.line_count == 2, "Unexpected line count");
		expect(strings[0].len == test_strings_2[0].len, "Unexpected string length");
		expect(memcmp(strings[0].ptr, test_strings_2[0].ptr, test_strings_1[0].len) == 0, "Unexpected string contents");
		expect(strings[1].len == test_strings_2[1].len, "Unexpected string length");
		expect(memcmp(strings[1].ptr, test_strings_2[1].ptr, test_strings_1[1].len) == 0, "Unexpected string contents");

		expected_id = BufferTable_get_next_id(&table, second_id);
		expect(expected_id == first_id, "Unexpected ID");
		strtable_row = BufferTable_get_strtable_id(&table, expected_id);
		strings = table.string_table.strings_col[strtable_row];
		strings_data = table.string_table.data_col[strtable_row];
		expect(strings != NULL, "Failed to set strings row value");
		expect(strings_data.string_count == 2, "Unexpected string count");
		expect(strings_data.line_count == 2, "Unexpected line count");
		expect(strings[0].len == test_strings_1[0].len, "Unexpected string length");
		expect(memcmp(strings[0].ptr, test_strings_1[0].ptr, test_strings_1[0].len) == 0, "Unexpected string contents");
		expect(strings[1].len == test_strings_1[1].len, "Unexpected string length");
		expect(memcmp(strings[1].ptr, test_strings_1[1].ptr, test_strings_1[1].len) == 0, "Unexpected string contents");

		BufferTable_destroy(&table);
	}

	{ // test 2
		BufferTable table = BufferTable_create();

		String test_strings_1[] = {
			String_of("test 1"),
			String_of("test 2"),
			String_of("test 3"),
		};

		int file_ends[2];
		assert(socketpair(PF_LOCAL, SOCK_STREAM, AF_LOCAL, file_ends) != -1);

		size_t starting_id = BufferTable_insert_StaticBuffer(&table, test_strings_1, array_size(test_strings_1));
		size_t second_id = BufferTable_insert_ReaderBuffer(&table, (struct pollfd){ .fd = file_ends[1], .events = POLLIN });

		for (size_t i = 0; i < 7; i += 1) {
			BufferTable_insert_StaticBuffer(&table, test_strings_1, array_size(test_strings_1));
		}

		expect(table.entity_table.entities_max == 16, "Unexpected entity count");
		size_t next_id = BufferTable_get_next_id(&table, starting_id);
		expect(next_id == second_id, "Unexpected next id");
		size_t looparound_id = BufferTable_get_next_id(&table, 8);
		expect(table.entity_table.entity_kinds[8] == BUFFER_STATIC, "Expected nineth entity to exist");
		expect(looparound_id == 0, "Unexpected looparound id");
		expect(looparound_id == starting_id, "Unexpected looparound or starting id");

		// ssize_t write_size = write(file_ends[0], test_strings_1[0].ptr, test_strings_1[0].len);
		// expect((size_t)write_size == test_strings_1[0].len, "Unexpected write size");
		// BufferTable_update(&table, SIZE_MAX);
		BufferTable_destroy(&table);
	}

	return 0;
}
#endif

static inline size_t BufferTable_get_prev_id(BufferTable *self, size_t current_id) {
	for (size_t i = current_id; i > 0; i -= 1) {
		if (
			self->entity_table.entity_kinds[i - 1] != BUFFER_INVALID &&
			self->entity_table.entity_kinds[i - 1] != BUFFER_CHILD
		) { return i - 1; }
	}
	for (size_t i = self->entity_table.entities_max - 1; i > current_id; i -= 1) {
		if (
			self->entity_table.entity_kinds[i] != BUFFER_INVALID &&
			self->entity_table.entity_kinds[i] != BUFFER_CHILD
		) { return i; }
	}
	return current_id;
}

#ifdef TEST
uint8_t TEST_BufferTable_get_prev_id() {
	BufferTable table = BufferTable_create();

	String test_strings[] = {
		String_of("test string 1"),
		String_of("test string 2")
	};

	size_t base_id = BufferTable_insert_StaticBuffer(&table, test_strings, array_size(test_strings));
	expect(base_id == 0, "Unexpected first entity id");

	size_t second_id = BufferTable_insert_StaticBuffer(&table, test_strings, array_size(test_strings));
	expect(second_id == 1, "Unexpected second id");

	size_t wraparound_id = BufferTable_get_prev_id(&table, base_id);
	expect(wraparound_id == second_id, "Unexpected wraparound id");

	BufferTable_destroy(&table);

	return 0;
}
#endif


typedef enum {
	INPUT_UNDEFINED = 0,
	INPUT_UP,
	INPUT_DOWN,
	INPUT_PAGE_UP,
	INPUT_PAGE_DOWN,
	INPUT_QUIT,
	INPUT_BUFFER_NEXT,
	INPUT_BUFFER_PREV,
	INPUT_INVALID, // this value, and any value greater are invalid (for testing)
} InputAction;

// const size_t KEYMAPS_MAX = 64;
#ifndef KEYMAPS_MAX
#define KEYMAPS_MAX 64
#endif
typedef struct {
	uint32_t keyvalues[KEYMAPS_MAX];
	InputAction actions[KEYMAPS_MAX];
} InputMap;

static inline InputAction get_action_of(uint32_t keypress, InputMap *map) {
	for (size_t i = 0; i < KEYMAPS_MAX; i += 1) {
		if (map->keyvalues[i] == keypress) {
			return map->actions[i];
		}
	}
	return INPUT_UNDEFINED;
}

#ifdef TEST
uint8_t TEST_get_action_of() {
	{ // test 1
		InputMap map = {
			.keyvalues = { 0x00002faa, 0x00000055, 0x000000ff },
			.actions = { INPUT_BUFFER_NEXT, INPUT_DOWN, INPUT_QUIT, INPUT_UP }
		};

		expect(get_action_of(0x2faa, &map) == INPUT_BUFFER_NEXT, "Unexpected input action");
		expect(get_action_of(0xff, &map) == INPUT_QUIT, "Unexpected input action");
		expect(get_action_of(0x55, &map) == INPUT_DOWN, "Unexpected input action");
		expect(get_action_of(0x00, &map) == INPUT_UP, "Unexpected input action");
		expect(get_action_of(0x0100, &map) == INPUT_UNDEFINED, "Unexpected input action");
	}
	// TODO make more robust

	return 0;
}
#endif


static InputMap input_map =  {
	.keyvalues = {
		0x0000006a, // 'j'
		0x00425b1b, // Down Arrow
		0x0000006b, // 'k'
		0x00415b1b, // Up Arrow
		0x7e365b1b, // Page Down
		0x7e355b1b, // Page Up
		0x00000068, // 'h'
		0x0000006c, // 'l'
		0x00000071, // 'q'
		0x0000001b, // Escape
	},
	.actions = {
		INPUT_DOWN,
		INPUT_DOWN,
		INPUT_UP,
		INPUT_UP,
		INPUT_PAGE_DOWN,
		INPUT_PAGE_UP,
		INPUT_BUFFER_PREV,
		INPUT_BUFFER_NEXT,
		INPUT_QUIT,
		INPUT_QUIT,
	},
};

struct termios saved_termconfig;

void cleanup() {
	tcsetattr(STDOUT_FILENO, TCSAFLUSH, &saved_termconfig);
}

struct winsize window_size = { 0 };

void update_window_size(int _signum) {
	(void)_signum;
	// TODO investigate the safety of writing the window_size in an async way
	int ioctl_result = ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size);
	(void)ioctl_result;
}

String help_strings[] = {
	String_of("Pager - by Aiden Kring <aidenjkring@gmail.com>"),
	String_of("______________________________________________"),
	String_of(""),
	String_of("Navigation"),
	String_of("----------"),
	String_of("Up          'k' or ↑"),
	String_of("Down        'j' or ↓"),
	String_of("Page Up     <PgUp>"),
	String_of("Page Down   <PgDn>"),
	String_of("Buffer Next 'l'"),
	String_of("Buffer Prev 'h'"),
	String_of("Quit        'q' or <Escape>"),
	String_of(""),
	String_of("Invokation"),
	String_of("----------"),
	String_of("-h or --help        Opens this buffer"),
	String_of("-s or --spawn <cmd> Page over the outputs of a command"),
	String_of("<filename>          Open `filename` in a buffer")
};

const size_t help_strings_count = sizeof(help_strings) / sizeof(help_strings[0]);


int testing_main();
void print_keycodes();
int main(int argc, char **argv) {
	// TODO IOBuffer_write_realloc function
	// TODO user-defined key bindings

	// Call testing main when compiled for tests
	#ifdef TEST
	char *failure_abort_value = getenv("PAGER_TEST_ABORT");
	if (failure_abort_value != NULL) {
		// abort_on_test_failure = !(strcmp(failure_abort_value, "true") == 0);
		if (strcmp(failure_abort_value, "true") == 0) {
			abort_on_test_failure = true;
		}
	}
	return testing_main();
	#endif

	BufferTable buffer_table = BufferTable_create();

	uint8_t get_keycodes = false;
	{ // parse command line arguments
		assert(argc > 0);
		for (size_t i = 1; i < (size_t)argc; i += 1) {
			if(memcmp(argv[i], "--get-keycodes", str_size("--get-keycodes")) == 0) {
				get_keycodes = true;
			}else if (
				memcmp(argv[i], "-h", str_size("-h")) == 0 ||
				memcmp(argv[i], "--help", str_size("--help")) == 0
			) {
				BufferTable_insert_StaticBuffer(&buffer_table, help_strings, help_strings_count);
			}else if (
				memcmp(argv[i], "-s", str_size("-s")) == 0 ||
				memcmp(argv[i], "--spawn", str_size("--spawn")) == 0
			) {
				i += 1;
				#ifdef DEBUG
				assert(i <= (size_t)argc);
				#endif
				if (i == (size_t)argv) {
					fprintf(stderr, "Error: expected \"spawn\" flag to be followed by a command argument\n");
					return EXIT_FAILURE;
				}
				char *error_string = NULL;
				size_t spawn_result = BufferTable_insert_ChildBuffer(&buffer_table, argv[i], &error_string);
				if (spawn_result == SIZE_MAX) {
					assert(error_string != NULL);
					fprintf(stderr, "Error: failed to spawn subprocess -> %s | %s\n", strerror(errno), error_string);
					return EXIT_FAILURE;
				}
			}else {
				int file_result = open(argv[i], O_RDONLY);
				if (file_result == -1) {
					fprintf(stderr, "Failed attempt to open file \"%s\", %s\n", argv[i], strerror(errno));
					return EXIT_FAILURE;
				}
				// TODO add file window
				BufferTable_insert_ReaderBuffer(&buffer_table, (struct pollfd){ .fd = file_result, .events = POLLIN });
			}
		}
	}

	if (!isatty(STDOUT_FILENO)) {
		char *tty_name = NULL;
		if (isatty(STDIN_FILENO)) {
			tty_name = ttyname(STDIN_FILENO);
		}else if (isatty(STDERR_FILENO)) {
			tty_name = ttyname(STDERR_FILENO);
		}else {
			fprintf(stderr, "Error: unable to find controlling terminal\n");
			abort();
		}
		assert(tty_name != NULL);

		int new_stdout_fd = open(tty_name, O_WRONLY);
		assert(new_stdout_fd != -1);

		fprintf(stdout, "WARN: redirecting stdout to controlling tty\n");
		fflush(stdout);

		dup2(new_stdout_fd, STDOUT_FILENO);
	}

	if (!isatty(STDIN_FILENO)) {
		char *tty_name = NULL;
		if (isatty(STDOUT_FILENO)) {
			tty_name = ttyname(STDOUT_FILENO);
		}else if (isatty(STDERR_FILENO)) {
			tty_name = ttyname(STDERR_FILENO);
		}else {
			fprintf(stderr, "Error: invalid state, a tty should be available\n");
			abort();
		}
		assert(tty_name != NULL);

		int new_stdin_fd = open(tty_name, O_RDONLY);
		assert(new_stdin_fd != -1);

		int old_stdin_pipe = dup(STDIN_FILENO);
		dup2(new_stdin_fd, STDIN_FILENO);

		BufferTable_insert_ReaderBuffer(&buffer_table, (struct pollfd){ .fd = old_stdin_pipe, .events = POLLIN });
	}

	size_t buffer_id = BufferTable_get_next_id(&buffer_table, buffer_table.entity_table.entities_max - 1);
	if (buffer_table.entity_table.entity_kinds[buffer_id] == BUFFER_INVALID) {
		fprintf(stderr, "No data to page over. exiting\n");
		BufferTable_destroy(&buffer_table);
		return EXIT_SUCCESS;
	}

	tcgetattr(STDOUT_FILENO, &saved_termconfig);
	struct termios new_termconfig = saved_termconfig;
	new_termconfig.c_lflag &= ~(ICANON | ECHO);
	new_termconfig.c_cc[VTIME] = 0;
	new_termconfig.c_cc[VMIN] = 1;
	tcsetattr(STDOUT_FILENO, TCSAFLUSH, &new_termconfig);
	atexit(cleanup);

	if (get_keycodes) {
		print_keycodes();
		return EXIT_SUCCESS;
	}

	update_window_size(0);
	signal(SIGWINCH, update_window_size);

	IOBuffer out_buf = IOBuffer_create(40 * 120 * 100);

	size_t stdin_pollfd_id = PollfdTable_insert(
		&buffer_table.pollfd_table,
		(struct pollfd){ .fd = STDIN_FILENO, .events = POLLIN },
		SIZE_MAX
	);

	// size_t _buffer_id = BufferTable_insert_StaticBuffer(&buffer_table, help_strings, help_strings_count);
	// (void)_buffer_id;

	// int open_file = open("./src/main.c", O_RDONLY);
	// assert(open_file != -1);

	// size_t buffer_id = BufferTable_insert_ReaderBuffer(
	// 	&buffer_table, (struct pollfd) { .fd = open_file, .events = POLLIN }
	// );
	uint8_t needs_redraw = true;
	while (true) {
		int poll_result = poll(buffer_table.pollfd_table.fds, buffer_table.pollfd_table.fds_max, 100);
		if (poll_result == 0 && !needs_redraw) {
			// if (needs_redraw) { goto DRAW_SCREEN; }
			#define NANOS 1
			#define MICROS 1000
			#define MILLIS 1000 * 1000
			const struct timespec sleep_time = {
				.tv_nsec = 10 * MICROS,
				.tv_sec = 0
			};
			int sleep_result = nanosleep(&sleep_time, NULL);
			assert(sleep_result != -1);
			continue;
		}

		// uint8_t needs_redraw = false;
		uint16_t render_height = window_size.ws_row - 2;
		uint16_t render_width = window_size.ws_col - 2;

		if (buffer_table.pollfd_table.fds[stdin_pollfd_id].revents != 0) {
			assert(buffer_table.pollfd_table.fds[stdin_pollfd_id].revents == POLLIN);

			uint32_t input_keypress = 0;
			ssize_t read_size = read(STDIN_FILENO, (void *)&input_keypress, 4);
			assert(read_size != -1);
			assert(read_size != 0);

			InputAction action = get_action_of(input_keypress, &input_map);
			assert(action < INPUT_INVALID);

			switch (action) {
				case INPUT_UNDEFINED: break;
				case INPUT_UP: {
					size_t strtable_row = BufferTable_get_strtable_id(&buffer_table, buffer_id);
					StringsData *strings_data = &buffer_table.string_table.data_col[strtable_row];
					if (strings_data->top_line != 0) {
						strings_data->top_line -= 1;
					}
				}; break;
				case INPUT_DOWN: {
					size_t strtable_row = BufferTable_get_strtable_id(&buffer_table, buffer_id);
					StringsData *strings_data = &buffer_table.string_table.data_col[strtable_row];
					strings_data->top_line += 1;
					if (strings_data->top_line >= strings_data->line_count) {
						if (strings_data->line_count == 0) {
							strings_data->top_line = 0;
							// fprintf(stderr, "Unexpected action on empty buffer\n");
							// abort();
						}else {
							strings_data->top_line = strings_data->line_count - 1;
						}
					}
				}; break;
				case INPUT_BUFFER_NEXT: {
					buffer_id = BufferTable_get_next_id(&buffer_table, buffer_id);
				}; break;
				case INPUT_BUFFER_PREV: {
					buffer_id = BufferTable_get_prev_id(&buffer_table, buffer_id);
				}; break;
				case INPUT_PAGE_UP: {
					size_t strtable_row = BufferTable_get_strtable_id(&buffer_table, buffer_id);
					StringsData *strings_data = &buffer_table.string_table.data_col[strtable_row];
					if ((ssize_t)strings_data->top_line - (ssize_t)render_height > 0) {
						strings_data->top_line -= render_height;
					}else {
						strings_data->top_line = 0;
					}
				}; break;
				case INPUT_PAGE_DOWN: {
					size_t strtable_row = BufferTable_get_strtable_id(&buffer_table, buffer_id);
					StringsData *strings_data = &buffer_table.string_table.data_col[strtable_row];
					strings_data->top_line += render_height;
					if (strings_data->top_line > strings_data->line_count) {
						if (strings_data->line_count == 0) {
							strings_data->top_line = 0;
							// fprintf(stderr, "Unexpected action on empty buffer\n");
							// abort();
						}else {
							strings_data->top_line = strings_data->line_count - 1;
						}
					}
				}; break;
				case INPUT_QUIT: {
					goto APP_QUIT;
				}; break;
				case INPUT_INVALID: abort();
			}
			if (action != INPUT_UNDEFINED) { needs_redraw = true; }

			buffer_table.pollfd_table.fds[0].revents = 0x0;
		}

		needs_redraw |= BufferTable_update(&buffer_table, buffer_id);
		if (!needs_redraw) { continue; } // don't re-render if the current buffer wasn't updated

		IOBuffer_write(&out_buf, ERASE_SCREEN, sizeof(ERASE_SCREEN));
		BufferTable_display(
			&buffer_table, &out_buf, buffer_id,
			1, 1,
			render_width, render_height
		);

		IOBuffer_flush_to(&out_buf, STDOUT_FILENO);

		needs_redraw = false;

		// pid_t pid = getpid();
		// fprintf(stdout, "\npid -> %i", pid);
		// fflush(stdout);
	}

	APP_QUIT: {};

	BufferTable_destroy(&buffer_table);
	IOBuffer_destroy(&out_buf);

	return 0;
}

void print_keycodes() {
	if (!isatty(STDIN_FILENO)) {
		fprintf(stderr, "Error: expected stdin to be a terminal\n");
		abort();
	}
	struct pollfd stdin_pollfd = { .fd = STDIN_FILENO, .events = POLLIN };
	while(true) {
		int poll_result = poll(&stdin_pollfd, 1, -1);
		assert(poll_result == 1);

		uint32_t keycode_buffer;
		ssize_t read_size = read(STDIN_FILENO, (void *)&keycode_buffer, 4);
		assert(read_size != -1);
		assert(read_size != 0);

		fprintf(stdout, "Keypress Registered (%u bytes) %x\n", (uint8_t)read_size, keycode_buffer);
	}
}

#ifdef TEST
int testing_main() {
	uint16_t failed_tests = 0;

	#define test(test_name) { \
		if (test_name()) { \
			fprintf(stderr, \
				CSI COLOR_RED_FG "m" "Failed %s" CSI STYLE_AND_COLOR_RESET "m" " line %lu.\n%s\n", \
				failure_function, failure_line, failure_message \
			); \
			failed_tests += 1; \
			failure_function = NULL; \
			failure_function = NULL; \
			failure_line = ~0; \
		} \
	}

	test(TEST_digit_count);
	test(TEST_String_cmp);
	test(TEST_get_next_line);
	test(TEST_get_action_of);

	test(TEST_IOBuffer_write);
	test(TEST_IOBuffer_flush_to);
	test(TEST_IOBuffer_write_all);
	test(TEST_IOBuffer_printf);
	test(TEST_IOBuffer_read_from);
	test(TEST_IOBuffer_read_from_realloc);
	
	test(TEST_StringTable_create);
	test(TEST_StringTable_destroy);
	test(TEST_StringTable_insert);

	test(TEST_BufferEntityTable_create);
	test(TEST_BufferEntityTable_destroy);
	test(TEST_BufferEntityTable_insert);

	test(TEST_PollfdTable_create);
	test(TEST_PollfdTable_destroy);
	test(TEST_PollfdTable_insert);

	test(TEST_BufferTable_create);
	test(TEST_BufferTable_destroy);
	test(TEST_BufferTable_insert_StaticBuffer);
	test(TEST_BufferTable_insert_ReaderBuffer);
	test(TEST_BufferTable_update);
	test(TEST_BufferTable_get_next_id);
	test(TEST_BufferTable_get_prev_id);

	if (failed_tests) {
		fprintf(stderr,
			CSI COLOR_RED_FG "m" "Failed %u tests\n" CSI STYLE_AND_COLOR_RESET "m",
			failed_tests
		);
		return EXIT_FAILURE;
	}
	fprintf(stderr,
		CSI COLOR_GREEN_FG "m" "All tests passed.\n" CSI STYLE_AND_COLOR_RESET "m"
	);
	return EXIT_SUCCESS;

}
#endif

