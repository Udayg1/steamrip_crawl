game-dl: main.c dep.c
	@echo "Building..."
	@gcc -O3 -o game-dl main.c dep.c -l curl -Wall -Werror
