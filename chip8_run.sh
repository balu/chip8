gcc -o chip8 chip8.c
setterm -cursor off -background black -foreground green
stty raw -echo
./chip8 $1 $2
setterm -cursor on -background default -foreground default
stty sane echo
clear