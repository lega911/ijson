
import time
import threading
import requests


L = 'http://localhost:8001'
TIMEOUT = 5


def make_request(method):
    method = getattr(requests, method)
    def req(path, *a, type=None, headers=None, **kw):
        if 'timeout' not in kw:
            kw['timeout'] = TIMEOUT
        
        if type:
            if not headers:
                headers = {}
            headers['Type'] = type

        return method(L + path, *a, headers=headers, **kw)
    return req


post = make_request('post')
get = make_request('get')


def test_default():
    clients = [None, None]

    def _request(i):
        time.sleep(0.1 * i)
        clients[i - 1] = post('/test/cmd1', json={'id': i, 'params': 'ubuntu'})

    threading.Thread(target=_request, args=(1,)).start()
    threading.Thread(target=_request, args=(2,)).start()

    worker = get('/test/cmd1', type='get')
    assert worker.status_code == 200
    request = worker.json()
    assert request['id'] == 1
    assert request['params'] == 'ubuntu'
    assert post('/rpc/result', json={'id': 1, 'result': 'linux'}).status_code == 200
    time.sleep(0.1)

    assert clients[0].status_code == 200
    response = clients[0].json()
    assert response['id'] == 1
    assert response['result'] == 'linux'
    assert clients[1] is None

    worker = get('/test/cmd1', type='get')
    assert worker.status_code == 200
    request = worker.json()
    assert request['id'] == 2
    assert request['params'] == 'ubuntu'
    assert post('/rpc/result', json={'id': 2, 'result': 'unix'}).status_code == 200
    time.sleep(0.1)

    assert clients[1].status_code == 200
    response = clients[1].json()
    assert response['id'] == 2
    assert response['result'] == 'unix'


def test_request_without_id():
    def run_worker():
        r = get('/one', type='get')
        post('/' + r.headers['id'], json={'result': 'ok'}, type='result')
    
    threading.Thread(target=run_worker).start()
    time.sleep(0.1)
    r = post('/one', json={'params': 1})
    assert r.status_code == 200
    assert r.json()['result'] == 'ok'


def test_worker_mode():
    def worker():
        s = requests.Session()
        task = s.post(L + '/test/worker', json={'info': 'test for worker mode'}, headers={'Type': 'worker'}).json()
        while True:
            time.sleep(0.1)
            if task.get('stop'):
                break
            task = s.post(L, json={'result': sum(task['nums'])}).json()

    threading.Thread(target=worker).start()
    time.sleep(0.1)

    result = post('/test/worker', json={'nums': [1]}).json()
    assert result['result'] == 1

    result = post('/test/worker', json={'nums': [1, 3, 5]}).json()
    assert result['result'] == 9

    result = post('/test/worker', json={'nums': [7, 11, 13, 17]}).json()
    assert result['result'] == 48

    r = post('/test/worker', json={'stop': True})
    assert r.status_code == 503

    r = post('/rpc/details').json()
    assert r['test/worker']['info'] == 'test for worker mode'


def test_pattern():
    h_response = None

    def worker():
        nonlocal h_response
        s = requests.Session()
        r = s.get(L + '/pattern/*', headers={'Type': 'worker'})
        while True:
            name = r.headers['name'][8:]
            time.sleep(0.1)

            if not name:
                r = s.post(L, data=b'sum,mul,stop')
                continue
            
            if name == 'sum':
                task = r.json()
                response = {'result': sum(task['value'])}
            elif name == 'mul':
                task = r.json()
                response = {'result': task['value'][0] * task['value'][1]}
            elif name == 'stop':
                r = s.post(L, json={'result': 'ok'}, headers={'Option': 'stop'})
                break
            else:
                response = {'error': 'no method'}

            r = s.post(L, json=response)

        time.sleep(0.2)
        h_response = s.post('http://localhost:8001/task/revert', json={'id': 12345}).json()

    threading.Thread(target=worker).start()
    time.sleep(0.1)

    assert get('/pattern/').text == 'sum,mul,stop'

    r = post('/pattern/sum', json={'value': [1, 2, 3, 4]})
    assert r.json()['result'] == 10

    r = post('/pattern/mul', json={'value': [3, 5]})
    assert r.json()['result'] == 15

    r = post('/pattern/typo', json={'value': [3, 5]})
    assert r.json()['error'] == 'no method'

    r = post('/pattern/stop')
    assert r.status_code == 200
    assert r.json()['result'] == 'ok'

    error = None
    try:
        get('/pattern/', timeout=0.1)
    except Exception as e:
        error = e
    assert isinstance(error, requests.exceptions.ReadTimeout)

    task = get('/task/revert', type='get').json()
    assert task['id'] == 12345
    post('/12345', json={'result': 'ok'}, type='result')
    time.sleep(0.1)
    assert h_response['result'] == 'ok'


def test4():
    h_response = None

    def worker():
        nonlocal h_response
        s = requests.Session()
        s.get(L + '/test4/*', headers={'Type': 'worker'})
        s.post(L, data=b'sum,mul,stop')

    threading.Thread(target=worker).start()
    time.sleep(0.1)

    assert get('/test4/').text == 'sum,mul,stop'

    r = post('/test4/sum', json={'value': [1, 2, 3, 4]})
    assert r.status_code == 503


def run(delay):
    def wrapper(fn):
        def wr():
            if delay:
                time.sleep(delay)
            fn()
        th = threading.Thread(target=wr)
        th.start()
    return wrapper


def test6_priority():
    result = []

    @run(0)
    def worker():
        worker = requests.Session()
        task = worker.post(L + '/test6', headers={'Type': 'get+'}).json()
        worker.post(L, json={'result': task['request']}, headers={'X-Type': 'result'}, timeout=TIMEOUT)

    time.sleep(0.1)
    response = post('/test6', json={'request': 0}).json()
    result.append(response['result'])

    def request(delay, value, priority=0):
        @run(delay)
        def send():
            headers = {}
            if priority:
                headers['Priority'] = str(priority)
            response = post('/test6', json={'request': value}, headers=headers).json()
            result.append(response['result'])

    request(0.1, 1)
    request(0.2, 2, 5)
    request(0.3, 3, -5)
    request(0.4, 4, 3)
    request(0.5, 5, -3)
    request(0.6, 6, 1)
    request(0.7, 7, -1)
    request(0.8, 8)
    request(0.9, 9, 4)

    time.sleep(1.5)

    details = post('/rpc/details').json()
    assert details['test6']['clients'] == 9

    worker = requests.Session()
    for _ in range(9):
        task = worker.post(L + '/test6', headers={'Type': 'get+'}, timeout=TIMEOUT).json()
        worker.post(L, json={'result': task['request']}, headers={'Type': 'result'}, timeout=TIMEOUT)
    time.sleep(0.5)
    assert result == [0, 2, 9, 4, 6, 1, 8, 7, 5, 3]


def test7_async():
    is_async = False
    data = None

    @run(0)
    def first():
        nonlocal is_async, data
        r = post('/test7/async', headers={'Type': 'get'})
        is_async = r.headers.get('Async') == 'true'
        data = r.text
    
    time.sleep(0.1)

    r = post('/test7/async', headers={'Type': 'async'}, data='test7')
    assert r.status_code == 200
    time.sleep(0.1)

    assert is_async
    assert data == 'test7'

    details = post('/rpc/details').json()
    assert details['test7/async']['workers'] == 0
    assert details['test7/async']['clients'] == 0

    for i in range(10):
        r = post('/test7/async', headers={'Type': 'async'}, data='test_' + str(i))
        assert r.status_code == 200
        time.sleep(0.01)

    details = post('/rpc/details').json()
    assert details['test7/async']['workers'] == 0
    assert details['test7/async']['clients'] == 10

    for i in range(10):
        r = post('/test7/async', headers={'Type': 'get'})
        assert r.headers.get('Async') == 'true'
        assert r.text == 'test_' + str(i)

    time.sleep(0.2)

    details = post('/rpc/details').json()
    assert details['test7/async']['workers'] == 0
    assert details['test7/async']['clients'] == 0

    for i in range(10):
        time.sleep(i*0.02)

        @run(0)
        def worker():
            r = post('/test7/async', headers={'Type': 'get'})
            assert r.headers.get('Async') == 'true'
            assert r.text == 'test_' + str(i)

    time.sleep(0.1)

    details = post('/rpc/details').json()
    assert details['test7/async']['workers'] == 10
    assert details['test7/async']['clients'] == 0

    for i in range(10):
        r = post('/test7/async', headers={'Type': 'async'}, data='test_' + str(i))
        assert r.status_code == 200
        time.sleep(0.01)
    
    details = post('/rpc/details').json()
    assert details['test7/async']['workers'] == 0
    assert details['test7/async']['clients'] == 0


def test7_async_worker():
    result = 0
    count = 0
    check = None

    @run(0)
    def worker():
        nonlocal result, count, check
        s = requests.Session()
        r = s.get(L + '/test7/async2', headers={'Type': 'worker'})
        while True:
            assert r.status_code == 200
            if r.content == b'stop':
                r = s.post(L, headers={'Option': 'stop'})
                assert r.status_code == 200
                break

            #assert r.headers.get('Async') == 'true'
            result += r.json()['data']
            if result == 0:
                time.sleep(0.5)
                check = post('/rpc/details').json()
            count += 1
            r = s.get(L)
        result += 1000

    time.sleep(0.1)
    s = requests.Session()
    for i in range(15):
        if i == 7:
            time.sleep(1)
            check2 = post('/rpc/details').json()
        r = s.post(L + '/test7/async2', json={'data': i}, headers={'Type': 'async'})
        assert r.status_code == 200
    s.post(L + '/test7/async2', data=b'stop')

    time.sleep(0.2)
    check3 = post('/rpc/details').json()

    assert count == 15
    assert result == 1105

    assert check['test7/async2']['clients'] == 6
    assert check['test7/async2']['workers'] == 0

    assert check2['test7/async2']['clients'] == 0
    assert check2['test7/async2']['workers'] == 1

    assert check3['test7/async2']['clients'] == 0
    assert check3['test7/async2']['workers'] == 0
