#!/bin/bash
BASEDIR=$(dirname $0)

$BASEDIR/../test.sh "$BASEDIR/test_lru.c \
$BASEDIR/../../src/cache/lru.c \
-lpthread"
