.PHONY: info make_builder build_app build push

info:
	echo debug release
make_builder:
	docker build -t ijson_builder_alpine ./builder
build_app:
	docker run -it --rm -v `pwd`/..:/cpp -w /cpp ijson_builder_alpine make alpine
build: build_app
	docker build -t lega911/ijson:alpine .
push:
	docker push lega911/ijson:alpine
