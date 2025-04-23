make clean
rm -f duo-imgtools/kernel.bin
date +%s --date 'now - 60 secs' > usr/etc/now
make
make kernel/kernel.bin
cp kernel/kernel.bin duo-imgtools/
cp fs.img duo-imgtools/
cd duo-imgtools
mkimage -f xv6.its boot.sd
./genimage.sh -c xv6.cfg
