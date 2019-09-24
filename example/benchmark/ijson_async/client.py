
import sys
from requests import Session, Counter

port = 8001
if len(sys.argv) >= 2:
    port = sys.argv[1]

counter = Counter()
s = Session()
while True:
    s.post(f'http://localhost:{port}/sum', json={'a': 5, 'b': 8}, headers={'Option': 'async'})
    counter.inc()
