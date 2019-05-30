#!/bin/sh
docker build -t py ./python
docker run -it --rm --network host -v `pwd`:/app py -c "taskset 0xfffc python3 /app/counter.py"
