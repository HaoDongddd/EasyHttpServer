test:http.o test.o work.o log.o 
	g++ http.o test.o work.o log.o -g -o test -lpthread 
http.o:http.cc
	g++ -c http.cc -o http.o -std=c++11 -fPIC
test.o:test.cpp
	g++ -c test.cpp -o test.o -std=c++11  -fPIC
work.o:work.cpp
	g++ -c work.cpp -o work.o -std=c++11 -fPIC
log.o:../c_c++/log_system/log.cc
	g++ -c ../log_system/log.cc -o log.o -std=c++11 -fPIC -no-pie
clean:
	rm http.o test.o work.o log.o test
run:
	./test
gdb:
	g++ test.cpp http.cc work.cpp ../log_system/log.cc -g -o test -std=c++11 -lpthread -no-pie