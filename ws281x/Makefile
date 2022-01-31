PROGRAM = homekit-ws281x

EXTRA_COMPONENTS += \
	extras/i2s_dma \
	extras/ws2812_i2s

ifeq ($(WS2812_I2S_DEBUG),1)
	EXTRA_CFLAGS += -DWS2812_I2S_DEBUG -DI2S_DMA_DEBUG
endif

include ../common.mk
