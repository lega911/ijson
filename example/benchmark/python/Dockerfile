FROM python:3.7.3

RUN pip3 install uwsgi ujson pycurl bottle uvloop httptools pika pyzmq zerorpc autobahn twisted cbor grpcio-tools
COPY requests.py /pylib/requests.py
ENV PYTHONPATH /pylib
WORKDIR /app
ENTRYPOINT ["bash"]
