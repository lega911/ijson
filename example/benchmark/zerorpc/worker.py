
import zerorpc

class RPC(object):
    def sum(self, request):
        return {'result': request['a'] + request['b']}

s = zerorpc.Server(RPC())
s.bind("tcp://0.0.0.0:4242")
s.run()
