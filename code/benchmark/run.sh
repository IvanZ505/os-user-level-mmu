#!/bin/bash
cd ../ && make clean && make allfrag && cd benchmark && make clean && make testfrag
./mtest
exit