### Inverted Json
Inverted Json is a job server which helps you to organize **RPC, MQ, PubSub** communication between clients and workers. It helps to save time and resources.
* It's **very fast**, it's built with C/C++ and epoll, it's **7+ time faster** than RabbitMQ for RPC ([look at benchmark](#benchmark)).
* It's **supported by all languages/frameworks**, because it works via http.
* It **uses much less of memory** (and CPU), less than 50+ time than RabbitMQ ([memory usage](files/mem9.png)).
* Docker image is just **2.6Mb** (slim version)
* API is easy and compact (look at examples, quickstart will be soon)
* It's **a single point of access**: [Client] -> [Inverted Json] <- [Worker] (clients and workers connect to Inverted Json), to simplify the configuration for projects.
* Supported platforms: Linux, FreeBSD, MacOS, Docker.

#### Benchmark
<a id="benchmark"></a>
![Performance](files/performance9.png)

<sup>[Memory usage](files/mem9.png), [CPU usage](files/cpu9.png), [Multi-core result](files/performance9mc.png)</sup>

#### Try Inverted Json in 5 min
![Example](files/example.gif)

<sup>
Here we:<br/>
1. Start Inverted Json<br/>
2. A worker publishes a command<br/>
3. A client invokes the command<br/>
4. The worker responds to the client<br/>
</sup>

[read more, an article](https://medium.com/@lega911/rpc-benchmark-and-inverted-json-b5ce0bf587be)


#### Start ijson
``` bash
docker run -it -p 8001:8001 lega911/ijson
```

#### Example with curl (client + worker)
``` bash
# 1. a worker requests for a command (task) "test/command"
curl localhost:8001/test/command -H 'type: get'

# 2. a client invokes the command
curl localhost:8001/test/command -d '{"id": 123, "params": "test data"}'

# the worker receives {"id": 123, "params": "test data"}
# 3. and sends response with the same id
curl localhost:8001 -H 'type: result' -d '{"id": 123, "result": "data received"}'

# client receives {"id": 123, "result": "data received"}
```

#### Python client
``` python
response = requests.post('http://127.0.0.1:8001/test/command', json={'id': 1, 'params': 'Hello'})
print(response.json())
```

#### Python worker
``` python
while True:
    # get a request
    request = requests.post('http://127.0.0.1:8001/test/command', headers={'Type': 'get'}).json()
    
    # send a response
    response = {
        'id': request['id'],
        'result': request['params'] + ' world!'
    }
    requests.post('http://127.0.0.1:8001/', json=response, headers={'Type': 'result'})
```
