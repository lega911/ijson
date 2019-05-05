
import requests

print(requests.post('http://127.0.0.1:8001/test/command', json={'id': 1, 'params': 'Hello'}).json())
