
import grpc
import helloworld_pb2
import helloworld_pb2_grpc
from requests import Counter


counter = Counter()
with grpc.insecure_channel('localhost:50051') as channel:
    stub = helloworld_pb2_grpc.GreeterStub(channel)
    while True:
        response = stub.SayHello(helloworld_pb2.HelloRequest(a=3, b=5))
        assert response.result == 8
        counter.inc()
