#include "i2s_rgb.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>
#include <string.h>
#include <stdint.h>

#include "i2s_rgb_config.h"


#if I2RGB_TIMING == WS2812
/*
 * Timings: 0: 300ns (high), 600ns (low)
 *          1: 600ns (high), 300ns (low)
 * -> Encoded into 3 bits per bit
 */
#define I2RGB_ENCODED_BITS_PER_BIT 3
#elif I2RGB_TIMING == SK6812
/*
 * Timings: 0: 300ns (high), 900ns (low)
 *          1: 600ns (high), 600ns (low)
 * -> Encoded into 4 bits per bit
 */
#define I2RGB_ENCODED_BITS_PER_BIT 4
#else
#error Invalid LED timing setting
#endif

#if I2RGB_DMA_MODE != I2RGB_DMA_MODE_NO_DMA
#define I2RGB_USE_DMA
#endif


#define I2RGB_NUM_DATA_BYTES (I2RGB_NUM_OF_LEDS * I2RGB_BYTES_PER_LED * 8 * I2RGB_ENCODED_BITS_PER_BIT - 1) / 8 + 1
#define I2RGB_NUM_BREAK_BYTES (I2RGB_NUM_BREAK_BITS * I2RGB_ENCODED_BITS_PER_BIT - 1) / 8 + 1

#define I2RGB_TOTAL_BYTES (I2RGB_NUM_DATA_BYTES + I2RGB_NUM_BREAK_BYTES)
#define I2RGB_BUFFER_PADDING (I2RGB_TOTAL_BYTES % 4)

static uint16_t global_buffer[(I2RGB_TOTAL_BYTES + I2RGB_BUFFER_PADDING) / sizeof(uint16_t)];

#if I2RGB_TIMING == SK6812
static const uint8_t lookup_table[] =
{
    0x88,
    0x8c,
    0xc8,
    0xcc
};
#endif

static uint32_t encode_byte(uint8_t b)
{
    uint32_t ret = 0;
#if I2RGB_TIMING == SK6812
        for (int i = 0; i < 4; ++i)
        {
            ret = (ret << 8) | lookup_table[b >> 6];
            b <<= 2;
        }
#else
    for (int i = 0; i < 8; ++i)
    {
        ret = (ret << 3) | 0x04;
        if (b & 0x80)
        {
            ret |= 0x02;
        }
        b <<= 1;
    }
#endif

    return ret;
}


static size_t encode_data(const uint8_t *data, uint16_t *buffer, size_t data_length)
{
    size_t outpos = 0;
    for (size_t i = 0; i < data_length; i++)
    {
        uint32_t enc = encode_byte(data[i]);
#if I2RGB_ENCODED_BITS_PER_BIT == 4
        buffer[outpos++] = enc >> 16;
        buffer[outpos++] = enc;
#elif I2RGB_ENCODED_BITS_PER_BIT == 3
        if (i & 1)
        {
            buffer[outpos++] |= enc >> 16;
            buffer[outpos++] = enc;
        }
        else
        {
            buffer[outpos++] = enc >> 8;
            buffer[outpos] = enc << 8;
        }
#else
#error Bad number of I2RGB_ENCODED_BITS_PER_BIT
#endif
    }

#if I2RGB_ENCODED_BITS_PER_BIT == 3
    if (data_length & 0x01)
    {
        outpos++;
    }
#endif

    return outpos;
}


void i2s_rgb_init(void)
{
    // Set up the I2S PLL

    // Turn off the I2S PLL
    RCC_CR &= ~RCC_CR_PLLI2SON;

    // Configure the pll (mul and div)
    RCC_PLLI2SCFGR  = I2RGB_I2S_PLL_MUL << RCC_PLLI2SCFGR_PLLI2SN_SHIFT;
    RCC_PLLI2SCFGR |= I2RGB_I2S_PLL_DIV << RCC_PLLI2SCFGR_PLLI2SR_SHIFT;

    // Turn on the PLL
    RCC_CR |= RCC_CR_PLLI2SON;

    // Wait for PLL ready
    while ((RCC_CR & RCC_CR_PLLI2SRDY) == 0)
    {
        // Just wait
    }

    // Set the clock source to the pll
    RCC_CFGR &= ~ RCC_CFGR_I2SSRC;

    // Power up the I2S (SPI) peripheral
	rcc_periph_clock_enable(I2RGB_SPI_RCC);

    // Switch to I2S mode
    SPI_I2SCFGR(I2RGB_SPI) = SPI_I2SCFGR_I2SMOD;

    // Master mode, Tx only
    SPI_I2SCFGR(I2RGB_SPI) |= SPI_I2SCFGR_I2SCFG_MASTER_TRANSMIT << SPI_I2SCFGR_I2SCFG_LSB;

    // Set to default Phillips mode
    //SPI_I2SCFGR(SPI) &= ~SPI_I2SCFGR_I2SSTD;

    // Set to 32 bit mode
    SPI_I2SCFGR(I2RGB_SPI) |= SPI_I2SCFGR_DATLEN_32BIT << SPI_I2SCFGR_I2SCFG_LSB;

    // Configure the prescaler (and disable master clock output)
    SPI_I2SPR(I2RGB_SPI) = I2RGB_I2S_CLK_DIV >> 1;
	if (I2RGB_I2S_CLK_DIV & 0x01)
	{
		SPI_I2SPR(I2RGB_SPI) |= SPI_I2SPR_ODD;
	}

    // Enable the I2S
    SPI_I2SCFGR(I2RGB_SPI) |= SPI_I2SCFGR_I2SE;

    // Do the GPIO configuration
	rcc_periph_clock_enable(I2RGB_SPI_GPIO_RCC);
	gpio_mode_setup(I2RGB_SPI_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, I2RGB_SPI_GPIO_PIN);
	gpio_set_af(I2RGB_SPI_GPIO_PORT, I2RGB_SPI_GPIO_PIN_AF, I2RGB_SPI_GPIO_PIN);

    // Init the static buffer
    memset(global_buffer, 0, sizeof(global_buffer));

#ifdef I2RGB_USE_DMA
    // Set-up DMA

    // Enable DMA 1
	rcc_periph_clock_enable(I2RGB_DMA_RCC);

	dma_stream_reset(I2RGB_DMA, I2RGB_DMA_STREAM);

	dma_enable_memory_increment_mode(I2RGB_DMA, I2RGB_DMA_STREAM);
	dma_set_transfer_mode(I2RGB_DMA, I2RGB_DMA_STREAM, DMA_SxCR_DIR_MEM_TO_PERIPHERAL);


	// SPI data reg is written 16-bit wise, even though the data transfers are 32 bit
	dma_set_peripheral_size(I2RGB_DMA, I2RGB_DMA_STREAM, DMA_SxCR_PSIZE_16BIT);
	dma_set_memory_size(I2RGB_DMA, I2RGB_DMA_STREAM, DMA_SxCR_MSIZE_16BIT);

	dma_channel_select(I2RGB_DMA, I2RGB_DMA_STREAM, I2RGB_DMA_CHANNEL);

	dma_set_number_of_data(I2RGB_DMA, I2RGB_DMA_STREAM, sizeof(global_buffer) / sizeof(global_buffer[0]));
	dma_set_peripheral_address(I2RGB_DMA, I2RGB_DMA_STREAM, (uint32_t)&SPI_DR(I2RGB_SPI));
	dma_set_memory_address(I2RGB_DMA, I2RGB_DMA_STREAM, (uint32_t)global_buffer);


    // Enable DMA for SPI
    SPI_CR2(I2RGB_SPI) |= SPI_CR2_TXDMAEN;

#if I2RGB_DMA_MODE == I2RGB_DMA_MODE_CONTINOUS
	dma_enable_circular_mode(I2RGB_DMA, I2RGB_DMA_STREAM);
#endif

    // Start the DMA
	dma_enable_stream(I2RGB_DMA, I2RGB_DMA_STREAM);
#endif
}


void i2s_rgb_set(const uint8_t *data)
{
#if I2RGB_DMA_MODE == I2RGB_DMA_MODE_SINGLE
    // Need to wait for the DMA to finish
    while (DMA_SCR(I2RGB_DMA, I2RGB_DMA_STREAM) & DMA_SxCR_EN);
#endif

    encode_data(data, global_buffer, I2RGB_NUM_OF_LEDS * I2RGB_BYTES_PER_LED);

#if I2RGB_DMA_MODE == I2RGB_DMA_MODE_SINGLE
	// Clear interrupts
	dma_clear_interrupt_flags(I2RGB_DMA, I2RGB_DMA_STREAM, DMA_TCIF | DMA_HTIF | DMA_TEIF | DMA_DMEIF | DMA_FEIF);

	// Start the DMA again
	dma_enable_stream(I2RGB_DMA, I2RGB_DMA_STREAM);
#elif I2RGB_DMA_MODE == I2RGB_DMA_MODE_NO_DMA
	spi_send(I2RGB_SPI, 0);
	spi_send(I2RGB_SPI, 0);
	for (uint32_t i = 0; i < (sizeof(global_buffer) / sizeof(global_buffer[0])); i++)
	{
		spi_send(I2RGB_SPI, global_buffer[i]);
	}
#endif
}
