FROM alpine:3.9
RUN apk update && apk add --no-cache libstdc++ libuuid
COPY ijson /ijson
ENTRYPOINT ["/ijson"]
CMD ["--log", "8", "--host", "0.0.0.0"]
