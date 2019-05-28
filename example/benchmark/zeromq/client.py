
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
for i in range(0, 10**10):
    r = call({'a': 5, 'b': 8})
    assert r['result'] == 13
    counter.set(i)
