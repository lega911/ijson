
from requests import Session

s = Session()
while True:
    request = s.post('http://localhost:8001/rpc/add', json={'name': '/sum', 'option': 'no_id'})
    s.post('http://localhost:8001/rpc/result', json={'result': request['a'] + request['b']})
