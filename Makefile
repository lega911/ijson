.PHONY: debug release info build clean test docker

info:
	@echo debug release test
debug:
	g++ src/*.cpp -pthread -std=c++17 -DDEBUG -rdynamic -o ijson.debug
release:
	g++ src/*.cpp -pthread -std=c++17 -O2 -o ijson
build: debug release
clean:
	rm -f ijson ijson.debug
docker:
	g++ src/*.cpp -pthread -std=c++17 -DDOCKER -O2 -o docker/ijson
	g++ src/*.cpp -pthread -std=c++17 -DDOCKER -DDEBUG -rdynamic -o docker/ijson.debug
docker_slim:
	g++ src/*.cpp -pthread -std=c++17 -DDOCKER -O2 -o docker-slim/ijson -static
test:
	cd tests; pytest37 -v -s main.py
