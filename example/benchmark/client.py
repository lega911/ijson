
import sys
from requests import Session, Counter

port = 8001
if len(sys.argv) >= 2:
    port = sys.argv[1]

counter = Counter()
s = Session()
for i in range(0, 10**10):
    response = s.post(f'http://localhost:{port}/sum', json={'a': 5, 'b': 8})
    assert response['result'] == 13
    counter.set(i)
