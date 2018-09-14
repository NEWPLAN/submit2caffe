CC = gcc
C++ =g++

CFLAGS = -O3 -W -Wall
CPPFLAGES = -std=c++11 $(CFLAGS)
LDFLAGS = -lglane -pthread -s -lmyhdparm

all: hugepage glane test
	@echo $^

hugepage:hugepage.cpp
	gcc $< -o $@ -Wall $(CPPFLAGES)

glane:glane.cpp glane_library.h
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LIBS)

myhdparam: my-hdparm.cpp hdparm.h
	g++ $(CPPFLAGES) $< -o myhdparam $(LDFLAGS)  


.PHONY:clean
clean:
	rm -rf hugepage glane myhdparam

CFILE = main.cpp my-hdparm.cpp
HEAD = hdparm.h
test:$(CFILE) 
	g++ $(CPPFLAGES) $^ -o test $(LDFLAGS)  


run_myhdparam:test
	sudo ./test /home/yang/project/imagenet/lmdb/ilsvrc12/train.txt /mnt/dc_p3700/imagenet/train/