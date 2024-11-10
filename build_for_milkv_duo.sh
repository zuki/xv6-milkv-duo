cp kernel/config.h.cv1800b kernel/config.h
make clean
rm -f duo-imgtools/kernel.bin
make
make kernel/kernel.bin
cp kernel/kernel.bin duo-imgtools/
cp fs.img duo-imgtools/
cd duo-imgtools
mkimage -f xv6.its boot.sd
./genimage.sh -c xv6.cfg
