
import sys
from requests import Session

queue = sys.argv[1] if len(sys.argv) > 1 else '/sum'
s = Session()
while True:
    s.post(f'http://localhost:8001' + queue, json={'a': 5, 'b': 8}, headers={'Type': 'pub'})
