
# uwsgi_python3 --socket :5000 --wsgi-file app.py
# uwsgi --plugin python3 --socket :5000 --wsgi-file app.py --processes=8
# uwsgi --plugin python3 --socket :5000 --wsgi-file app.py --processes=4 --disable-logging

import ujson


def application(env, start_response):
    body = env['wsgi.input'].read()
    data = ujson.loads(body)
    response = {'result': data['a'] + data['b']}
    start_response('200 OK', [('Content-Type','text/html')])
    return [ujson.dumps(response).encode('utf8')]
