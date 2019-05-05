### ijson - inverted json (inverted jsonrpc proxy)

ijson helps to make RPC communication via http where both clients and workers are http-clients: [client] -> [ijson] <- [worker]
* It's fast - it uses c++ and epoll
* JsonRPC2 partly supported
* Able to send binary data (not only json)
* Multimethods
* Able to send command on certain worker
* Using Keep-Alive to detect if worker is ok
* Docker image ~9Mb (proxy itself is ~100kb)

### Start ijson
```
docker run -i -p 8001:8001 lega911/ijson
```

### Example with curl
``` bash
# 1. a worker publishes rpc command
curl -d '{"params": "/test/command"}' http://localhost:8001/rpc/add

# 2. a client invokes the command
curl -d '{"id": 123, "params": "test data"}' http://localhost:8001/test/command

# the worker receives {"id": 123, "params": "test data"}
# 3. and sends response with the same id
curl -d '{"id": 123, "result": "data received"}' http://localhost:8001/rpc/result

# client receives {"id": 123, "result": "data received"}
```

### Python example
``` python
import requests

# client
response = requests.post('http://127.0.0.1:8001/test/command', json={'id': 1, 'params': 'Hello'})
print(response.json())

# worker
while True:
    request = session.post('http://127.0.0.1:8001/rpc/add', json={'params': '/test/command'}).json()
    
    response = {'id': request['id'], 'result': request['params'] + ' world!'}
    session.post('http://127.0.0.1:8001/rpc/result', json=response)
```
