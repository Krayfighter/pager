

run: pager
	./pager --spawn "cd .. && ls -R" 2> dbg.txt

test: pager
	./pager --spawn "cd /home/aiden/code/flark && make"

pager: src/main.c src/interface.c
	$(CC) -pg src/interface.c src/main.c -Iplustypes -Wall -Wpedantic -o pager

release: src
	$(CC) src/interface.c src/main.c -O3 -Iplustypes -o pager

vg: pager
	valgrind -s --track-origins=yes --leak-check=full --show-leak-kinds=all ./pager --spawn find\ . 2> dbg.txt

cg: pager
	valgrind 


