.PHONY: debug release info build

info:
	echo debug release
debug:
	g++ *.cpp -luuid -std=c++17 -DDEBUG -o ijson.debug
release:
	g++ *.cpp -luuid -std=c++17 -O2 -o ijson
build: debug release
