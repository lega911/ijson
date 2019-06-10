.PHONY: debug release info build clean test

info:
	echo debug release
debug:
	g++ src/*.cpp -luuid -std=c++17 -DDEBUG -rdynamic -o ijson.debug
release:
	g++ src/*.cpp -luuid -std=c++17 -O2 -o ijson
build: debug release
clean:
	rm -f ijson ijson.debug
alpine:
	cd /cpp
	g++ src/*.cpp -luuid -std=c++17 -O2 -DALPINE -o /cpp/docker-alpine/ijson
	g++ src/*.cpp -luuid -std=c++17 -DDEBUG -DALPINE -rdynamic -o /cpp/docker-alpine/ijson.debug
test:
	cd tests; pytest37 main.py
