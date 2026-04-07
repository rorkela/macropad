#include <stdlib.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#define NRF24_CE_PORT  GPIOB
#define NRF24_CE_PIN   GPIO1
#define NRF24_CSN_PORT GPIOB
#define NRF24_CSN_PIN  GPIO0

// -- Register maps --
#define NRF24_REG_CONFIG      0x00

#define NRF24_CONFIG_PRIM_RX		(1 << 0)
#define NRF24_CONFIG_PRIM_TX		(0 << 0)
#define NRF24_CONFIG_PWR_UP			(1 << 1)
#define NRF24_CONFIG_CRCO_1B		(0 << 2)
#define NRF24_CONFIG_CRCO_2B		(1 << 2)
#define NRF24_CONFIG_EN_CRC			(1 << 3)
#define NRF24_CONFIG_MASK_MAX_RT	(1 << 4)
#define NRF24_CONFIG_MASK_TX_DS		(1 << 5)
#define NRF24_CONFIG_MASK_RX_DR		(1 << 6)

#define NRF24_REG_EN_AA       0x01

#define NRF24_ENAA_P0   (1 << 0)
#define NRF24_ENAA_P1   (1 << 1)
#define NRF24_ENAA_P2   (1 << 2)
#define NRF24_ENAA_P3   (1 << 3)
#define NRF24_ENAA_P4   (1 << 4)
#define NRF24_ENAA_P5   (1 << 5)

#define NRF24_REG_EN_RXADDR   0x02

#define NRF24_ERX_P0   (1 << 0)
#define NRF24_ERX_P1   (1 << 1)
#define NRF24_ERX_P2   (1 << 2)
#define NRF24_ERX_P3   (1 << 3)
#define NRF24_ERX_P4   (1 << 4)
#define NRF24_ERX_P5   (1 << 5)

#define NRF24_REG_SETUP_AW    0x03

#define NRF24_AW_3B 0x01
#define NRF24_AW_4B 0x02
#define NRF24_AW_5B 0x03

#define NRF24_REG_SETUP_RETR  0x04

#define NRF24_SETUP_RETR_ARD_250  0x00
#define NRF24_SETUP_RETR_ARD_500  0x10
#define NRF24_SETUP_RETR_ARD_750  0x20
#define NRF24_SETUP_RETR_ARD_1000 0x30
#define NRF24_SETUP_RETR_ARD_1250 0x40
#define NRF24_SETUP_RETR_ARD_1500 0x50
#define NRF24_SETUP_RETR_ARD_1750 0x60
#define NRF24_SETUP_RETR_ARD_2000 0x70
#define NRF24_SETUP_RETR_ARD_2250 0x80
#define NRF24_SETUP_RETR_ARD_2500 0x90
#define NRF24_SETUP_RETR_ARD_2750 0xA0
#define NRF24_SETUP_RETR_ARD_3000 0xB0
#define NRF24_SETUP_RETR_ARD_3250 0xC0
#define NRF24_SETUP_RETR_ARD_3500 0xD0
#define NRF24_SETUP_RETR_ARD_3750 0xE0
#define NRF24_SETUP_RETR_ARD_4000 0xF0

#define NRF24_SETUP_RETR_ARC_DISABLE	0x00
#define NRF24_SETUP_RETR_ARC_1			0x01
#define NRF24_SETUP_RETR_ARC_2			0x02
#define NRF24_SETUP_RETR_ARC_3			0x03
#define NRF24_SETUP_RETR_ARC_4			0x04
#define NRF24_SETUP_RETR_ARC_5			0x05
#define NRF24_SETUP_RETR_ARC_6			0x06
#define NRF24_SETUP_RETR_ARC_7			0x07
#define NRF24_SETUP_RETR_ARC_8			0x08
#define NRF24_SETUP_RETR_ARC_9			0x09
#define NRF24_SETUP_RETR_ARC_10			0x0A
#define NRF24_SETUP_RETR_ARC_11			0x0B
#define NRF24_SETUP_RETR_ARC_12			0x0C
#define NRF24_SETUP_RETR_ARC_13			0x0D
#define NRF24_SETUP_RETR_ARC_14			0x0E
#define NRF24_SETUP_RETR_ARC_15			0x0F

#define NRF24_REG_RF_CH       0x05

#define NRF24_REG_RF_SETUP    0x06

#define NRF24_RF_SETUP_LNA_HCURR	(1 << 0)
#define NRF24_RF_SETUP_RF_PWR_18DBM (0x00 << 1)
#define NRF24_RF_SETUP_RF_PWR_12DBM (0x01 << 1)
#define NRF24_RF_SETUP_RF_PWR_6DBM	(0x02 << 1)
#define NRF24_RF_SETUP_RF_PWR_0DBM	(0x03 << 1)
#define NRF24_RF_SETUP_RF_DR_1MBPS	(0 << 3)
#define NRF24_RF_SETUP_RF_DR_2MBPS	(1 << 3)
#define NRF24_RF_SETUP_PLL_LOCK		(1 << 4)

#define NRF24_REG_STATUS      0x07

#define NRF24_STATUS_TX_FULL		(1 << 0)
#define NRF24_STATUS_RX_P_0			(0x0 << 1)
#define NRF24_STATUS_RX_P_1			(0x1 << 1)
#define NRF24_STATUS_RX_P_2			(0x2 << 1)
#define NRF24_STATUS_RX_P_3			(0x3 << 1)
#define NRF24_STATUS_RX_P_4			(0x4 << 1)
#define NRF24_STATUS_RX_P_5			(0x5 << 1)
#define NRF24_STATUS_RX_P_NOT_USED	(0x6 << 1)
#define NRF24_STATUS_RX_P_EMPTY		(0x7 << 1)
#define NRF24_STATUS_MAX_RT			(1 << 4)
#define NRF24_STATUS_TX_DS			(1 << 5)
#define NRF24_STATUS_RX_DR			(1 << 6)

#define NRF24_REG_OBSERVE_TX  0x08
#define NRF24_REG_CD		  0x09
#define NRF24_REG_RX_ADDR_P0  0x0A
#define NRF24_REG_RX_ADDR_P1  0x0B
#define NRF24_REG_RX_ADDR_P2  0x0C
#define NRF24_REG_RX_ADDR_P3  0x0D
#define NRF24_REG_RX_ADDR_P4  0x0E
#define NRF24_REG_RX_ADDR_P5  0x0F
#define NRF24_REG_TX_ADDR     0x10
#define NRF24_REG_RX_PW_P0    0x11
#define NRF24_REG_RX_PW_P1    0x12
#define NRF24_REG_RX_PW_P2    0x13
#define NRF24_REG_RX_PW_P3    0x14
#define NRF24_REG_RX_PW_P4    0x15
#define NRF24_REG_RX_PW_P5    0x16
#define NRF24_REG_FIFO_STATUS 0x17
#define NRF24_REG_DYNPD       0x1C

#define NRF24_DYNPD_DPL_P0    (1 << 0)
#define NRF24_DYNPD_DPL_P1    (1 << 1)
#define NRF24_DYNPD_DPL_P2    (1 << 2)
#define NRF24_DYNPD_DPL_P3    (1 << 3)
#define NRF24_DYNPD_DPL_P4    (1 << 4)
#define NRF24_DYNPD_DPL_P5    (1 << 5)

#define NRF24_REG_FEATURE     0x1D

#define NRF24_FEATURE_EN_DPL	 (1 << 2)
#define NRF24_FEATURE_EN_ACK_PAY (1 << 1)
#define NRF24_FEATURE_EN_DYN_ACK (1 << 0)

// -- Commands --
#define NRF24_CMD_READ_REG     0x00
#define NRF24_CMD_WRITE_REG    0x20
#define NRF24_CMD_R_RX_PAYLOAD 0x61
#define NRF24_CMD_W_TX_PAYLOAD 0xA0
#define NRF24_CMD_FLUSH_TX     0xE1
#define NRF24_CMD_FLUSH_RX     0xE2
#define NRF24_CMD_NOP		   0xFF

static void spi_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_SPI1);
	rcc_periph_clock_enable(RCC_AFIO);

	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
		GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO5);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
		GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO7);
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
		GPIO_CNF_INPUT_FLOAT, GPIO6);

	gpio_set_mode(NRF24_CSN_PORT, GPIO_MODE_OUTPUT_50_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL, NRF24_CSN_PIN);
	gpio_set_mode(NRF24_CE_PORT, GPIO_MODE_OUTPUT_50_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL, NRF24_CE_PIN);

	rcc_periph_reset_pulse(RST_SPI1);

	spi_init_master(SPI1, SPI_CR1_BAUDRATE_FPCLK_DIV_16, SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
				 SPI_CR1_CPHA_CLK_TRANSITION_1, SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);

	spi_enable_software_slave_management(SPI1);
	spi_set_nss_high(SPI1);

	spi_enable(SPI1);
}

static void nrf24_csn_low(void)
{
	gpio_clear(NRF24_CSN_PORT, NRF24_CSN_PIN);
}

static void nrf24_csn_high(void)
{
	gpio_set(NRF24_CSN_PORT, NRF24_CSN_PIN);
}

static uint8_t spi_transfer(uint8_t data)
{
	uint32_t timeout;

	for (timeout = 0; !(SPI_SR(SPI1) & SPI_SR_TXE) && timeout < 10000; timeout++);
	spi_send(SPI1, data);

	for (timeout = 0; !(SPI_SR(SPI1) & SPI_SR_RXNE) && timeout < 10000; timeout++);
	return spi_read(SPI1);
}

static void nrf24_write_reg(uint8_t reg, uint8_t value)
{
	nrf24_csn_low();
	spi_transfer(NRF24_CMD_WRITE_REG | reg);
	spi_transfer(value);
	nrf24_csn_high();
}

static uint8_t nrf24_read_reg(uint8_t reg)
{
	nrf24_csn_low();
	uint8_t value;
	spi_transfer(NRF24_CMD_READ_REG | reg);
	value = spi_transfer(NRF24_CMD_NOP);
	nrf24_csn_high();
	return value;
}

static void nrf24_write_multi(uint8_t reg, uint8_t *data, uint8_t len)
{
	nrf24_csn_low();
	spi_transfer(NRF24_CMD_WRITE_REG | reg);
	for (uint8_t i = 0; i < len; i++) {
		spi_transfer(data[i]);
	}
	nrf24_csn_high();
}

static void nrf24_read_payload(uint8_t *data, uint8_t len)
{
	nrf24_csn_low();
	spi_transfer(NRF24_CMD_R_RX_PAYLOAD);
	for (uint8_t i = 0; i < len; i++) {
		data[i] = spi_transfer(0x00);
	}
	nrf24_csn_high();
}

static void nrf24_init(void)
{
	nrf24_csn_high();
	gpio_clear(NRF24_CE_PORT, NRF24_CE_PIN);

	nrf24_write_reg(NRF24_REG_CONFIG, NRF24_CONFIG_PWR_UP | NRF24_CONFIG_CRCO_1B | NRF24_CONFIG_EN_CRC);
	for(volatile int i = 0; i < 200000; i++) __asm__("nop");

	nrf24_write_reg(NRF24_REG_EN_AA, NRF24_ENAA_P0);
	nrf24_write_reg(NRF24_REG_SETUP_RETR, NRF24_SETUP_RETR_ARC_10 | NRF24_SETUP_RETR_ARD_1000);
	nrf24_write_reg(NRF24_REG_RF_CH, 76);
	nrf24_write_reg(NRF24_REG_RF_SETUP, NRF24_RF_SETUP_RF_PWR_0DBM | NRF24_RF_SETUP_RF_DR_1MBPS);
	nrf24_write_reg(NRF24_REG_SETUP_AW, NRF24_AW_5B);

	nrf24_csn_low();
	spi_transfer(NRF24_CMD_FLUSH_RX);
	nrf24_csn_high();

	uint8_t rx_addr[5] = {0x31, 0x30, 0x30, 0x30, 0x30};
	nrf24_write_multi(NRF24_REG_RX_ADDR_P0, rx_addr, 5);
	nrf24_write_reg(NRF24_REG_EN_RXADDR, NRF24_ERX_P0);
	nrf24_write_reg(NRF24_REG_RX_PW_P0, 1);

	nrf24_write_reg(NRF24_REG_STATUS, NRF24_STATUS_RX_DR | NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT);

	nrf24_write_reg(NRF24_REG_CONFIG, NRF24_CONFIG_PWR_UP | NRF24_CONFIG_CRCO_1B | NRF24_CONFIG_PRIM_RX);

	nrf24_csn_low();
	spi_transfer(NRF24_CMD_FLUSH_RX);
	nrf24_csn_high();

	gpio_set(NRF24_CE_PORT, NRF24_CE_PIN);
	nrf24_csn_high();

	for (volatile unsigned i = 0; i < 10000; i++) {
		__asm__("nop");
	}
}

static uint8_t nrf24_available(void)
{
	uint8_t status = nrf24_read_reg(NRF24_REG_STATUS);
	if (status & NRF24_STATUS_RX_DR) {
		return 1;
	}
	return 0;
}

static void nrf24_receive(uint8_t *data, uint8_t len)
{
	nrf24_read_payload(data, len);
	nrf24_write_reg(NRF24_REG_STATUS, NRF24_STATUS_RX_DR);
}

int main(void)
{
	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_HSI_48MHZ]);

	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
	gpio_set(GPIOC, GPIO13);

	spi_init();
	nrf24_init();

	uint8_t test = nrf24_read_reg(NRF24_REG_SETUP_RETR);
	if (test == (NRF24_SETUP_RETR_ARC_10 | NRF24_SETUP_RETR_ARD_1000)) {
		gpio_clear(GPIOC, GPIO13);
	}

	for (volatile unsigned i = 0; i < 10000000; i++) __asm__("nop");

	uint8_t packet[1];

	bool on = false;
	int timer = 50000;

	while (1) {
		if (nrf24_available()) {
			nrf24_receive(packet, 1);
			if(packet[0] == 0xff)
				on = true;
		}
		if(on) {
			gpio_clear(GPIOC, GPIO13);
			timer--;
			if(timer <= 0) {
				on = false;
				timer = 50000;
			}
		}
		else {
			gpio_set(GPIOC, GPIO13);
		}
	}
}
