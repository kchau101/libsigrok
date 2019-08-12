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
#include "protocol.h"

static const uint64_t timebases[][2] = {
	/* nanoseconds */
	{ 1, 1000000000 },
	{ 2, 1000000000 },
	{ 5, 1000000000 },
	{ 10, 1000000000 },
	{ 20, 1000000000 },
	{ 50, 1000000000 },
	{ 100, 1000000000 },
	{ 500, 1000000000 },
	/* microseconds */
	{ 1, 1000000 },
	{ 2, 1000000 },
	{ 5, 1000000 },
	{ 10, 1000000 },
	{ 20, 1000000 },
	{ 50, 1000000 },
	{ 100, 1000000 },
	{ 200, 1000000 },
	{ 500, 1000000 },
	/* milliseconds */
	{ 1, 1000 },
	{ 2, 1000 },
	{ 5, 1000 },
	{ 10, 1000 },
	{ 20, 1000 },
	{ 50, 1000 },
	{ 100, 1000 },
	{ 200, 1000 },
	{ 500, 1000 },
	/* seconds */
	{ 1, 1 },
	{ 2, 1 },
	{ 5, 1 },
	{ 10, 1 },
	{ 20, 1 },
	{ 50, 1 },
	{ 100, 1 },
	{ 200, 1 },
	{ 500, 1 },
	{ 1000, 1 },
};


SR_PRIV int hp_16500b_send_command(struct sr_serial_dev_inst *serial, GString *cmd)
{
	int ret;
	
	/* Append the LF byte to the command */
	cmd = g_string_append(cmd, "\n");
	sr_dbg("Sending command %s", cmd->str);
	ret = serial_write_blocking(serial, cmd->str, cmd->len, serial_timeout(serial, cmd->len));
	if (ret != cmd->len)
	{
		if (ret < 0) {
			sr_info("%s: Error with sending command.", __func__);
			return SR_ERR;
		}
		else if (ret > 0 | ret < cmd->len)
		{
			sr_info("%s: Timed out while sending command.\n", __func__);
		}
	}
	return SR_OK;
}

SR_PRIV int hp_16500b_read(struct sr_serial_dev_inst *serial, GString *data)
{
	int buflen = (int) data->allocated_len;
	sr_info("%s: buflen: %d", __func__, buflen);
	if (serial_readline(serial, &data->str, &buflen, serial_timeout(serial, buflen*2)) != SR_OK)
	{
		sr_info("%s: Error with reading response.", __func__);
		return SR_ERR;
	}
	data->len = buflen;
	sr_info("%s: GString len: %d\n", __func__, data->len);
	sr_info("%s: Received %s\n", __func__, data->str);
	return SR_OK;
}

SR_PRIV int hp_16500b_send_command_then_read(struct sr_serial_dev_inst *serial, char *cmd, GString *data)
{
	GString *command;
	int ret = SR_OK;
	
	command = g_string_new(cmd);
	while (ret == SR_OK)
	{
		ret |= hp_16500b_send_command(serial, command);
		ret |= hp_16500b_read(serial, data);
		break;
	}
	
	g_string_free(command, TRUE);
	sr_info("%s: Returning %d\n", __func__, ret);
	return ret;
}

SR_PRIV int hp_16500b_request_id(struct sr_serial_dev_inst *serial, GString *resp)
{
	return hp_16500b_send_command_then_read(serial, "*IDN?", resp);
}

SR_PRIV struct sr_dev_inst *hp_16500b_get_metadata(struct sr_serial_dev_inst *serial)
{
	/* This function fetches all of the available info from the
	 * mainframe and parses it into functional information for
	 * the sigrok device instance struct *sdi.
	 */
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct hp_16500b_card *cards;
	/* Set some basic info if we're using the serial interface */
	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->vendor = g_strdup("HEWLETT PACKARD");
	sdi->model = g_strdup("16500B");
	sdi->inst_type = SR_INST_SERIAL;
	sdi->version = g_strdup("1.00"); /* Default to 1.00 */
	devc = g_malloc0(sizeof(struct dev_context));
	devc->num_channels = 0; 
	cards = &devc->cards;
	
	sdi->priv = devc;
	sdi->conn = serial;
	
	hp_16500b_process_cardcage(serial, cards);
	for (int i = 0; i < MAX_CARDSLOTS; i++)
	{
		sr_info("%d: card type: %d", i, cards[i].type);
		hp_16500b_process_card(sdi, &cards[i]);
	}

	devc->timebases = &timebases;
	devc->num_timebases = ARRAY_SIZE(timebases);
	
	return sdi;
}

SR_PRIV enum hp_16500b_sample_depth hp_16500b_card_get_max_sample_depth(struct hp_16500b_card *card)
{
	if(card->type != -1)
		return supported_cards[card->type].supported_max_sample_rate;
	else
		return HP16500_NO_SAMPLE_RATE;
}

SR_PRIV enum hp_16500b_sample_depth hp_16500b_get_max_sample_count(struct sr_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;
	struct hp_16500b_card *cards = devc->cards;
	enum hp_16500b_sample_depth max_sample_depth = HP16500_NO_SAMPLE_RATE;

	for (int i = 0; i < MAX_CARDSLOTS; i++)
	{
		if (max_sample_depth < hp_16500b_card_get_max_sample_depth(&cards[i]))
			max_sample_depth = hp_16500b_card_get_max_sample_depth(&cards[i]);
	}
	return max_sample_depth;
}
SR_PRIV int hp_16500b_process_cardcage(struct sr_serial_dev_inst *serial, struct hp_16500b_card *cards)
{
	/* Get Cardcage info */
	
	GString *card_cage = g_string_sized_new(64);
	hp_16500b_send_command_then_read(serial, ":CARDCAGE?", card_cage);
	
	/* Parse the response and count how many CSVs there are:
	 * 10 - HP16500B Mainframe Only
	 * 20 - HP16500B with HP16501A
	 *
	 * We can have up to 20 values.
	 */

	char* tokens[20];
	char* token;
	int card_master_pos;
	
	int i = 0;
	int ret = SR_OK;
	
	token = strtok(card_cage->str, ",");
	while(i < 20)
	{
		if (token == NULL) {
			break;
		}
		else
		{
			tokens[i] = g_strdup(token);
			token = strtok(NULL, ",");
		}
		i += 1;
	}

	for (int a = 0; a < i/2; a++) {
		ret = sr_atoi(tokens[a], &cards[a].type);
		if (ret != SR_OK)
			return ret;
		ret = sr_atoi(tokens[a + i/2], &card_master_pos);
		if (ret != SR_OK)
			return ret;
		cards[a].card_master = &cards[card_master_pos];
	}
	
	return SR_OK;
}

SR_PRIV int hp_16500b_process_card(struct sr_dev_inst *sdi, struct hp_16500b_card *card)
{
	struct dev_context *devc;
	struct sr_channel *ch;
	struct sr_channel_group *cg;
	GString *ch_name = g_string_new(NULL);
	GString *chgrp_name = g_string_new(NULL);
	char ch_prefix;

	devc = sdi->priv;

	if (supported_cards[card->type].num_channel_groups != -1) {
		ch_prefix = hp_16500b_get_next_channel_letter();
		
		for (int i = 0; i < supported_cards[card->type].num_channel_groups; i++) {
			cg = g_malloc0(sizeof(struct sr_channel_group));
			if (supported_cards[card->type].channel_type == SR_CHANNEL_LOGIC) {
				cg->name = g_strdup_printf("%s_%c%d", 
				 hp_16500b_lookup_cardname(card->type),
				 ch_prefix, i+1);
			}
			else if (supported_cards[card->type].channel_type == SR_CHANNEL_ANALOG) {
				cg->name = g_strdup_printf("%s_CH%d", hp_16500b_lookup_cardname(card->type), i);
			}
			
			else {
				break;
			}

			for (int j = 0; j < supported_cards[card->type].num_channels_per_group; j++) {
				if (supported_cards[card->type].channel_type == SR_CHANNEL_LOGIC) {
					/* Follow HP16500B Naming Convention
					 * Channel Groups (1-indexed): A1, A2, A3, ...
					 * Channels (0-indexed): A1[0], A1[1], ...
					 */
					g_string_printf(ch_name, "%c", ch_prefix);
					g_string_append_printf(ch_name, "%d", i+1);
					g_string_append_printf(ch_name, "\[%d\]", j);
					ch = sr_channel_new(sdi, devc->num_channels++, SR_CHANNEL_LOGIC, TRUE, ch_name->str);
					cg->channels = g_slist_prepend(cg->channels, ch);
				}
				else if (supported_cards[card->type].channel_type == SR_CHANNEL_ANALOG) {
					g_string_printf(ch_name, "CH%d", i);
					ch = sr_channel_new(sdi, devc->num_channels++, 
					 SR_CHANNEL_ANALOG, TRUE, ch_name->str);
					cg->channels = g_slist_prepend(cg->channels, ch);
				}
			}
			
			cg->channels = g_slist_reverse(cg->channels);
			/* Add Channel Grouping */
			sdi->channel_groups = g_slist_append(sdi->channel_groups, cg);
		}
	}
	
	g_string_free(ch_name, TRUE);
	return SR_OK;
}

SR_PRIV int hp_16500b_receive_data(int fd, int revents, void *cb_data)
{
	const struct sr_dev_inst *sdi;
	struct dev_context *devc;

	(void)fd;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	if (revents == G_IO_IN) {
		/* TODO */
	}

	return TRUE;
}

SR_PRIV const char* hp_16500b_lookup_cardname(enum hp_16500b_cardtype cardtype) {
	return supported_cards[cardtype].name;
}

SR_PRIV char hp_16500b_get_next_channel_letter(void) {
	static const char prefix_letters[26] = "ABCDEFGHIJKLMNPQRSTUVWXYZ"; // Skip O avoid confusion with 0.
	static int a = 0;
	
	if (a > 25)
		return NULL;
	else {
		return prefix_letters[a++];
	}
}