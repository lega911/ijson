
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


def mem(links):
    if not isinstance(links, list):
        links = [links]

    def get_memory():
        return int(get('/debug').text.split(': ')[1])

    def inner(fn):
        def wrapper():
            for link in links:
                post(link, type='create')
                post(link, type='delete')
            time.sleep(0.6)
            mem = get_memory()
            fn()
            for link in links:
                post(link, type='delete')
            time.sleep(0.6)
            assert mem == get_memory()
        return wrapper
    return inner


@mem('/test/cmd1')
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


@mem('/one')
def test_request_without_id():
    def run_worker():
        r = get('/one', type='get')
        post('/' + r.headers['id'], json={'result': 'ok'}, type='result')
    
    threading.Thread(target=run_worker).start()
    time.sleep(0.1)
    r = post('/one', json={'params': 1})
    assert r.status_code == 200
    assert r.json()['result'] == 'ok'


@mem('/test/worker')
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


@mem(['/pattern/*', '/task/revert'])
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


@mem('/test4/*')
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


@mem('/test6')
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


@mem('/test7/async')
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


@mem('/test7/async2')
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


@mem('/test8')
def test8_worker_id():
    def go(sleep, name, id=None):
        @run(sleep)
        def worker():
            s = requests.Session()
            headers = {'Type': 'get+'}
            if id:
                headers['Set-ID'] = str(id)
            r = s.get(L + '/test8', headers=headers)
            s.post(L, json={'name': name}, headers={'Type': 'result'})

    go(0, 'linux')
    go(0.1, 'windows')
    go(0.2, 'freebsd', 2000000)
    go(0.3, 'macos')
    go(0.4, 'unix', 1000000)
    go(0.5, 'redhat')
    go(0.6, 'ubuntu')

    time.sleep(1)

    d = get('/rpc/details').json()['test8']
    workers = list(map(str, d['worker_ids']))

    assert get('/test8', headers={'Worker-ID': workers[3]}).json()['name'] == 'macos'
    assert get('/test8', headers={'Worker-ID': workers[1]}).json()['name'] == 'windows'
    assert get('/test8', headers={'Worker-ID': workers[5]}).json()['name'] == 'redhat'
    del workers[5]
    del workers[3]
    del workers[1]

    d = get('/rpc/details').json()['test8']
    assert workers == list(map(str, d['worker_ids']))

    assert get('/test8', headers={'Worker-ID': '1000000'}).json()['name'] == 'unix'
    assert get('/test8', headers={'Worker-ID': '2000000'}).json()['name'] == 'freebsd'

    assert len(get('/rpc/details').json()['test8']['worker_ids']) == 2

    assert get('/test8').json()['name'] == 'linux'
    assert get('/test8').json()['name'] == 'ubuntu'

    assert len(get('/rpc/details').json()['test8']['worker_ids']) == 0
    time.sleep(0.1)


@mem('/test8')
def test8_worker_id2():
    c0 = None
    w0 = None

    @run(0)
    def worker():
        nonlocal w0
        w0 = get('/test8', type='get')
        post('/' + w0.headers['id'], type='result', json={'result': 'w0'})

    @run(0.1)
    def client():
        nonlocal c0
        c0 = get('/test8', headers={'Worker-ID': '1000015'}, json={'data': 'c0'})

    time.sleep(0.5)
    assert c0 is None and w0 is None
    details = get('/rpc/details').json()['test8']
    assert details['workers'] == 1
    assert details['clients'] == 1

    c1 = get('/test8', json={'data': 'c1'})
    assert c1.json()['result'] == 'w0'
    assert w0.json()['data'] == 'c1'

    assert c0 is None
    details = get('/rpc/details').json()['test8']
    assert details['workers'] == 0
    assert details['clients'] == 1

    w1 = get('/test8', type='get', headers={'Set-ID': '1000015'})
    assert w1.json()['data'] == 'c0'
    post('/' + w1.headers['id'], type='result', json={'result': 'w1'})
    time.sleep(0.1)
    assert c0.json()['result'] == 'w1'


@mem('/test9/pub')
def test9_pubsub():
    error = 0
    stopped = 0
    r_sum = [0] * 6

    def go(type, index):
        @run(0)
        def worker_get():
            nonlocal error, stopped
            if type == 'get':
                get = requests.get
            else:
                s = requests.Session()
                get = s.get
            while True:
                r = get(L + '/test9/pub', headers={'Type': type})
                if not r.headers['Async']:
                    error += 1
                msg = r.json()
                if msg['data'] == 'stop':
                    break
                r_sum[index] += msg['data']
                time.sleep(0.1)
            stopped += 1
    
    go('get', 0)
    go('get', 1)
    go('get+', 2)
    go('get+', 3)
    go('worker', 4)
    go('worker', 5)

    s = requests.Session()
    def client(data):
        r = s.post(L + '/test9/pub', headers={'Type': 'pub'}, json=data)
        assert r.status_code == 200
    
    time.sleep(0.2)
    client({'data': 1})
    time.sleep(0.2)
    assert r_sum == [1, 1, 1, 1, 1, 1]

    client({'data': 2})
    time.sleep(0.2)
    assert r_sum == [3, 3, 3, 3, 3, 3]

    client({'data': 3})
    client({'data': 4})
    client({'data': 5})
    client({'data': 6})
    client({'data': 7})
    time.sleep(0.7)
    
    assert r_sum == [6, 6, 28, 28, 28, 28]

    client({'data': 'stop'})
    time.sleep(0.2)
    assert stopped == 6


@mem('/test10/*')
def test10():
    names = []

    @run(0)
    def worker():
        s = requests.Session()
        while True:
            r = s.post(L + '/test10/*', headers={'Type': 'get+'})
            name = r.headers['Name']
            if name == 'test10/exit':
                s.post(L, data='exit', headers={'Type': 'result'})
                break

            value = r.json()['value'] if r.content else None
            names.append((name, value))
            s.post(L, data='ok', headers={'Type': 'result'})
            if value == 3:
                time.sleep(0.5)
            elif value in (7, 8, 9):
                time.sleep(0.2)

    time.sleep(0.1)
    assert post('/test10/one', json={'value': 1}).text == 'ok'
    assert names[0] == ('test10/one', 1)

    time.sleep(0.1)
    assert post('/test10/two/system', json={'value': 2}).text == 'ok'
    assert names[1] == ('test10/two/system', 2)

    time.sleep(0.1)
    assert post('/test10/delay', json={'value': 3}).text == 'ok'
    assert names[2] == ('test10/delay', 3)

    assert post('/test10/check4', json={'value': 4}).text == 'ok'
    assert names[3] == ('test10/check4', 4)

    time.sleep(0.1)
    assert post('/test10/async5', json={'value': 5}, type='async').status_code == 200
    time.sleep(0.1)
    assert names[4] == ('test10/async5', 5)

    time.sleep(0.1)
    assert post('/test10/async6', json={'value': 6}, type='async').status_code == 200
    time.sleep(0.1)
    assert names[5] == ('test10/async6', 6)

    assert post('/test10/async7', json={'value': 7}, type='async').status_code == 200
    assert post('/test10/async8', json={'value': 8}, type='async').status_code == 200
    assert post('/test10/async9', json={'value': 9}, type='async').status_code == 200
    assert post('/test10/async10', json={'value': 10}, type='async').status_code == 200

    assert post('/test10/exit').text == 'exit'

    assert names[6] == ('test10/async7', 7)
    assert names[7] == ('test10/async8', 8)
    assert names[8] == ('test10/async9', 9)
    assert names[9] == ('test10/async10', 10)
