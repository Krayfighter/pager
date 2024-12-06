

run: pager
	./pager test.txt bigtest.txt 2> dbg.txt

pager: src/main.c src/interface.c
	$(CC) -g src/interface.c src/main.c -o pager


