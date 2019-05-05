.PHONY: debug release info build

info:
	echo debug release
debug:
	g++ src/*.cpp -luuid -std=c++17 -DDEBUG -o ijson.debug
release:
	g++ src/*.cpp -luuid -std=c++17 -O2 -o ijson
alpine:
	cd /cpp; g++ src/*.cpp -luuid -std=c++17 -O2 -o /cpp/docker/ijson
build: debug release
