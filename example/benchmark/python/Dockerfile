FROM python:3.7

RUN pip3 install uwsgi ujson pycurl bottle uvloop httptools pika pyzmq zerorpc autobahn twisted cbor grpcio-tools asyncio-nats-client
COPY requests.py /pylib/requests.py
ENV PYTHONPATH /pylib
ENV LC_ALL=C
WORKDIR /app
ENTRYPOINT ["bash"]
