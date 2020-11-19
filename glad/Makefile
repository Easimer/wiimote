all: glad.a

glad.o: glad.c glad.h khrplatform.h
	$(CC) $(CFLAGS) -o glad.o -c glad.c

glad.a: glad.o
	$(AR) rcs glad.a glad.o

clean:
	rm -f glad.a glad.o

.PHONY: clean
