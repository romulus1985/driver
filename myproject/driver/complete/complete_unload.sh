#!/bin/bash
ko="complete_sz"
dev="complete_dev"

echo "ko is:"${ko}
/sbin/rmmod ${ko} $* || exit 1
#/sbin/rmmod complete_sz
rm -f /dev/${dev}
