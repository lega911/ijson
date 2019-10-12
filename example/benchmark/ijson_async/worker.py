
from requests import Session, Counter

s = Session()
counter = Counter()
while True:
    request = s.post('http://localhost:8001/rpc/add', json={'name': '/sum'})
    assert request['a'] + request['b'] == 13
    counter.inc()
