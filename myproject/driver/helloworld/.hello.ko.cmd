cmd_/home/romulus/driver/helloworld/hello.ko := ld -r -m elf_x86_64 -T /usr/src/linux-headers-3.2.0-89-generic/scripts/module-common.lds --build-id  -o /home/romulus/driver/helloworld/hello.ko /home/romulus/driver/helloworld/hello.o /home/romulus/driver/helloworld/hello.mod.o