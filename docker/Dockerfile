FROM alpine:3.9
RUN apk update && apk add --no-cache libstdc++ libuuid
COPY ijson /ijson
COPY ijson.debug /ijson.debug
ENTRYPOINT ["/ijson"]
CMD ["--host", "0.0.0.0"]
