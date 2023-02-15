run:
	gcc -I lib ./lib/sigsegv.c ./lib/stackvma.c ./stackoverflow.c -o stackoverflow
	./stackoverflow
