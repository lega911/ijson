
from twisted.internet import reactor
from twisted.internet.defer import inlineCallbacks
from autobahn.twisted.wamp import ApplicationSession, ApplicationRunner
from requests import Counter


class Component(ApplicationSession):
    @inlineCallbacks
    def onJoin(self, details):
        counter = Counter()
        for i in range(0, 10**10):
            response = yield self.call('func', {'a': 1, 'b': 2})
            assert response['result'] == 3
            counter.set(i)

        self.leave()

    def onDisconnect(self):
        print("disconnected")
        reactor.stop()


if __name__ == '__main__':
    runner = ApplicationRunner("ws://127.0.0.1:8080/ws", "realm1")
    runner.run(Component)
