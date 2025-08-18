
run: pager
	perf record "cat Makefile | ./pager src/main.c compile_flags.txt -s ls"

pager: src/*
	gcc src/main.c \
		-ggdb -O0 \
		-std=gnu23 -fsanitize=address\
		-Wall -Wextra -Wpedantic \
		-DDEBUG \
		-Itermcodes \
		-o pager

test: tests
	./tests

tests: src/*
	gcc src/main.c \
		-ggdb -O0 \
		-std=gnu23 \
		-Wall -Wextra -Wpedantic \
		-DTEST -DDEBUG\
		-Itermcodes \
		-o tests
		# -fsanitize=address \

release: src/*
	gcc src/main.c \
		-Ofast \
		-std=gnu23 \
		-Wall -Wextra -Wpedantic \
		-Itermcodes \
		-o pager

