SOURCES=$(wildcard *.cpp)
HEADERS=$(SOURCES:.cpp=.h)
FLAGS=-DDEBUG -g -std=c++17 -lpthread

all: main

main: $(SOURCES) $(HEADERS)
	mpic++ $(SOURCES) $(FLAGS) -o main

clear: clean

clean:
	rm main a.out

run: main
	mpirun -hostfile hosty.txt -np 8 ./main
