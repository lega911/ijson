
from requests import Counter
import zerorpc


c = zerorpc.Client()
c.connect("tcp://127.0.0.1:4242")

counter = Counter()
while True:
    response = c.sum({'a': 5, 'b': 3})
    assert response['result'] == 8
    counter.inc()
