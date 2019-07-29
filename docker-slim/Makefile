.PHONY: build push

build_app:
	docker run -it --rm -v `pwd`/..:/cpp -w /cpp ijson_builder make docker_slim
build: build_app
	docker build -t lega911/ijson:slim .
push:
	docker push lega911/ijson:slim
