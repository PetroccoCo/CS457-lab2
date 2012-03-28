OBJECTS = awget.o ss.o
OUTPUT = awget ss
FLAGS = -ggdb -Wall
all: $(OBJECTS)
	g++ ${FLAGS} ss.o -o ss
	g++ ${FLAGS} awget.o -o awget
ss: ss.o
	g++ ss.o -o ss
awget: awget.o
	g++ awget.o -o awget
awget.o: awget.h awget.c
	g++ ${FLAGS} -c awget.c
ss.o: awget.h ss.c
	g++ ${FLAGS} -c ss.c
clean:
	rm -rf *.o $(OUTPUT)