#!/bin/bash
BASEDIR=$(dirname $0)

$BASEDIR/../test.sh "$BASEDIR/test_http_response.c \
$BASEDIR/../../src/http/http_common.c \
$BASEDIR/../../src/http/http_response.c \
$BASEDIR/../../src/http/http_parser.c"
