
import requests

s = requests.Session()
task = s.post('http://127.0.0.1:8001/rpc/worker', json={'params': {'name': '/test/command', 'info': 'Test command'}}).json()
while True:
    task = s.post('http://127.0.0.1:8001/rpc/worker', json={'result': task['params'] + ' world!'}).json()
