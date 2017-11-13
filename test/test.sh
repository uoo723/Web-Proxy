#!/bin/bash
if [ $# -eq 0 ]; then
	echo "Usage $0 [target] [LD wrap...]"
	exit 1
fi

BASEDIR=$(dirname $0)
target=$1
ld=""

shift

for i in $@; do
	ld="$ld -W1,--wrap=$i"
done

gcc -g -I$BASEDIR/../src -L$BASEDIR/../libs $ld $target \
$(pkg-config --cflags --libs cmocka) \
&& ./a.out && rm a.out
