.PHONY: debug release info build

info:
	echo debug release
debug:
	g++ src/*.cpp -luuid -std=c++17 -DDEBUG -rdynamic -o ijson.debug
release:
	g++ src/*.cpp -luuid -std=c++17 -O2 -o ijson
build: debug release
alpine:
	cd /cpp
	g++ src/*.cpp -luuid -std=c++17 -O2 -DALPINE -o /cpp/docker/ijson
	g++ src/*.cpp -luuid -std=c++17 -DDEBUG -DALPINE -rdynamic -o /cpp/docker/ijson.debug
