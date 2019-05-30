.PHONY: info make_builder build_app build push

info:
	echo debug release
make_builder:
	docker build -t ijson_builder ./builder
build_app:
	docker run -it --rm -v `pwd`/..:/cpp -w /cpp ijson_builder make build
	mv -f ../ijson ./
	mv -f ../ijson.debug ./
build: build_app
	docker build -t lega911/ijson .
push:
	docker push lega911/ijson
