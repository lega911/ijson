#!/usr/bin/env bash
curl localhost:8001/run/command -H 'Type: async' -d 'start'
sleep 5
curl localhost:8001/run/command -H 'Type: async' -d 'stop'
