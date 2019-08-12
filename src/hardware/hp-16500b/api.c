/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2018 Kevin Chau <chau.kevin101@gmail.com>
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

#include <config.h>
#include <math.h>
#include "protocol.h"

#define SERIALCOMM "9600/8n1"
#define DRIVERNAME "hp-16500b"
#define IDN_STRING "HEWLETT PACKARD,16500B"

static const char *logic_pattern_str[] = {
	"sigrok",
	"random",
	"incremental",
	"walking-one",
	"walking-zero",
	"all-low",
	"all-high",
	"squid",
	"graycode",
};

static const char *analog_pattern_str[] = {
	"square",
	"sine",
	"triangle",
	"sawtooth",
};

static const uint32_t scanopts[] = {
	SR_CONF_NUM_LOGIC_CHANNELS,
	SR_CONF_NUM_ANALOG_CHANNELS,
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
	SR_CONF_OSCILLOSCOPE,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_AVERAGING | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_AVG_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_FRAMES | SR_CONF_SET,
	//	SR_CONF_TIMEBASE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_NUM_HDIV | SR_CONF_GET,
	SR_CONF_HORIZ_TRIGGERPOS | SR_CONF_SET,
	SR_CONF_TRIGGER_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_SLOPE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_LEVEL | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_DATA_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static const uint32_t devopts_cg_logic[] = {
	SR_CONF_PATTERN_MODE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static const uint32_t devopts_cg_analog_group[] = {
	SR_CONF_NUM_VDIV | SR_CONF_GET,
	SR_CONF_VDIV | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_COUPLING | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_PROBE_FACTOR | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static const uint32_t devopts_cg_analog_channel[] = {
	SR_CONF_PATTERN_MODE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_AMPLITUDE | SR_CONF_GET | SR_CONF_SET,
};

static const char *coupling[] = {
	"AC", "DC", "GND",
};

static const uint64_t vdivs[][2] = {
	/* microvolts */
	{ 500, 1000000 },
	/* millivolts */
	{ 1, 1000 },
	{ 2, 1000 },
	{ 5, 1000 },
	{ 10, 1000 },
	{ 20, 1000 },
	{ 50, 1000 },
	{ 100, 1000 },
	{ 200, 1000 },
	{ 500, 1000 },
	/* volts */
	{ 1, 1 },
	{ 2, 1 },
	{ 5, 1 },
	{ 10, 1 },
	{ 20, 1 },
	{ 50, 1 },
	{ 100, 1 },
};

static const uint64_t probe_factor[] = {
	1, 2, 5, 10, 20, 50, 100, 200, 500, 1000,
};

static const int32_t trigger_matches[] = {
	SR_TRIGGER_ZERO,
	SR_TRIGGER_ONE,
	SR_TRIGGER_RISING,
	SR_TRIGGER_FALLING,
	SR_TRIGGER_EDGE,
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct sr_config *src;
	struct drv_context *drvc;
	const char *conn, *serialcomm;
	struct sr_serial_dev_inst *serial;
	struct sr_dev_inst *sdi;
	GSList *l;
	int ret;

	(void)options;

	sr_info("%s: Start of scan.\n", __func__);
	
	conn = serialcomm = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return NULL;

	if (!serialcomm)
		serialcomm = SERIALCOMM;

	serial = sr_serial_dev_inst_new(conn, serialcomm);

	drvc = di->context;
	drvc->instances = NULL;

	/* TODO: scan for devices, either based on a SR_CONF_CONN option
	 * or on a USB scan. */
	 
	sr_info("Probing %s", conn);
	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;
		
	/* Send the *IDN? code and parse to see if we have an HP16500B 
	 * The return string should be similar to:
	 * HEWLETT PACKARD,16500B,0,REV 01.00
	 */
	GString *resp = g_string_sized_new(33);
	ret = hp_16500b_request_id(serial, resp);
	if (ret != SR_OK)
	{
		sr_info("%s: Request_id reported: %d\n", __func__, ret);
		sr_err("Problem sending serial data.");
		goto err_cleanup;
	}
	
	if (strncmp(resp->str, IDN_STRING, 22) != 0)
	{
		sr_info("Received: %s\n", resp->str);
		sr_info("Expected: %s\n", IDN_STRING);
		sr_err("Response String did not match.");
		goto err_cleanup;
	}
	
	sdi = hp_16500b_get_metadata(serial);

	g_string_free(resp, TRUE);
	return std_scan_complete(di, g_slist_append(NULL, sdi));
	
	err_cleanup:
		g_string_free(resp, TRUE);
		return NULL;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	struct sr_channel *ch;
	struct analog_gen *ag;
	int pattern;
	char *tmp_str;
	float smallest_diff = INFINITY;
	int idx = -1;
	unsigned i;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;
	switch (key) {
	case SR_CONF_CAPTURE_RATIO:
		*data = g_variant_new_uint64(devc->capture_ratio);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case SR_CONF_NUM_HDIV:
		*data = g_variant_new_int32(NUM_HORIZONTAL_DIVS);
		break;
	case SR_CONF_NUM_VDIV:
		*data = g_variant_new_int32(NUM_VERTICAL_DIVS);
		break;
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(sample_rate_table[devc->seconds_per_div_ps]);
		break;
	case SR_CONF_TRIGGER_SOURCE:
		/* Check trigger Mode is Edge, could be CH1, CH2, or EXT */
		/* If trigger mode is pattern, trigger source is ALL (CH1,CH2, EXT) */
		if (!strcmp(devc->trigger_source, "CHAN1"))
			tmp_str = "CH1";
		else if (!strcmp(devc->trigger_source, "CHAN2"))
			tmp_str = "CH2";
		else if (!strcmp(devc->trigger_source, "CHAN3"))
			tmp_str = "CH3";
		else if (!strcmp(devc->trigger_source, "CHAN4"))
			tmp_str = "CH4";
		else
			tmp_str = devc->trigger_source;
		*data = g_variant_new_string(tmp_str);
		break;
	case SR_CONF_TRIGGER_SLOPE:
		if (!strncmp(devc->trigger_slope, "POS", 3)) {
			tmp_str = "r";
		} else if (!strncmp(devc->trigger_slope, "NEG", 3)) {
			tmp_str = "f";
		} else {
			sr_dbg("Unknown trigger slope: '%s'.", devc->trigger_slope);
			return SR_ERR_NA;
		}
		*data = g_variant_new_string(tmp_str);
		break;
	}

	sr_info("%s: Couldn't find matching key: %d", __func__, key);
	return SR_ERR_NA;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;

	struct dev_context *devc;
	(void)sdi;
	(void)data;
	uint64_t tmp_u64;
	(void)cg;

	devc = sdi->priv;
	ret = SR_OK;
	switch (key) {
	/* TODO */
	case SR_CONF_SAMPLERATE:
		devc->cur_samplerate = g_variant_get_uint64(data);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		tmp_u64 = g_variant_get_uint64(data);
		devc->limit_samples = tmp_u64;
		break;
	default:
		ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	
	struct sr_channel *ch;
	struct dev_context *devc;

	if (!cg) {
		switch (key) {
		case SR_CONF_SCAN_OPTIONS:
		case SR_CONF_DEVICE_OPTIONS:
			return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
		case SR_CONF_SAMPLERATE:
			*data = std_gvar_samplerates_steps(ARRAY_AND_SIZE(samplerates));
			break;
		case SR_CONF_TRIGGER_MATCH:
			*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
			break;
		case SR_CONF_LIMIT_SAMPLES:
			*data = std_gvar_tuple_u64(HP16500_4K,
			hp_16500b_get_max_sample_count(sdi));
			break;
		case SR_CONF_TIMEBASE:
			if (devc->num_timebases <= 0)
			{
				sr_info("No timebases!");
				return SR_ERR_NA;
			}
			*data = std_gvar_tuple_array(devc->timebases, devc->num_timebases);
			break;
		default:
			return SR_ERR_NA;
		}
	/* Special case for channel groups */
	} else {
		sr_info("CHGRPNAME: %s", cg->name);
		sr_info("%d", &cg->channels);
		sr_info("%d", &cg->channels->data);
		ch = cg->channels->data;
		switch (key) {
		case SR_CONF_DEVICE_OPTIONS:
			if (ch->type == SR_CHANNEL_LOGIC)
				*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_logic));
			else if (ch->type == SR_CHANNEL_ANALOG) {
				if (strcmp(cg->name, "Analog") == 0)
					*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_analog_group));
				else
					*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_analog_channel));
			}
			else
				return SR_ERR_BUG;
			break;
		case SR_CONF_PATTERN_MODE:
			/* The analog group (with all 4 channels) shall not have a pattern property. */
			if (strcmp(cg->name, "Analog") == 0)
				return SR_ERR_NA;

			if (ch->type == SR_CHANNEL_LOGIC)
				*data = g_variant_new_strv(ARRAY_AND_SIZE(logic_pattern_str));
			else if (ch->type == SR_CHANNEL_ANALOG)
				*data = g_variant_new_strv(ARRAY_AND_SIZE(analog_pattern_str));
			else
				return SR_ERR_BUG;
			break;
		
		default:
			return SR_ERR_NA;
		}
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	/* TODO: configure hardware, reset acquisition state, set up
	 * callbacks and send header packet. */

	(void)sdi;

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	/* TODO: stop acquisition. */

	(void)sdi;

	return SR_OK;
}

SR_PRIV struct sr_dev_driver hp_16500b_driver_info = {
	.name = "hp-16500b",
	.longname = "HP 16500B Logic Analysis Mainframe",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};

SR_REGISTER_DEV_DRIVER(hp_16500b_driver_info);
