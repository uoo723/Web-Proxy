#!/bin/bash
BASEDIR=$(dirname $0)

$BASEDIR/../test.sh "$BASEDIR/test_http_common.c \
$BASEDIR/../../src/http/http_common.c"
