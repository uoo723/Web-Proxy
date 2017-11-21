#!/bin/bash
BASEDIR=$(dirname $0)

for each in $BASEDIR/**/*.sh
do
	$each
done
