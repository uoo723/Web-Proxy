#!/bin/bash
BASEDIR=$(dirname $0)

$BASEDIR/../test.sh "$BASEDIR/test_http_log.c \
$BASEDIR/../../src/http/http_log.c \
$BASEDIR/../../src/http/http_common.c \
$BASEDIR/../../src/http/http_request.c \
$BASEDIR/../../src/http/http_response.c \
$BASEDIR/../../src/http/http_parser.c \
-lpthread"
