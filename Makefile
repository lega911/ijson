.PHONY: debug release info build clean test docker

VERSION = $(shell cat ./version)

info:
	@echo debug release test
debug:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -DDEBUG -rdynamic -o ijson.debug
release:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -O2 -o ijson
release_freebsd:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -O2 -D_KQUEUE -o ijson
release_macos:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -O2 -D_KQUEUE -D_NOBALANCER -o ijson
build: debug release
clean:
	rm -f ijson ijson.debug
docker:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -DDOCKER -O2 -o docker/ijson
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -DDOCKER -DDEBUG -rdynamic -o docker/ijson.debug
docker_slim:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -DDOCKER -O2 -o docker-slim/ijson -static
test:
	cd tests; pytest -v -s main.py
