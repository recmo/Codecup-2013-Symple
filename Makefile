

main: main.cpp
	g++ -DLOCAL -march=nocona -Wall -O2 -g main.cpp -lm -o main

ssetest: ssetest.cpp
	g++ -march=nocona -DLOCAL -Wall -O2 -g ssetest.cpp -lm -o ssetest

competition: main
	cd caia/symple/bin; ./caiaio -m competition





