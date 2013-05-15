#!/bin/sh

# you should probably download a txt dictionary of your favourite language and use that as a words.txt

WORDSTOPLAY=10

sort -R words.txt | fgrep -v \" | head -n $WORDSTOPLAY | ./game

