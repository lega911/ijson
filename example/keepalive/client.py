
import requests

r = requests.post('http://127.0.0.1:8001/test/command', json={'params': 'Hello'})
print(r.json())
