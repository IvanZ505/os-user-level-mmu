#!/bin/bash
cd ../ && make clean && make && cd benchmark && make clean && make
./mtest
exit