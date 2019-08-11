
import requests

s = requests.Session()
data = {'name': 'msg/hello'}  # add command 'msg/hello' on first request
while True:
    task = s.post('http://127.0.0.1:8001/rpc/worker', json=data).json()
    data = {'result': 'Hello ' + task['name'] + '!'}
