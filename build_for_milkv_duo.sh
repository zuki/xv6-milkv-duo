cp kernel/config.h.cv1800b kernel/config.h
make
make kernel/kernel.bin
cp kernel/kernel.bin duo-imgtools/
cd duo-imgtools
mkimage -f xv6.its boot.sd
./genimage.sh -c xv6.cfg
