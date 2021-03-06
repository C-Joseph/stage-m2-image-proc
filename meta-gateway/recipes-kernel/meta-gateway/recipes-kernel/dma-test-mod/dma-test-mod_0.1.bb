SUMMARY = "External DMA Proxy Linux kernel module"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=12f884d2ae1ff87c09e5b7ccc2c4ca7e"

inherit module

FILESEXTRAPATHS_prepend := "${THISDIR}/files/inc:"
FILESEXTRAPATHS_prepend := "${THISDIR}/files/src:"
SRC_URI = "file://Makefile \
           file://dma-test.c \
           file://COPYING \
          "

S = "${WORKDIR}"

#FILES_${PN} += "/lib/modules/4.8.26-yocto-standard/extra"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.
