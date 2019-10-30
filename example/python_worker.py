
import requests

while True:
    req = requests.post('http://127.0.0.1:8001/test/command', headers={'Type': 'get'}).json()
    response = {
        'id': req['id'],
        'result': req['params'] + ' world!'
    }
    requests.post('http://127.0.0.1:8001/', json=response, headers={'Type': 'result'})
