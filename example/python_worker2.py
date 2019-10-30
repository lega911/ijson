
import requests

s = requests.Session()
task = s.post('http://127.0.0.1:8001/test/command', headers={'Type': 'worker'}).json()
while True:
    task = s.post('http://127.0.0.1:8001', json={'result': task['params'] + ' world!'}).json()
