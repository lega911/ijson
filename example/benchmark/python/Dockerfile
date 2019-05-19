FROM python:3.7.3

RUN pip3 install uwsgi ujson pycurl bottle httptools pika
COPY requests.py /pylib/requests.py
ENV PYTHONPATH /pylib
WORKDIR /app
ENTRYPOINT ["bash"]
