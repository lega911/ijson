
import requests

while True:
    request = requests.post('http://127.0.0.1:8001/rpc/add', json={'params': '/test/command'}).json()
    response = {
        'id': request['id'],
        'result': request['params'] + ' world!'
    }
    requests.post('http://127.0.0.1:8001/rpc/result', json=response)
