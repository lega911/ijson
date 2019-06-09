
## Docs by examples

A few statements:
* Inverted Json (iJson) never transforms body of http-request, so it just forwards it as is, from client to worker and vise-versa.
* You can send binary data (not only json)
* First slash is removed from method, so `/test/command` is the same as `test/command`
* Id is optional
* *for all python examples need to **import requests**.*

### Content
* [Start Inverted Json](#start-inverted-json)
* [Default mode](#default-mode-curl)
* [If client doesn't provide an id](#if-client-doesn-t-provide-an-id)
* [Using /rpc/call, JsonRPC2 example](#using-rpccall-jsonrpc2-example)
* [Send binary data, set method's name and id in headers](#send-binary-data-set-methods-name-and-id-in-headers)
* [Worker: mode "fail_on_disconnect"](#worker-mode-fail_on_disconnect)
* [Worker: keep-alive mode without id](index.md#worker-keep-alive-mode-without-id)


### Start Inverted Json
```bash
docker run -i -p 8001:8001 lega911/ijson
```
<hr/>


### Default mode (curl)
1. a worker publishes rpc command `/test/command`
```bash
curl -d '{"params": "/test/command"}' http://localhost:8001/rpc/add
```
2. a client invokes the command
```bash
curl -d '{"id": 123, "params": "test data"}' http://localhost:8001/test/command
```
3. the worker receives `{"id": 123, "params": "test data"}` and sends response with the same id
```bash
curl -d '{"id": 123, "result": "data received"}' http://localhost:8001/rpc/result
```
and client receives `{"id": 123, "result": "data received"}`


#### The same - python version
Client:
```python
response = requests.post('http://127.0.0.1:8001/test/command', json={'id': 123, 'params': 'test data'})
print(response.json())
```
Worker:
```python
while True:
  # get a request
  request = requests.post('http://127.0.0.1:8001/rpc/add', json={'params': '/test/command'}).json()

  # send a response
  response = {'id': request['id'], 'result': request['params'] + ' world!'}
  requests.post('http://127.0.0.1:8001/rpc/result', json=response)
```
<hr/>


### If client doesn't provide an id
in this case an id will be generated as uuid and set to headers.

1. a worker publishes rpc command `/test/command`
```bash
curl -v -d '{"params": "/test/command"}' http://localhost:8001/rpc/add
```
2. a client invokes the command
```bash
curl -d '{"params": "test data"}' http://localhost:8001/test/command
```
3. the worker receives `{"params": "test data"}` and id in headers, so sends response with id (id can be in headers or in body)
```bash
curl -H "id: db35eba6-89fb-11e9-af5b-0242ac110002" -d '{"result": "data received"}' http://localhost:8001/rpc/result
# id is taken from headers
```
and client receives `{"result": "data received"}`


#### the same - python version
Client:
```python
response = requests.post('http://127.0.0.1:8001/test/command', json={'params': 'test data'})
print(response.json())
```
Worker:
```python
while True:
  # get a request
  r = requests.post('http://127.0.0.1:8001/rpc/add', json={'params': '/test/command'})
  task_id = r.headers['id']  # get id from headers
  task = r.json()

  # send a response
  requests.post('http://127.0.0.1:8001/rpc/result',
  	json={'result': task['params'] + ' world!'},
  	headers={'id': task_id}  # send for this id
  )
```
<hr/>


### Using /rpc/call (JsonRPC2 example)
With /rpc/call, a real method is taken from body

1. a worker publishes rpc command `/test/command`
```bash
curl -d '{"jsonrpc": "2.0", "method": "rpc/add", "params": "test/command"}' http://localhost:8001/rpc/call
```
2. a client invokes the command
```bash
curl -d '{"jsonrpc": "2.0", "method": "test/command", "params": "test data", "id": 123}' http://localhost:8001/rpc/call
```
3. the worker receives `{"jsonrpc": "2.0", "method": "test/command", "params": "test data", "id": 123}`, so send response with the same id
```bash
curl -d '{"jsonrpc": "2.0", "method": "/rpc/result", "id": 123, "result": "data received"}' http://localhost:8001/rpc/call
```
and client receives `{"jsonrpc": "2.0", "method": "/rpc/result", "id": 123, "result": "data received"}`
<hr/>

### Send binary data, set method's name and id in headers
1. a worker publishes rpc command `/test/command`
```bash
curl -H 'Name: test/command' http://localhost:8001/rpc/add
```
2. a client invokes the command
```bash
curl -H "id: 123" -d 'BINARY' http://localhost:8001/test/command
```
3. the worker receives `BINARY`, so send response with the same id
```bash
curl -H "id: 123" -d 'OK' http://localhost:8001/rpc/result
```
and client receives `OK`
<hr/>


### Worker: mode "fail_on_disconnect"
it will be keep-alive connection, and client receives http-503 error immediately when worker fails (on disconnect)

Client:
```python
response = requests.post('http://127.0.0.1:8001/test/command', json={'id': 123, 'params': 'test data'})
print(response.json())
```
Worker:
```python
# make keep-alive session
with requests.Session() as session:
  while True:
    # set "fail_on_disconnect=true", so iJson will send http-503 on disconnect
    request = session.post('http://127.0.0.1:8001/rpc/add', json={'params': {'name': 'test/command', 'fail_on_disconnect': True}}).json()

    # send result
    session.post('http://127.0.0.1:8001/rpc/result', json={
      'id': request['id'],
      'result': request['params'] + ' world!'
    })
```
<hr/>


### Worker: keep-alive mode without id
it includes "fail_on_disconnect" mode and not requires id

Client:
```python
response = requests.post('http://127.0.0.1:8001/test/command', json={'params': 'test data'})
print(response.json())
```
Worker:
```python
# make keep-alive session
with requests.Session() as session:
  while True:
    # set "id=false", means work in keep-alive mode without sending id
    request = session.post('http://127.0.0.1:8001/rpc/add', json={'params': {'name': 'test/command', 'id': False}}).json()

    # send result
    session.post('http://127.0.0.1:8001/rpc/result', json={
      'result': request['params'] + ' world!'
    })
```
