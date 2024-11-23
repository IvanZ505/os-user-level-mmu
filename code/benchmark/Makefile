
all : test
all64 : test64
allfrag : testfrag
test: ../my_vm.h
	gcc test.c -L../ -lmy_vm -m32 -o test
	gcc multi_test.c -L../ -lmy_vm -m32 -o mtest -lpthread

test64: ../my_vm64.h
	gcc test.c -L../ -lmy_vm64 -o test
	gcc multi_test.c -L../ -lmy_vm64 -o mtest -lpthread

testfrag: ../my_vm_frag.h
	gcc test.c -L../ -lmy_vm_frag -m32 -o test
	gcc multi_test.c -L../ -lmy_vm_frag -m32 -o mtest -lpthread

clean:
	rm -rf test mtest
