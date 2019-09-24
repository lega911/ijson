
from requests import Session

s = Session()
while True:
    request = s.post('http://localhost:8001/rpc/add', json={'name': '/sum'})
    assert request['a'] + request['b'] == 13
