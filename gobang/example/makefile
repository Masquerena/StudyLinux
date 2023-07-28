# json:json.cpp
# 	g++ json.cpp -o json -std=c++11 -ljsoncpp

wsserver:wsserver.cpp
	g++ wsserver.cpp -o wsserver -std=c++11 -pthread -lboost_system
.PHONY:clean
clean:
	rm -f wsserver