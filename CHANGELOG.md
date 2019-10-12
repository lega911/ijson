# Changelog

## 0.4
* Publish message to all workers - header `"Type: pub"`
* Set type of request - header `"Type"/"X-Type"`, e.g. `"Type: async"`, supported: *async, get, get+, worker, result*
* Call a certain worker by id, header `"Worker-ID: <worker id>"`
* `/rpc/details` displays worker ids
* Set worker id via header `"Set-ID: <worker id>"`

## 0.3
* Call a command without await a response: header `Option: async`
* Support: Linux - epoll, FreeBSD and MacOS - kqueue
* Priority for calling a command: header `Priority: <number>`
* `/rpc/help` displays count `number workers - number clients`
* change uuid to numerator
* a new mode for workers to reduce io: `worker mode`
* multi-threading, to use more than 1 core
* docker-slim - compact version of docker-image
* support a pattern for queue name, e.g. `/test/*`
