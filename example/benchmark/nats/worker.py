
import asyncio
import ujson
from nats.aio.client import Client as NATS
from nats.aio.errors import ErrConnectionClosed, ErrTimeout, ErrNoServers


async def main():
    nc = NATS()
    await nc.connect("127.0.0.1:4222")

    async def handler(msg):
        request = ujson.loads(msg.data.decode('utf8'))
        response = {'result': request['a'] + request['b']}
        await nc.publish(msg.reply, ujson.dumps(response).encode('utf8'))

    sid = await nc.subscribe("sum", "workers", handler)

    await asyncio.sleep(3600)

    await nc.unsubscribe(sid)
    await nc.close()

asyncio.run(main())
