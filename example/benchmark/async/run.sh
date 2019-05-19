#!/bin/sh
docker run -d --network host -v `pwd`/nginx.conf:/etc/nginx/nginx.conf:ro nginx:1.15
echo python3 app.py 5000
echo python3 app.py 5001
echo ...
docker run -it --network host -v `pwd`:/app py
