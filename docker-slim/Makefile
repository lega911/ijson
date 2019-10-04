.PHONY: build push

VERSION = $(shell cat ../version | grep -oP '^\d+\.\d+')
VERSION_FULL = $(shell cat ../version)

build_app:
	@echo "\n === Build $(VERSION_FULL) ===\n"
	docker run -it --rm -v `pwd`/..:/cpp -w /cpp ijson_builder make docker_slim
build: build_app
	docker build -t lega911/ijson:slim -t lega911/ijson:$(VERSION)-slim -t lega911/ijson:$(VERSION_FULL)-slim .
push:
	docker push lega911/ijson:slim
	docker push lega911/ijson:$(VERSION)-slim
	docker push lega911/ijson:$(VERSION_FULL)-slim
