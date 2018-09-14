CC = gcc
C++ =g++

CFLAGS = -O3 -W -Wall
CPPFLAGES = -std=c++11 $(CREF)
LDFLAGS = -lglane -pthread -s -lmyhdparm

all: hugepage glane myhdparam
	@echo $^

hugepage:hugepage.c
	gcc $< -o $@ -Wall $(CPPFLAGES)

glane:glane.c glane_library.h
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LIBS)

myhdparam: my-hdparm.cpp hdparm.h
	g++ $< -o myhdparam $(LDFLAGS) 
.PHONY:clean
clean:
	rm -rf hugepage glane myhdparam

run_myhdparam:myhdparam
	sudo ./myhdparam /home/yang/project/imagenet/lmdb/ilsvrc12/train.txt /mnt/dc_p3700/imagenet/train/