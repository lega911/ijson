
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
        curl.setopt(pycurl.HTTPHEADER, ['Accept-Encoding:', 'Content-Type:', 'Accept:', 'User-Agent:'])
        curl.perform()

        response = buffer.getvalue().decode('utf8')
        if response:
            return ujson.loads(response)

    def get(self, url, *, headers=None):
        curl = self.curl
        buffer = BytesIO()

        hdr = {
            'Accept-Encoding': None,
            'Content-Type': None,
            'Accept': None,
            'User-Agent': None
        }
        if headers:
            for k, v in headers.items():
                hdr[k] = v

        hdrl = []
        for k, v in hdr.items():
            if v is None:
                hdrl.append(k + ':')
            else:
                hdrl.append(k + ': ' + v)

        curl.setopt(pycurl.URL, url)
        curl.setopt(pycurl.WRITEDATA, buffer)
        curl.setopt(pycurl.HTTPHEADER, hdrl)
        curl.perform()

        response = buffer.getvalue().decode('utf8')
        if response:
            return ujson.loads(response)

