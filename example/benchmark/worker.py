
from requests import Session

s = Session()
while True:
    #request = s.post('http://localhost:8001/rpc/add', json={'params': {'name': '/sum', 'id': False}})
    request = s.get('http://localhost:8001/rpc/add', headers={'Name': '/sum', 'Id': 'false'})

    s.post('http://localhost:8001/rpc/result', json={'result': request['a'] + request['b']})
