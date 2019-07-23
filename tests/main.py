
import time
import threading
import requests


def post(path, *a, **kw):
    if 'timeout' not in kw:
        kw['timeout'] = 5
    return requests.post('http://localhost:8001' + path, *a, **kw)


def test_default():
    clients = [None, None]

    def _request(i):
        time.sleep(0.1 + 0.1 * i)
        clients[i] = post('/test/cmd1', json={'id': i + 1, 'params': 'ubuntu'}, timeout=5)

    threading.Thread(target=_request, args=(0,)).start()
    threading.Thread(target=_request, args=(1,)).start()

    worker = post('/rpc/add', json={'params': 'test/cmd1'}, timeout=5)
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

    worker = post('/rpc/add', json={'params': 'test/cmd1'}, timeout=5)
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


def test_multimethods():
    worker = None

    def run_worker():
        nonlocal worker
        try:
            s = requests.Session()
            while True:
                r = s.post('http://localhost:8001/rpc/add', json={'params': {'name': 'test/cmd2+x15,test/cmd2,stop', 'id': False}})
                assert r.status_code == 200
                worker = r.headers['name']
                if worker == 'stop':
                    break
                task = r.json()
                r = s.post('http://localhost:8001/rpc/result', json={'result': task['a'] + task['b']})
                assert r.status_code == 200
            worker = 'stopped'
        except (Exception, AssertionError):
            worker = 'error'
    
    threading.Thread(target=run_worker).start()
    time.sleep(0.1)

    try:
        r = post('/test/nonexists')
        assert r.status_code == 404

        r = post('/test/cmd2', json={'a': 3, 'b': 5})
        assert r.status_code == 200
        assert r.json()['result'] == 8
        assert worker == 'test/cmd2'

        r = post('/test/cmd2+x15', json={'a': 3, 'b': 10})
        assert r.status_code == 200
        assert r.json()['result'] == 13
        assert worker == 'test/cmd2+x15'

        r = post('/test/cmd2', json={'a': 5, 'b': 5})
        assert r.status_code == 200
        assert r.json()['result'] == 10
        assert worker == 'test/cmd2'
    finally:
        post('/stop')
        time.sleep(0.1)
        assert worker == 'stopped'


def test_request_withnoid():
    def run_worker():
        r = post('/rpc/add', json={'params': 'one'})
        post('/rpc/result', json={'result': 'ok'}, headers={'id': r.headers['id']})
    
    threading.Thread(target=run_worker).start()
    time.sleep(0.1)
    r = post('/one', json={'params': 1})
    assert r.status_code == 200
    assert r.json()['result'] == 'ok'


def test_worker_mode():
    def worker():
        s = requests.Session()
        task = s.post('http://localhost:8001/rpc/worker', json={'params': '/test/worker'}).json()
        while True:
            time.sleep(0.1)
            if task.get('stop'):
                break
            task = s.post('http://localhost:8001/rpc/worker', json={'result': sum(task['nums'])}).json()

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


def test_pattern():
    h_response = None

    def worker():
        nonlocal h_response
        link = 'http://localhost:8001/rpc/worker'
        s = requests.Session()
        r = s.post(link, json={'params': 'pattern/*'})
        while True:
            name = r.headers['name'][8:]
            time.sleep(0.1)

            if not name:
                r = s.post(link, data=b'sum,mul,stop')
                continue
            
            if name == 'sum':
                task = r.json()
                response = {'result': sum(task['value'])}
            elif name == 'mul':
                task = r.json()
                response = {'result': task['value'][0] * task['value'][1]}
            elif name == 'stop':
                r = s.post(link, json={'result': 'ok'}, headers={'Option': 'stop'})
                break
            else:
                response = {'error': 'no method'}

            r = s.post(link, json=response)

        time.sleep(0.2)
        h_response = s.post('http://localhost:8001/task/revert', json={'id': 12345}).json()

    threading.Thread(target=worker).start()
    time.sleep(0.1)

    assert requests.get('http://localhost:8001/pattern/').text == 'sum,mul,stop'

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
        requests.get('http://localhost:8001/pattern/', timeout=0.1)
    except Exception as e:
        error = e
    assert isinstance(error, requests.exceptions.ReadTimeout)
    
    task = post('/rpc/add', json={'params': '/task/revert'}).json()
    assert task['id'] == 12345
    post('/rpc/result', json={'id': 12345, 'result': 'ok'})
    time.sleep(0.1)
    assert h_response['result'] == 'ok'
    assert h_response['id'] == 12345


def test4():
    h_response = None

    def worker():
        nonlocal h_response
        link = 'http://localhost:8001/rpc/worker'
        s = requests.Session()
        s.post(link, json={'params': 'test4/*'})
        s.post(link, data=b'sum,mul,stop')

    threading.Thread(target=worker).start()
    time.sleep(0.1)

    assert requests.get('http://localhost:8001/test4/').text == 'sum,mul,stop'

    r = post('/test4/sum', json={'value': [1, 2, 3, 4]})
    assert r.status_code == 503
