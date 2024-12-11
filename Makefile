

run: pager
	./pager --spawn ls 2> dbg.txt

pager: src/main.c src/interface.c
	$(CC) -g src/interface.c src/main.c -Wall -Wpedantic -o pager

debug: pager
	valgrind -s --track-origins=yes --leak-check=full --show-leak-kinds=all ./pager --spawn find\ . 2> dbg.txt


