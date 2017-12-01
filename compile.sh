sh clean.sh
nasm -f elf64 -o print.o print.asm
gcc -c -m64 -fPIC -std=gnu11 -o allocr.o allocr.c
gcc -c -m64 -fPIC -std=gnu11 -o print.o.1 print.c
gcc -m64 -shared -o lib/libmdl-allocr.so allocr.o print.o print.o.1
cp allocr.h inc/mdl
gcc -std=gnu11 -Iinc -Llib -o main main.c -lmdl-allocr
