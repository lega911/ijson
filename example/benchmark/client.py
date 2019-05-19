
import sys
from requests import Session
import time

port = 8001
if len(sys.argv) >= 2:
    port = sys.argv[1]

start = time.time()
counter = Session()
s = Session()
prev = 0
for i in range(0, 10**10):
    response = s.post(f'http://localhost:{port}/sum', json={'a': 5, 'b': 8})
    assert response['result'] == 13

    now = time.time()
    if now - start > 0.2:
        counter.post('http://localhost:7000/', data=str(i - prev).encode('utf8'))
        start = now
        prev = i
