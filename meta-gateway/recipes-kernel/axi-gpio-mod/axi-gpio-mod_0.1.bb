SUMMARY = "Example of how to build an external Linux kernel module"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=12f884d2ae1ff87c09e5b7ccc2c4ca7e"

inherit module


FILESEXTRAPATHS_prepend := "${THISDIR}/files/inc:"
FILESEXTRAPATHS_prepend := "${THISDIR}/files/src:"
SRC_URI = "file://Makefile \
           file://gpio-xilinx.c \
           file://gpio-xilinx.h \
           file://COPYING \
          "

S = "${WORKDIR}"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.
