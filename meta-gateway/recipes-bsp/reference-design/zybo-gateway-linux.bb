SUMMARY = "The reference design for zybo-gateway"
DESCRIPTION = "Contains the Reference Design Files and hardware software \
hand-off file. The HDF provides bitstream and Xilinx ps7_init_gpl.c/h \
platform headers."
SECTION = "bsp"
DEPENDS += "unzip-native"

LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Proprietary;md5=0557f9d92cf58f2ccdd50f62f8ac0b28"

COMPATIBLE_MACHINE = "zybo-gateway"

HW_BD = "hdmi2hdmi_video"

FILESEXTRAPATHS_prepend := "/home/joseph/Projects/hdmi2hdmi_video/:"

SRC_URI = "file://${HW_BD}_wrapper.hdf"

S = "${WORKDIR}"

HDF = "${HW_BD}_wrapper.hdf"

S ?= "${WORKDIR}/${MACHINE}"

PROVIDES = "virtual/bitstream virtual/zynq7-platform-init"

PLATFORM_INIT ?= "ps7_init_gpl.c \
		  ps7_init_gpl.h"

FILES_${PN}-platform-init += " \
		${PLATFORM_INIT_DIR}/ps7_init_gpl.c \
		${PLATFORM_INIT_DIR}/ps7_init_gpl.h \
		"

FILES_${PN}-bitstream += " \
		download.bit \
		"

PACKAGES = "${PN}-platform-init ${PN}-bitstream"

BITSTREAM ?= "bitstream-${PV}-${PR}.bit"

PACKAGE_ARCH = "${MACHINE_ARCH}"

inherit zynq7-platform-paths
inherit deploy

SYSROOT_DIRS += "${PLATFORM_INIT_DIR}"

do_install() {
	fn=$(unzip -l ${S}/${HDF} | awk '{print $NF}' | grep ".bit$")
	unzip -o ${S}/${HDF} ${fn} -d ${D}
	[ "${fn}" == "download.bit" ] || mv ${D}/${fn} ${D}/download.bit

	install -d ${D}${PLATFORM_INIT_DIR}
	for fn in ${PLATFORM_INIT}; do
		unzip -o ${S}/${HDF} ${fn} -d ${D}${PLATFORM_INIT_DIR}
	done
}

do_deploy () {
	if [ -e ${D}/download.bit ]; then
		install -d ${DEPLOYDIR}
		install -m 0644 ${D}/download.bit ${DEPLOYDIR}/${BITSTREAM}
		ln -sf ${BITSTREAM} ${DEPLOYDIR}/download.bit
		# for u-boot 2016.3 with spl load bitstream patch
		ln -sf ${BITSTREAM} ${DEPLOYDIR}/bitstream
	fi
}
addtask deploy before do_build after do_install
