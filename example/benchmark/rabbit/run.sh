#!/bin/sh
docker run -it --rm --network host --entrypoint sh rabbitmq -c "taskset 0x1 docker-entrypoint.sh rabbitmq-server"
