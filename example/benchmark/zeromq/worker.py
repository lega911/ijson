
import zmq
import ujson


context = zmq.Context(1)
server = context.socket(zmq.REP)
server.bind("tcp://*:5555")

while True:
    request = ujson.loads(server.recv())
    response = {'result': request['a'] + request['b']}
    server.send(ujson.dumps(response).encode('utf8'))

server.close()
context.term()
