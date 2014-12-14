#!/bin/sh
start_time=`date +'%d/%m/%y %H:%M:%S'`
export KERNELDIR=`readlink -f .`
export RAMFS_SOURCE=`readlink -f $KERNELDIR/ramfs`
#export USE_SEC_FIPS_MODE=true

if [ "${1}" != "" ];then
  export KERNELDIR=`readlink -f ${1}`
fi

RAMFS_TMP="/home/yank555-lu/temp/tmp/ramfs-source-n6-lollipop"

. $KERNELDIR/.config

echo ".............................................................Building new ramdisk............................................................."
#remove previous ramfs files
rm -rf $RAMFS_TMP
rm -rf $RAMFS_TMP.cpio
rm -rf $RAMFS_TMP.cpio.gz
#copy ramfs files to tmp directory
cp -ax $RAMFS_SOURCE $RAMFS_TMP
#clear git repositories in ramfs
find $RAMFS_TMP -name .git -exec rm -rf {} \;
#remove empty directory placeholders
find $RAMFS_TMP -name EMPTY_DIRECTORY -exec rm -rf {} \;
rm -rf $RAMFS_TMP/tmp/*
#remove mercurial repository
rm -rf $RAMFS_TMP/.hg
cd $RAMFS_TMP
find | fakeroot cpio -H newc -o > $RAMFS_TMP.cpio 2>/dev/null
ls -lh $RAMFS_TMP.cpio
gzip -9 $RAMFS_TMP.cpio

echo "...............................................................Compiling kernel..............................................................."
#remove previous out files
rm $KERNELDIR/dt.img
rm $KERNELDIR/boot.img
rm $KERNELDIR/*.ko
#compile kernel
cd $KERNELDIR
make -j `getconf _NPROCESSORS_ONLN` || exit 1

echo ".............................................................Making new boot image............................................................"
./buildtools/mkbootimg_dtb --kernel $KERNELDIR/arch/arm/boot/zImage-dtb --ramdisk $RAMFS_TMP.cpio.gz --cmdline "console=ttyHSL0,115200,n8 androidboot.console=ttyHSL0 androidboot.hardware=shamu msm_rtb.filter=0x37 ehci-hcd.park=3 utags.blkdev=/dev/block/platform/msm_sdcc.1/by-name/utags utags.backup=/dev/block/platform/msm_sdcc.1/by-name/utagsBackup coherent_pool=8M" --pagesize 2048 -o $KERNELDIR/boot.img

echo "...............................................................Stripping Modules.............................................................."
find . -name "*.ko" -exec mv {} . \;
${CROSS_COMPILE}strip --strip-unneeded ./*.ko
echo "Started  : $start_time"
echo "Finished : `date +'%d/%m/%y %H:%M:%S'`"
echo ".....................................................................done....................................................................."
find . -name "boot.img"
find . -name "*.ko"
