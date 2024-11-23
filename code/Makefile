CC = gcc
CFLAGS = -g -c -m32
AR = ar -rc
RANLIB = ranlib

all: my_vm.a

all64: my_vm64.a

allfrag: my_vm_frag.a

my_vm.a: my_vm.o
	$(AR) libmy_vm.a my_vm.o
	$(RANLIB) libmy_vm.a

my_vm.o: my_vm.h

	$(CC) $(CFLAGS)  my_vm.c

my_vm64.a: my_vm64.o
	$(AR) libmy_vm64.a my_vm64.o
	$(RANLIB) libmy_vm64.a

my_vm64.o: my_vm64.h

	$(CC)	-g -c  my_vm64.c

my_vm_frag.a: my_vm_frag.o
	$(AR) libmy_vm_frag.a my_vm_frag.o
	$(RANLIB) libmy_vm_frag.a

my_vm_frag.o: my_vm_frag.h

	$(CC) $(CFLAGS)  my_vm_frag.c


clean:
	rm -rf *.o *.a
