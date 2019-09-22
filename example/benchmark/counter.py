
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
    history = []
    hi = 0
    while True:
        start = time.time()
        time.sleep(1)
        now = time.time()
        dur = now - start
        ops = (count - prev) / dur
        history.append(ops)
        if len(history) > 20:
            history = history[-20:]
        avg = int(sum(history)/len(history))
        if avg > hi:
            hi = avg
        print('{} ops, (avg {}, max {})'.format(int(ops), avg, hi))
        start = now
        prev = count

threading.Thread(target=show).start()
run(host='localhost', port=7000, quiet=True)
