#!/bin/bash

kill -9 $(< /tmp/kovcam.was.pid)
kill -9 $(< /tmp/kovcam.broadcaster.pid)
