rm -f *.o main
nasm -f elf64 -o print.o print.asm
gcc -c -std=gnu11 -o allocr.o allocr.c
gcc -std=gnu11 -o main main.c allocr.o print.o
