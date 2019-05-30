
from twisted.internet.defer import inlineCallbacks
from autobahn.twisted.wamp import ApplicationSession, ApplicationRunner
from autobahn.wamp.types import RegisterOptions


class Component(ApplicationSession):
    @inlineCallbacks
    def onJoin(self, details):
        def func(data):
            return {'result': data['a'] + data['b']}

        try:
            # option: (single, first, last, roundrobin, random)
            yield self.register(func, 'func', options=RegisterOptions(invoke='roundrobin'))
        except Exception as e:
            print("failed to register procedure: {}".format(e))
        else:
            pass


if __name__ == '__main__':
    runner = ApplicationRunner("ws://127.0.0.1:8080/ws", "realm1")
    runner.run(Component)
