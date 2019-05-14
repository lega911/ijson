
from requests import Session

s = Session()
while True:
    request = s.post('http://localhost:8001/rpc/add', json={'params': {'name': '/sum', 'id': False}})

    s.post('http://localhost:8001/rpc/result', json={'result': request['a'] + request['b']})
