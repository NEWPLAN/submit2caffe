all:test
	 
test: main.cpp threadpool.hpp
	g++ -std=c++11 $^ -o $@  -lopencv_core -lopencv_highgui -lopencv_imgproc -lboost_system -lboost_thread
clean:
	rm -rf test
