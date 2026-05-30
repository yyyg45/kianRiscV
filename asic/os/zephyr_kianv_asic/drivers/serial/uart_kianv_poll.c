// SPDX-License-Identifier: Apache-2.0
/* Copyright (c) 2026 Hirosh Dabui <hirosh@dabui.de> */

#define DT_DRV_COMPAT kianv_poll_uart

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#define KIANV_UART_LSR_DR   0x01u
#define KIANV_UART_LSR_THRE 0x20u
#define KIANV_UART_LSR_TEMT 0x40u

struct kianv_uart_config {
	uintptr_t data_reg;
	uintptr_t lsr_reg;
	uintptr_t div_reg;
	uint32_t clock_frequency;
	uint32_t current_speed;
	uint32_t divisor1;
};

struct kianv_uart_data {
	const struct device *dev;
	uart_irq_callback_user_data_t callback;
	void *callback_data;
	bool rx_enabled;
	bool tx_enabled;
	bool thread_started;
	struct k_thread poll_thread;
	K_KERNEL_STACK_MEMBER(poll_stack, 512);
};

static void kianv_uart_poll_out(const struct device *dev, unsigned char c);
static void kianv_uart_start_poll_thread(const struct device *dev);

static uint8_t mmio_read8(uintptr_t addr)
{
	return *(volatile uint8_t *)addr;
}

static void mmio_write8(uintptr_t addr, uint8_t val)
{
	*(volatile uint8_t *)addr = val;
}

static int kianv_uart_poll_in(const struct device *dev, unsigned char *c)
{
	const struct kianv_uart_config *cfg = dev->config;

	if ((mmio_read8(cfg->lsr_reg) & KIANV_UART_LSR_DR) == 0u) {
		return -1;
	}

	*c = mmio_read8(cfg->data_reg);
	return 0;
}

static int kianv_uart_fifo_fill(const struct device *dev, const uint8_t *tx_data, int len)
{
	for (int i = 0; i < len; i++) {
		kianv_uart_poll_out(dev, tx_data[i]);
	}

	return len;
}

static int kianv_uart_fifo_read(const struct device *dev, uint8_t *rx_data, const int size)
{
	int count = 0;

	while ((count < size) && (kianv_uart_poll_in(dev, &rx_data[count]) == 0)) {
		count++;
	}

	return count;
}

static void kianv_uart_irq_tx_enable(const struct device *dev)
{
	struct kianv_uart_data *data = dev->data;

	data->tx_enabled = true;
}

static void kianv_uart_irq_tx_disable(const struct device *dev)
{
	struct kianv_uart_data *data = dev->data;

	data->tx_enabled = false;
}

static int kianv_uart_irq_tx_ready(const struct device *dev)
{
	const struct kianv_uart_config *cfg = dev->config;
	struct kianv_uart_data *data = dev->data;

	return data->tx_enabled && ((mmio_read8(cfg->lsr_reg) & KIANV_UART_LSR_THRE) != 0u);
}

static void kianv_uart_irq_rx_enable(const struct device *dev)
{
	struct kianv_uart_data *data = dev->data;

	data->rx_enabled = true;
}

static void kianv_uart_irq_rx_disable(const struct device *dev)
{
	struct kianv_uart_data *data = dev->data;

	data->rx_enabled = false;
}

static int kianv_uart_irq_tx_complete(const struct device *dev)
{
	const struct kianv_uart_config *cfg = dev->config;

	return (mmio_read8(cfg->lsr_reg) & KIANV_UART_LSR_TEMT) != 0u;
}

static int kianv_uart_irq_rx_ready(const struct device *dev)
{
	const struct kianv_uart_config *cfg = dev->config;
	struct kianv_uart_data *data = dev->data;

	return data->rx_enabled && ((mmio_read8(cfg->lsr_reg) & KIANV_UART_LSR_DR) != 0u);
}

static int kianv_uart_irq_is_pending(const struct device *dev)
{
	return kianv_uart_irq_rx_ready(dev) || kianv_uart_irq_tx_ready(dev);
}

static void kianv_uart_irq_update(const struct device *dev)
{
	ARG_UNUSED(dev);
}

static void kianv_uart_irq_callback_set(const struct device *dev,
					uart_irq_callback_user_data_t cb,
					void *user_data)
{
	struct kianv_uart_data *data = dev->data;

	data->callback = cb;
	data->callback_data = user_data;

	if (cb != NULL) {
		kianv_uart_start_poll_thread(dev);
	}
}

static void kianv_uart_poll_thread(void *arg0, void *arg1, void *arg2)
{
	const struct device *dev = arg0;
	struct kianv_uart_data *data = dev->data;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);

	for (;;) {
		if ((data->callback != NULL) && kianv_uart_irq_is_pending(dev)) {
			data->callback(dev, data->callback_data);
		}

		k_yield();
	}
}

static void kianv_uart_start_poll_thread(const struct device *dev)
{
	struct kianv_uart_data *data = dev->data;

	if (data->thread_started) {
		return;
	}

	data->thread_started = true;
	k_thread_create(&data->poll_thread, data->poll_stack,
			K_THREAD_STACK_SIZEOF(data->poll_stack),
			kianv_uart_poll_thread, (void *)dev, NULL, NULL,
			K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
}

static void kianv_uart_poll_out(const struct device *dev, unsigned char c)
{
	const struct kianv_uart_config *cfg = dev->config;

	while ((mmio_read8(cfg->lsr_reg) & (KIANV_UART_LSR_THRE | KIANV_UART_LSR_TEMT)) == 0u) {
	}

	mmio_write8(cfg->data_reg, c);
}

static int kianv_uart_err_check(const struct device *dev)
{
	ARG_UNUSED(dev);
	return 0;
}

static int kianv_uart_init(const struct device *dev)
{
	struct kianv_uart_data *data = dev->data;

	data->dev = dev;

	return 0;
}

static DEVICE_API(uart, kianv_uart_driver_api) = {
	.poll_in = kianv_uart_poll_in,
	.poll_out = kianv_uart_poll_out,
	.err_check = kianv_uart_err_check,
	.fifo_fill = kianv_uart_fifo_fill,
	.fifo_read = kianv_uart_fifo_read,
	.irq_tx_enable = kianv_uart_irq_tx_enable,
	.irq_tx_disable = kianv_uart_irq_tx_disable,
	.irq_tx_ready = kianv_uart_irq_tx_ready,
	.irq_rx_enable = kianv_uart_irq_rx_enable,
	.irq_rx_disable = kianv_uart_irq_rx_disable,
	.irq_tx_complete = kianv_uart_irq_tx_complete,
	.irq_rx_ready = kianv_uart_irq_rx_ready,
	.irq_is_pending = kianv_uart_irq_is_pending,
	.irq_update = kianv_uart_irq_update,
	.irq_callback_set = kianv_uart_irq_callback_set,
};

#define KIANV_UART_INIT(inst)                                                              \
	static struct kianv_uart_data kianv_uart_data_##inst;                              \
	static const struct kianv_uart_config kianv_uart_config_##inst = {                 \
		.data_reg = DT_INST_REG_ADDR_BY_NAME(inst, data),                          \
		.lsr_reg = DT_INST_REG_ADDR_BY_NAME(inst, lsr),                            \
		.div_reg = DT_INST_REG_ADDR_BY_NAME(inst, div),                            \
		.clock_frequency = DT_INST_PROP(inst, clock_frequency),                    \
		.current_speed = DT_INST_PROP(inst, current_speed),                        \
		.divisor1 = DT_INST_PROP(inst, kianv_divisor1),                            \
	};                                                                                 \
	DEVICE_DT_INST_DEFINE(inst, kianv_uart_init, NULL, &kianv_uart_data_##inst,     \
			      &kianv_uart_config_##inst,                                  \
			      PRE_KERNEL_1, CONFIG_SERIAL_INIT_PRIORITY,                       \
			      &kianv_uart_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KIANV_UART_INIT)
