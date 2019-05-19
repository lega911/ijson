
import sys
import asyncio
from httptools import HttpRequestParser
import ujson


class Proto(asyncio.Protocol):
    def connection_made(self, transport):
        self.transport = transport
        self.parser = HttpRequestParser(self)
        self.body = None

    def connection_lost(self, exc):
        self.transport = None

    def data_received(self, data):
        self.parser.feed_data(data)

    def on_body(self, body):
        self.body = body

    def on_message_complete(self):
        request = ujson.loads(self.body)
        response = {'result': request['a'] + request['b']}
        self.send(ujson.dumps(response).encode('utf8'))
        self.body = None

    def send(self, data):
        response = b'HTTP/1.1 200 OK\nContent-Length: ' + str(len(data)).encode() + b'\n\n' + data
        self.transport.write(response)

        if not self.parser.should_keep_alive():
            #print('no keep-alive')
            self.transport.close()
            self.transport = None


if __name__ == '__main__':
    port = int(sys.argv[1])
    loop = asyncio.get_event_loop()
    coro = loop.create_server(Proto, '127.0.0.1', port)
    srv = loop.run_until_complete(coro)

    loop.run_forever()
    loop.close()
