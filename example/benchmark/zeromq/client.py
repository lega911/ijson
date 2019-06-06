
from requests import Counter
import zmq
import ujson

context = zmq.Context()
socket = context.socket(zmq.REQ)
socket.connect("tcp://localhost:5555")


def call(request):
    socket.send(ujson.dumps(request).encode('utf8'))
    return ujson.loads(socket.recv())

counter = Counter()
while True:
    r = call({'a': 5, 'b': 8})
    assert r['result'] == 13
    counter.inc()
