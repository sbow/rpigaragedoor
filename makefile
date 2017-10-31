
all: output_file_name

output_file_name: main.o
	gcc main.o -lbcm2835 -o output_file_name -lpthread -lwiringPi

main.o: main.c
	gcc -c main.c

clean:
	rm -rf *o output_file_name

