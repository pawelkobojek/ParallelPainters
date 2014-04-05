N = 10
FLAGS = -lpthread -Wall -pedantic

compile: main.c 
	gcc ${FLAGS} -o main main.c
run: main
	./main ${N}

.PHONY: clean

clean:
	rm main
