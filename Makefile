

main: main.cpp
	g++ -Wall -O2 -g main.cpp -lm -o main

competition: main
	cd caia/symple/bin; ./caiaio -m competition
