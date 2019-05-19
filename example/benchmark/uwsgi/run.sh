#!/bin/sh
docker run -d --network host -v `pwd`/nginx.conf:/etc/nginx/nginx.conf:ro nginx:1.15
echo uwsgi --plugin python3 --socket :5000 --wsgi-file app.py --processes=4 --disable-logging
docker run -it --network host -v `pwd`:/app py
