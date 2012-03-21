OBJECTS = awget.o ss.o
OUTPUT = awget ss
all: $(OBJECTS)
	g++ ss.o -o ss
	g++ awget.o -o awget
ss: ss.o
	g++ ss.o -o ss
awget: awget.o
	g++ awget.o -o awget
awget.o: awget.h awget.c
	g++ -c awget.c
ss.o: awget.h ss.c
	g++ -c ss.c
clean:
	rm -rf *.o $(OUTPUT)