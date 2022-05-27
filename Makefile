all: ./out/destination ./out/source

./out/destination : ./src/destination.c ./src/utils.c
	gcc ./src/destination.c -o ./out/destination

./out/source : ./src/source.c ./src/utils.c
	gcc ./src/source.c -o ./out/source
