.PHONY: info make_builder build_app build

info:
	echo debug release
make_builder:
	docker build -t json_builder ./builder
build_app:
	docker run -it --rm -v /home/lega/py/jsonrr/cpp1:/cpp -w /cpp json_builder make alpine
build: build_app
	docker build -t lega911/ijson .
push:
	docker push lega911/ijson
