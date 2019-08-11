
from requests import Session

url = 'http://localhost:8001/rpc/worker'
s = Session()
task = s.post(url, json={'name': '/sum'})
while True:
    response = {'result': task['a'] + task['b']}
    task = s.post(url, json=response)
