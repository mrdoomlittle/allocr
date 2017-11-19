sh clean.sh
nasm -f elf64 -o print.o print.asm
gcc -c -m64 -fPIC -std=gnu11 -o allocr.o allocr.c
gcc -c -m64 -fPIC -std=gnu11 -o print.o.1 print.c
gcc -m64 -shared -o lib/liballocr.so allocr.o print.o print.o.1
gcc -std=gnu11 -o main main.c allocr.o print.o print.o.1
