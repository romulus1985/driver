#!/bin/bash
ko="complete_sz"
device="complete_device_sz"
dev="complete_dev"

mode="666"
if grep -q '^staff:' /etc/group; then
	group="staff"
else
	group="wheel"
fi

/sbin/insmod ./${ko}.ko $* || exit 1

major=$(awk "\$2==\"${device}\" {print \$1}" /proc/devices)
#major=$(awk "\$2==\"complte_device_sz\" {print \$1}" /proc/devices)

rm -f /dev/${dev}
echo "major is:"${major}
mknod /dev/${dev} c ${major} 0

chgrp $group /dev/${dev}
chmod $mode /dev/${dev}
