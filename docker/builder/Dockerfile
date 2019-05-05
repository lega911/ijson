FROM alpine:3.9

RUN apk update && apk add --no-cache binutils cmake make libgcc musl-dev gcc g++ util-linux-dev

WORKDIR /cpp
CMD g++ *.cpp -luuid -std=c++17 -O2 -o ijson.alpine
