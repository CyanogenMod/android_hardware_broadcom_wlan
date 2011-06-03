#export ANDROID_PATH=/home/greg/ANDROID/FROYO/
export CC_KERNEL=/usr/local/google/android/master/kernel/
#export CC_TOOLS=/home/greg/ANDROID/GINGERBREAD/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin
#export BUILD_PATH=/home/greg/ANDROID/WORK/FALCON_122_POST/open-src/src/dhd/linux/dhd-cdc-sdmmc-nexus-cfg80211-gpl-debug-2.6.38-rc6/bcm4329.ko

echo "===> $CC_KERNEL <===="
echo "===> $CC_TOOLS <===="
echo "===> $ANDROID_PATH= <===="

#make clean

make LINUXDIR=$CC_KERNEL ARCH=arm CROSS_COMPILE=arm-eabi- dhd-cdc-sdmmc-nexus-cfg80211-gpl-debug

#adb root
#adb remount

echo "== $BUILD_PATH ==="
#adb push $BUILD_PATH  /system/lib/modules/bcm4329.ko
# FW /system/vendor/firmware
#echo "===> CFG : $BUILD_PATH <===="


