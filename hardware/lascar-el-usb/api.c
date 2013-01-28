/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <libusb.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"
#include "protocol.h"

SR_PRIV struct sr_dev_inst *lascar_scan(int bus, int address);
SR_PRIV struct sr_dev_driver lascar_el_usb_driver_info;
static struct sr_dev_driver *di = &lascar_el_usb_driver_info;
static int hw_dev_close(struct sr_dev_inst *sdi);

static const int hwopts[] = {
	SR_CONF_CONN,
	0,
};

static const int hwcaps[] = {
	SR_CONF_THERMOMETER,
	SR_CONF_HYGROMETER,
	SR_CONF_LIMIT_SAMPLES,
	0
};

/* Properly close and free all devices. */
static int clear_instances(void)
{
	struct sr_dev_inst *sdi;
	struct drv_context *drvc;
	struct dev_context *devc;
	GSList *l;

	if (!(drvc = di->priv))
		return SR_OK;

	for (l = drvc->instances; l; l = l->next) {
		if (!(sdi = l->data))
			continue;
		if (!(devc = sdi->priv))
			continue;

		hw_dev_close(sdi);
		sr_usb_dev_inst_free(devc->usb);
		sr_dev_inst_free(sdi);
	}

	g_slist_free(drvc->instances);
	drvc->instances = NULL;

	return SR_OK;
}

static int hw_init(struct sr_context *sr_ctx)
{
	struct drv_context *drvc;

	if (!(drvc = g_try_malloc0(sizeof(struct drv_context)))) {
		sr_err("Driver context malloc failed.");
		return SR_ERR_MALLOC;
	}

	drvc->sr_ctx = sr_ctx;
	di->priv = drvc;

	return SR_OK;
}

static GSList *hw_scan(GSList *options)
{
	struct drv_context *drvc;
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	struct sr_usb_dev_inst *usb;
	struct sr_config *src;
	GSList *usb_devices, *devices, *l;
	const char *conn;

	(void)options;

	drvc = di->priv;

	/* USB scan is always authoritative. */
	clear_instances();

	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = src->value;
			break;
		}
	}
	if (!conn)
		return NULL;

	devices = NULL;
	if ((usb_devices = sr_usb_find(drvc->sr_ctx->libusb_ctx, conn))) {
		/* We have a list of sr_usb_dev_inst matching the connection
		 * string. Wrap them in sr_dev_inst and we're done. */
		for (l = usb_devices; l; l = l->next) {
			usb = l->data;
			if (!(sdi = lascar_scan(usb->bus, usb->address))) {
				/* Not a Lascar EL-USB. */
				g_free(usb);
				continue;
			}
			devc = sdi->priv;
			devc->usb = usb;
			drvc->instances = g_slist_append(drvc->instances, sdi);
			devices = g_slist_append(devices, sdi);
		}
		g_slist_free(usb_devices);
	} else
		g_slist_free_full(usb_devices, g_free);

	return devices;
}

static GSList *hw_dev_list(void)
{
	struct drv_context *drvc;

	if (!(drvc = di->priv)) {
		sr_err("Driver was not initialized.");
		return NULL;
	}

	return drvc->instances;
}

static int hw_dev_open(struct sr_dev_inst *sdi)
{
	struct drv_context *drvc;
	struct dev_context *devc;
	int ret;

	if (!(drvc = di->priv)) {
		sr_err("Driver was not initialized.");
		return SR_ERR;
	}

	devc = sdi->priv;
	if (sr_usb_open(drvc->sr_ctx->libusb_ctx, devc->usb) != SR_OK)
		return SR_ERR;

	if ((ret = libusb_claim_interface(devc->usb->devhdl, LASCAR_INTERFACE))) {
		sr_err("Failed to claim interface: %s.", libusb_error_name(ret));
		return SR_ERR;
	}
	sdi->status = SR_ST_ACTIVE;

	return ret;
}

static int hw_dev_close(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	if (!di->priv) {
		sr_err("Driver was not initialized.");
		return SR_ERR;
	}

	devc = sdi->priv;
	if (!devc->usb->devhdl)
		/*  Nothing to do. */
		return SR_OK;

	libusb_release_interface(devc->usb->devhdl, LASCAR_INTERFACE);
	libusb_close(devc->usb->devhdl);
	devc->usb->devhdl = NULL;
	g_free(devc->config);
	sdi->status = SR_ST_INACTIVE;


	return SR_OK;
}

static int hw_cleanup(void)
{
	struct drv_context *drvc;

	if (!(drvc = di->priv))
		/* Can get called on an unused driver, doesn't matter. */
		return SR_OK;

	clear_instances();
	g_free(drvc);
	di->priv = NULL;

	return SR_OK;
}

static int config_set(int id, const void *value, const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;

	if (!di->priv) {
		sr_err("Driver was not initialized.");
		return SR_ERR;
	}
	if (sdi->status != SR_ST_ACTIVE) {
		sr_err("Device inactive, can't set config options.");
		return SR_ERR;
	}

	devc = sdi->priv;
	ret = SR_OK;
	switch (id) {
	case SR_CONF_LIMIT_SAMPLES:
		devc->limit_samples = *(const uint64_t *)value;
		sr_dbg("Setting sample limit to %" PRIu64 ".",
		       devc->limit_samples);
		break;
	default:
		sr_err("Unknown hardware capability: %d.", id);
		ret = SR_ERR_ARG;
	}

	return ret;
}

static int config_list(int key, const void **data, const struct sr_dev_inst *sdi)
{

	(void)sdi;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
		*data = hwopts;
		break;
	case SR_CONF_DEVICE_OPTIONS:
		*data = hwcaps;
		break;
	default:
		return SR_ERR_ARG;
	}

	return SR_OK;
}

static void mark_xfer(struct libusb_transfer *xfer)
{

	if (xfer->status == LIBUSB_TRANSFER_COMPLETED)
		xfer->user_data = GINT_TO_POINTER(1);
	else
		xfer->user_data = GINT_TO_POINTER(-1);

}

/* The Lascar software, in its infinite ignorance, reads a set of four
 * bytes from the device config struct and interprets it as a float.
 * That only works because they only use windows, and only on x86. However
 * we may be running on any architecture, any operating system. So we have
 * to convert these four bytes as the Lascar software would on windows/x86,
 * to the local representation of a float.
 * The source format is little-endian, with IEEE 754-2008 BINARY32 encoding. */
static float binary32_le_to_float(unsigned char *buf)
{
	GFloatIEEE754 f;

	f.v_float = 0;
	f.mpn.sign = (buf[3] & 0x80) ? 1 : 0;
	f.mpn.biased_exponent = (buf[3] << 1) | (buf[2] >> 7);
	f.mpn.mantissa = buf[0] | (buf[1] << 8) | ((buf[2] & 0x7f) << 16);

	return f.v_float;
}

static int lascar_proc_config(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;

	devc = sdi->priv;
	if (!(devc->config = lascar_get_config(devc->usb->devhdl)))
		return SR_ERR;

	ret = SR_OK;
	switch (devc->profile->logformat) {
	case LOG_TEMP_RH:
		devc->sample_size = 2;
		devc->temp_unit = devc->config[0x2e] | (devc->config[0x2f] << 8);
		if (devc->temp_unit != 0 && devc->temp_unit != 1) {
			sr_dbg("invalid temperature unit %d", devc->temp_unit);
			/* Default to Celcius, we're all adults here. */
			devc->temp_unit = 0;
		} else
			sr_dbg("temperature unit is %s", devc->temp_unit
					? "Fahrenheit" : "Celcius");
		break;
	case LOG_CO:
		devc->sample_size = 2;
		devc->co_high = binary32_le_to_float(devc->config + 0x24);
		devc->co_low = binary32_le_to_float(devc->config + 0x28);
		sr_dbg("EL-USB-CO calibration high %f low %f", devc->co_high,
				devc->co_low);
		break;
	default:
		ret = SR_ERR_ARG;
	}
	devc->logged_samples = devc->config[0x1e] | (devc->config[0x1f] << 8);
	sr_dbg("device log contains %d samples.", devc->logged_samples);

	return ret;
}

static int hw_dev_acquisition_start(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_header header;
	struct dev_context *devc;
	struct drv_context *drvc = di->priv;
	struct libusb_transfer *xfer_in, *xfer_out;
	const struct libusb_pollfd **pfd;
	struct timeval tv;
	int ret, i;
	unsigned char cmd[3], resp[4], *buf;

	if (!di->priv) {
		sr_err("Driver was not initialized.");
		return SR_ERR;
	}

	devc = sdi->priv;
	devc->cb_data = cb_data;

	if (lascar_proc_config(sdi) != SR_OK)
		return SR_ERR;

	sr_dbg("Starting log retrieval.");

	/* Send header packet to the session bus. */
	sr_dbg("Sending SR_DF_HEADER.");
	packet.type = SR_DF_HEADER;
	packet.payload = (uint8_t *)&header;
	header.feed_version = 1;
	sr_session_send(devc->cb_data, &packet);

	if (devc->logged_samples == 0) {
		/* This ensures the frontend knows the session is done. */
		packet.type = SR_DF_END;
		sr_session_send(devc->cb_data, &packet);
		return SR_OK;
	}

	if (!(xfer_in = libusb_alloc_transfer(0)) ||
			!(xfer_out = libusb_alloc_transfer(0)))
		return SR_ERR;

	libusb_control_transfer(devc->usb->devhdl, LIBUSB_REQUEST_TYPE_VENDOR,
			0x00, 0xffff, 0x00, NULL, 0, 50);
	libusb_control_transfer(devc->usb->devhdl, LIBUSB_REQUEST_TYPE_VENDOR,
			0x02, 0x0002, 0x00, NULL, 0, 50);
	libusb_control_transfer(devc->usb->devhdl, LIBUSB_REQUEST_TYPE_VENDOR,
			0x02, 0x0001, 0x00, NULL, 0, 50);


	/* Flush input. The F321 requires this. */
	while (libusb_bulk_transfer(devc->usb->devhdl, LASCAR_EP_IN, resp,
			256, &ret, 5) == 0 && ret > 0)
		;

	libusb_fill_bulk_transfer(xfer_in, devc->usb->devhdl, LASCAR_EP_IN,
			resp, sizeof(resp), mark_xfer, 0, 10000);
	if (libusb_submit_transfer(xfer_in) != 0) {
		libusb_free_transfer(xfer_in);
		libusb_free_transfer(xfer_out);
		return SR_ERR;
	}

	cmd[0] = 0x03;
	cmd[1] = 0xff;
	cmd[2] = 0xff;
	libusb_fill_bulk_transfer(xfer_out, devc->usb->devhdl, LASCAR_EP_OUT,
			cmd, 3, mark_xfer, 0, 100);
	if (libusb_submit_transfer(xfer_out) != 0) {
		libusb_free_transfer(xfer_in);
		libusb_free_transfer(xfer_out);
		return SR_ERR;
	}

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	while (!xfer_in->user_data || !xfer_out->user_data) {
		g_usleep(5000);
		libusb_handle_events_timeout(drvc->sr_ctx->libusb_ctx, &tv);
	}
	if (xfer_in->user_data != GINT_TO_POINTER(1) ||
			xfer_in->user_data != GINT_TO_POINTER(1)) {
		sr_dbg("no response to log transfer request");
		libusb_free_transfer(xfer_in);
		libusb_free_transfer(xfer_out);
		return SR_ERR;
	}
	if (xfer_in->actual_length != 3 || xfer_in->buffer[0] != 2) {
		sr_dbg("invalid response to log transfer request");
		libusb_free_transfer(xfer_in);
		libusb_free_transfer(xfer_out);
		return SR_ERR;
	}
	devc->log_size = xfer_in->buffer[1] + (xfer_in->buffer[2] << 8);
	libusb_free_transfer(xfer_out);

	pfd = libusb_get_pollfds(drvc->sr_ctx->libusb_ctx);
	for (i = 0; pfd[i]; i++) {
		/* Handle USB events every 100ms, for decent latency. */
		sr_source_add(pfd[i]->fd, pfd[i]->events, 100,
				lascar_el_usb_handle_events, (void *)sdi);
		/* We'll need to remove this fd later. */
		devc->usbfd[i] = pfd[i]->fd;
	}
	devc->usbfd[i] = -1;

	buf = g_try_malloc(4096);
	libusb_fill_bulk_transfer(xfer_in, devc->usb->devhdl, LASCAR_EP_IN,
			buf, 4096, lascar_el_usb_receive_transfer, cb_data, 100);
	if ((ret = libusb_submit_transfer(xfer_in) != 0)) {
		sr_err("Unable to submit transfer: %s.", libusb_error_name(ret));
		libusb_free_transfer(xfer_in);
		g_free(buf);
		return SR_ERR;
	}

	return SR_OK;
}

SR_PRIV int hw_dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;

	if (!di->priv) {
		sr_err("Driver was not initialized.");
		return SR_ERR;
	}

	if (sdi->status != SR_ST_ACTIVE) {
		sr_err("Device inactive, can't stop acquisition.");
		return SR_ERR;
	}

	sdi->status = SR_ST_STOPPING;
	/* TODO: free ongoing transfers? */

	return SR_OK;
}

SR_PRIV struct sr_dev_driver lascar_el_usb_driver_info = {
	.name = "lascar-el-usb",
	.longname = "Lascar EL-USB",
	.api_version = 1,
	.init = hw_init,
	.cleanup = hw_cleanup,
	.scan = hw_scan,
	.dev_list = hw_dev_list,
	.dev_clear = clear_instances,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = hw_dev_open,
	.dev_close = hw_dev_close,
	.dev_acquisition_start = hw_dev_acquisition_start,
	.dev_acquisition_stop = hw_dev_acquisition_stop,
	.priv = NULL,
};
