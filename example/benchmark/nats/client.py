
import asyncio
import ujson
from nats.aio.client import Client as NATS
from nats.aio.errors import ErrConnectionClosed, ErrTimeout, ErrNoServers
from requests import Counter


async def main():
    nc = NATS()
    await nc.connect("127.0.0.1:4222")

    counter = Counter()
    while True:
        response = await nc.request("sum", ujson.dumps({'a': 5, 'b': 8}).encode('utf8'))
        assert ujson.loads(response.data.decode('utf8'))['result'] == 13
        counter.inc()

    await nc.close()

asyncio.run(main())
