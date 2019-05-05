
import requests

with requests.Session() as session:
    while True:
        request = session.post('http://127.0.0.1:8001/rpc/add', json={'params': '/test/command'}).json()
        response ={
            'id': request['id'],
            'result': request['params'] + ' world!'
        }
        session.post('http://127.0.0.1:8001/rpc/result', json=response)
