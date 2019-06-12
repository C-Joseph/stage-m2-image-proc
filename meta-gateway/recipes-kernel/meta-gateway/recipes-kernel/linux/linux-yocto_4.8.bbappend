FILESEXTRAPATHS_prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://axi-gpio.cfg \
            file://leds.cfg \
	file://spi.cfg \
	file://kb-gpio.cfg \
	file://I2C.cfg \
	file://uartPS.cfg \
	file://UIO.cfg \
	file://cdc-acm.cfg \
	file://fb.cfg \
	file://video.cfg \
	"
