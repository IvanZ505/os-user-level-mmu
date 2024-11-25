#!/bin/bash
# cd ../ && make clean && make allfrag && cd benchmark && make clean && make testfrag
# cd ../ && make clean && make all64 && cd benchmark && make clean && make test64
cd ../ && make clean && make && cd benchmark && make clean && make
./mtest
exit