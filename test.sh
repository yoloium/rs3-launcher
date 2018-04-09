#!/bin/bash

hexn() { 
        var=$(printf "%x\n" "$1") 
        echo "\\x0${var:0:1}\\x${var:1:3}" 
} 

hexs() { 
        var=$(echo "$1" | xxd -p) 
        echo "${var::-2}00" | sed -e 's|..|\\x&|g' 
}

title="RuneScape"
game_folder="$HOME/Jagex"
min_w=765
min_h=540
max_w=3840
max_h=2160
width=1366
height=768
fifo=$$

hexn $min_width
hexn $min_height
hexn $max_width
hexn $max_height
hexn $width
hexn $height

echo -n -e '\0\1\0\4\0\0\0\0\0\0\0\0'

echo -n -e '\0\4\0\0\7'
echo "\n"
hexs $title
hexs $game_folder

pkt_len="$((${#game_folder} * 2 + ${#title} + 32))"
echo "packet lenth: $pkt_len"

packet="\0\x01\0\x04\0\0\0\0\0\0\0\0$(hexs $game_folder)$(hexs $game_folder)\0\x04\0\0\x07$(hexs $title)$(hexn $min_w)$(hexn $min_h)$(hexn $max_w)$(hexn $max_h)$(hexn $width)$(hexn $height)"

echo -n -e $packet > packet
