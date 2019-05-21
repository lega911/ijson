
import time
from requests import Session
import zerorpc


start = time.time()
counter = Session()
s = Session()
prev = 0

c = zerorpc.Client()
c.connect("tcp://127.0.0.1:4242")

for i in range(10**10):
    response = c.sum({'a': 5, 'b': 3})
    assert response['result'] == 8

    now = time.time()
    if now - start > 0.2:
        counter.post('http://localhost:7000/', data=str(i - prev).encode('utf8'))
        start = now
        prev = i
