#!/usr/bin/env bash

while true
do
    sleep 1
    task=$(curl -s localhost:8001/run/command -H 'Type: get')
    status="$?"

    if [ $status -ne 0 ]; then
            echo "Server error"
            sleep 9
            continue;
    fi

    if [ "$task" == "start" ]; then
        echo START
    fi
    if [ "$task" == "stop" ]; then
        echo STOP
    fi
done
