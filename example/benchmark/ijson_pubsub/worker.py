
import sys
from requests import Session, Counter

queue = sys.argv[1] if len(sys.argv) > 1 else '/sum'
s = Session()
counter = Counter()
request = s.get('http://localhost:8001' + queue, headers={'Type': 'worker'})
while True:
    assert request['a'] + request['b'] == 13
    counter.inc()
    request = s.get('http://localhost:8001/')
