
Documentation
#############

- `Definitions`_
- Quickstart

  - `Example with curl`_
  - `Python example`_
  - `Simple worker on bash`_
  - `Worker with keep-alive`_
  - `Example of worker-mode`_

- Compile and start Inverted Json

  - Docker
  - Download release
  - Compile by itself

- `Type of requests`_

  - Sync call
  - Async call
  - Publish (pub/sub)
  - Worker
  - Worker + keep-alive
  - Worker mode
  - Patterns/*
  - Send to certain worker
  - Priority

- Other

  - rpc/help
  - rpc/details

- `FAQ`_

.. _Definitions:
Definitions
***********
- **Server** - (Inverted Json) it's a proxy/multiplexer, it's a server which accept connections from clients and workers to make direct connection among them. Difference from reversed proxy like nginx/haproxy, is a reversed proxy connects to worker by itself, when with Inverted Json workers connect to the server.
- **Client** - it's a process that send tasks to a server.
- **Queue** - all tasks is located to some named queue by topics.
- **Sync Call** - a client makes request and waits for response
- **Async Call** - a client makes request without waiting for a response, so the response is not needed or can be taken another way.


.. _Example with curl:
Example with curl
*****************

1. Start Inverted Json
::
  docker run -i -p 8001:8001 lega911/ijson

2. A worker waits a task from queue "**calc/sum**"
::
  curl localhost:8001/calc/sum -H 'type: get'

3. A client makes a sync call "**calc/sum**", a task is created in a corresponding queue
::
  curl localhost:8001/calc/sum -d '{"id": 15, "params": "5 + 8"}'

4. The worker receives the task ``{"id": 15, "params": "5 + 8"}``, now we can send a result
::
  curl localhost:8001 -H 'type: result' -d '{"id": 15, "result": 13}'

5. The client receives result: ``{"id": 15, "result": 13}``


.. _Python example:
Python example
**************

Client

.. code-block:: python

  import requests
  r = requests.post('http://127.0.0.1:8001/test/command', json={'id': 1, 'params': 'Hello'})
  print(r.json())

Worker

.. code-block:: python

  import requests
  while True:
      # send request to get a task (type = 'get')
      req = requests.post('http://127.0.0.1:8001/test/command', headers={'type': 'get'}).json()
      response = {
          'id': req['id'],
          'result': req['params'] + ' world!'
      }
      # send result (type = 'result')
      requests.post('http://127.0.0.1:8001/', json=response, headers={'type': 'result'})


.. _Simple worker on bash:
Simple worker on bash
*********************
Worker

.. code-block:: bash

  while true
  do
      sleep 1
      task=$(curl -s localhost:8001/run/command -H 'type: get')
      status="$?"
      if [ $status -ne 0 ]; then
              echo "Server error"
              sleep 9
              continue;
      fi
      if [ "$task" == "start" ]; then
          echo START
      fi
      if [ "$task" == "stop" ]; then
          echo STOP
      fi
  done

Client
::
  curl localhost:8001/run/command -H 'type: async' -d 'start'


.. _Worker with keep-alive:
Worker with keep-alive
**********************
For keep-alive you need to set header `type='get+'` for worker, in this case ``id`` for a call is not required, because a cient is linked to a worker directly 

Worker

.. code-block:: python

  import requests
  with requests.Session() as s:
      while True:
          # send request to get a task (type = 'get+')
          req = s.post('http://127.0.0.1:8001/test/command', headers={'type': 'get+'}).json()
          response = {'result': req['params'] + ' world!'}
          # send result (type = 'result')
          s.post('http://127.0.0.1:8001', json=response, headers={'type': 'result'})

Client

.. code-block:: python

  import requests
  r = requests.post('http://127.0.0.1:8001/test/command', json={'params': 'Hello'})
  print(r.json())


.. _Example of worker-mode:
Example of worker-mode
**********************
Worker mode is the most performant way, due to less number of io, when sending result and get next task are united in the same request.

Worker

.. code-block:: python

  import requests
  s = requests.Session()
  # send request to get first task with worker-mode (type = 'worker')
  req = s.post('http://127.0.0.1:8001/msg/hello', headers={'type': 'worker'}).json()
  while True:
      result = {'result': 'Hello ' + req['name'] + '!'}
      # send result, and receive next task
      req = s.post('http://127.0.0.1:8001', json=result).json()

Client

.. code-block:: python

  import requests
  r = requests.post('http://127.0.0.1:8001/msg/hello', json={'name': 'ubuntu'})
  print(r.json())


.. _Type of requests:
Type of requests
****************

**Sync call** a client sends a command and receives a response, example:
::
  curl localhost:8001/some/command -d 'some-data'

**Async call** ``type=async``, a client sends a command without await a result, example:
::
  curl localhost:8001/some/command -d 'some-data' -H 'type: async'

**Publish (pub/sub)** ``type=pub``, a client sends a message without await result, all connected workers receive the message, example:
::
  curl localhost:8001/some/command -d 'some-data' -H 'type: pub'

**Worker** ``type=get``, a worker sends a request to get a task, example:
::
  curl localhost:8001/some/command -H 'type: get'

**Worker + keep-alive** ``type=get+``, 
::
  curl localhost:8001/some/command -H 'type: get+'

**Worker mode**
::
  curl localhost:8001/some/command -H 'type: worker'

**Result**
::
  curl localhost:8001/ -H 'type: result'
  curl localhost:8001/<id> -H 'type: result'

**Patterns/\***
::
  curl localhost:8001/section/* -H 'type: get'

**Send to certain worker**
::
  curl localhost:8001/some/command -H 'worker-id: 15'
and worker can set id for its connection
::
  curl localhost:8001/some/command -H 'set-id: 15'

**Priority**
::
  curl localhost:8001/some/command -H 'type: get' -H 'priority: 15'
priority by default is 0, positive number is a higher priority, negative number is a lower priority

.. _FAQ:
FAQ
***

Threads, how and what's for?
  On small servers Inverted Json can reach maximum performace with 1 thread (default mode), but if you find that Inverted Json reaches 100% usage of one core, then it make sence to start Inverted Json in multithread mode, you can define number of threads with option `--threads`

If you have any question or proposal you can contact me lega911@gmail.com or via `telegram <https://t.me/olegnechaev/>`_.
