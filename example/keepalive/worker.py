
import requests

with requests.Session() as session:
    while True:
        request = session.post('http://127.0.0.1:8001/rpc/add', json={'params': {'name': '/test/command', 'id': False}}).json()

        session.post('http://127.0.0.1:8001/rpc/result', json={
            'result': request['params'] + ' world!'
        })
