#!/bin/bash

_stop() { 
  kill -TERM "$pid" 2>/dev/null
}

trap '_stop' SIGTERM
trap '_stop' SIGINT

/ijson "$@" &

pid=$! 
wait "$pid"
