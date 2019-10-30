
import requests

s = requests.Session()
req = s.post('http://127.0.0.1:8001/msg/hello', headers={'Type': 'worker'}).json()
while True:
    result = {'result': 'Hello ' + req['name'] + '!'}
    req = s.post('http://127.0.0.1:8001', json=result).json()
