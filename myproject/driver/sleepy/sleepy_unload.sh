#!/bin/bash

module="sleepy_sz"
device="sleepy_sz"

/sbin/rmmod ${module} $* || exit 1

rm -f /dev/${device}
