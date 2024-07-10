#!/bin/sh

for i in $(seq 1 9);
do
	./test_app/fire
	sleep 2
done
