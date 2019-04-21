.PHONY: debug release info build

info:
	echo debug release
debug:
	g++ *.cpp -luuid -std=c++17 -DDEBUG -o jip.debug
release:
	g++ *.cpp -luuid -std=c++17 -O2 -o jip
build: debug release
