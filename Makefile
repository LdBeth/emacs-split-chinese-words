all: ejieba.so

libjieba:
	(cd cjieba; $(MAKE) libjieba.a; cd ..)

ejieba.o: ejieba.c
	gcc -Wall -c ejieba.c -I./cjieba -I.

ejieba.so: ejieba.o
	gcc -shared -o ejieba.so ejieba.o -L./cjieba -ljieba -lstdc++ -lm
