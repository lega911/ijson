
import pika
import uuid
import ujson
import time
from requests import Session, Counter


class Client(object):
    def __init__(self):
        self.corr_id = str(uuid.uuid4())
        self.connection = pika.BlockingConnection(
            pika.ConnectionParameters(host='localhost'))

        self.channel = self.connection.channel()

        result = self.channel.queue_declare('', exclusive=True)
        self.callback_queue = result.method.queue

        self.channel.basic_consume(
            queue=self.callback_queue,
            on_message_callback=self.on_response,
            auto_ack=True)

    def on_response(self, ch, method, props, body):
        if self.corr_id == props.correlation_id:
            self.response = body

    def call(self, request):
        self.response = None
        self.channel.basic_publish(
            exchange='',
            routing_key='rpc_queue',
            properties=pika.BasicProperties(
                reply_to=self.callback_queue,
                correlation_id=self.corr_id,
            ),
            body=ujson.dumps(request))
        while self.response is None:
            self.connection.process_data_events()
        return ujson.loads(self.response)

counter = Counter()
rpc = Client()
for i in range(10**10):
    response = rpc.call({'a': 8, 'b': 5})
    assert response['result'] == 13
    counter.set(i)
