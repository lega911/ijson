
from twisted.internet import reactor
from twisted.internet.defer import inlineCallbacks
from autobahn.twisted.wamp import ApplicationSession, ApplicationRunner
from requests import Counter


class Component(ApplicationSession):
    @inlineCallbacks
    def onJoin(self, details):
        counter = Counter()
        while True:
            response = yield self.call('func', {'a': 1, 'b': 2})
            assert response['result'] == 3
            counter.inc()

        self.leave()

    def onDisconnect(self):
        print("disconnected")
        reactor.stop()


if __name__ == '__main__':
    runner = ApplicationRunner("rs://localhost:8080", "realm1")
    runner.run(Component)
