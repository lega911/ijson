
import requests

with requests.Session() as s:
    while True:
        req = s.post('http://127.0.0.1:8001/test/command', headers={'Type': 'get+'}).json()

        response = {'result': req['params'] + ' world!'}
        s.post('http://127.0.0.1:8001', json=response, headers={'Type': 'result'})
