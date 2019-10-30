
import requests

r = requests.post('http://127.0.0.1:8001/msg/hello', json={'name': 'ubuntu'})
print(r.json())
