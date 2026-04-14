# freverse

`freverse` is an x86-64 assembly program that reverses the contents of a file.  

## Usage
- Works on files of any size (including >4 GiB).  
- Files shorter than 2 bytes remain unchanged.  
- No terminal output; exits with code **1** on error.  

## Build
```bash
nasm -f elf64 -w+all -w+error -o freverse.o freverse.asm
ld --fatal-warnings -o freverse freverse.o
```

