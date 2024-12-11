

run: pager
	./pager test.txt bigtest.txt 2> dbg.txt

pager: src/main.c src/interface.c
	$(CC) -g src/interface.c src/main.c -Wall -Wpedantic -o pager

debug: pager
	valgrind -s --track-origins=yes --leak-check=full --show-leak-kinds=all ./pager bigtest.txt test.txt


