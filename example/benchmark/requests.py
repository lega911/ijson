
import pycurl
from io import BytesIO
import ujson


class Session:
    def __init__(self):
        self.curl = pycurl.Curl()

    def post(self, url, *, json=None, data=None):
        if json:
            data = ujson.dumps(json).encode('utf8')

        curl = self.curl
        buffer = BytesIO()

        curl.setopt(pycurl.URL, url)
        curl.setopt(pycurl.POST, 1)
        if data:
            curl.setopt(pycurl.POSTFIELDS, data)
        curl.setopt(pycurl.WRITEDATA, buffer)
        curl.perform()

        response = buffer.getvalue().decode('utf8')
        if response:
            return ujson.loads(response)
