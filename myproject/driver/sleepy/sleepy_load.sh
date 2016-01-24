#!/bin/bash

module="sleepy_sz"
device="sleepy_sz"
mode="666"

if grep -q '^staff:' /etc/group; then
    group="staff"
else
    group="wheel"
fi

/sbin/insmod ./${module}.ko $* || exit 1

major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)
echo "major is:"${major}

rm -f /dev/${device}
mknod /dev/${device} c ${major} 0

chgrp $group /dev/${device}
chown $mode /dev/${device}
