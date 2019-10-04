.PHONY: info make_builder build_app build push

VERSION = $(shell cat ../version | grep -oP '^\d+\.\d+')
VERSION_FULL = $(shell cat ../version)

info:
	echo debug release
make_builder:
	docker build -t ijson_builder ./builder
build_app:
	@echo "\n === Build $(VERSION_FULL) ===\n"
	docker run -it --rm -v `pwd`/..:/cpp -w /cpp ijson_builder make docker
build: build_app
	docker pull debian:stretch-slim
	docker build -t lega911/ijson -t lega911/ijson:$(VERSION) -t lega911/ijson:$(VERSION_FULL) .
push:
	docker push lega911/ijson
	docker push lega911/ijson:$(VERSION)
	docker push lega911/ijson:$(VERSION_FULL)
