
from bottle import post, run, request
import threading
import time


count = 0

@post('/')
def index():
    global count
    count += int(request.body.read())
    return b''


def show():
    prev = 0
    while True:
        start = time.time()
        time.sleep(1)
        now = time.time()
        dur = now - start
        print(int((count - prev) / dur), 'ops')
        start = now
        prev = count

threading.Thread(target=show).start()
run(host='localhost', port=7000, quiet=True)
