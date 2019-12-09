.PHONY: debug release build clean test docker

VERSION = $(shell cat ./version)

build: debug release
debug:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -DDEBUG -rdynamic -o ijson.debug
release:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -O2 -o ijson
release_freebsd:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -O2 -D_KQUEUE -o ijson
release_macos:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -O2 -D_KQUEUE -D_NOBALANCER -o ijson
clean:
	rm -f ijson ijson.debug
docker:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -DDOCKER -O2 -o docker/ijson
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -DDOCKER -DDEBUG -rdynamic -o docker/ijson.debug
docker_slim:
	g++ src/*.cpp -pthread -std=c++17 -D_VERSION='"$(VERSION)"' -DDOCKER -O2 -o docker-slim/ijson -static
test:
	cd tests; pytest -v -s main.py
	cd tests; pytest -v -s main4.py
make_builder_u16:
	docker build -t ijson_builder_u16 ./docker/builder_u16
build_u16:
	docker run -it --rm -v `pwd`:/cpp -w /cpp ijson_builder_u16 make build
