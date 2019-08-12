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

#ifndef LIBSIGROK_HARDWARE_HP_16500B_PROTOCOL_H
#define LIBSIGROK_HARDWARE_HP_16500B_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <string.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "hp-16500b"
#define MAX_CARDSLOTS 					10
#define NUM_HORIZONTAL_DIVS 		10
#define NUM_VERTICAL_DIVS 			4
#define HP_CARD_NO_INFO					{-1, -1, -1, HP16500_NO_SAMPLE_RATE}

enum hp_16500b_cardtype {
	HP16500_NOCARD = -1,
	HP16500_16515A = 1,
	HP16500_16516A = 2,
	HP16500_16530A = 11,
	HP16500_16531A = 12,
	HP16500_16532A = 13,
	HP16500_16533A = 14,
	HP16500_16534A = 14,
	HP16500_16520A = 21,
	HP16500_16521A = 22,
	HP16500_16511B = 30,
	HP16500_16510A = 31,
	HP16500_16510B = 31,
	HP16500_16550A_MASTER = 32,
	HP16500_16550A_EXPANSION = 33,
	HP16500_16555A = 34,
	HP16500_16540A = 40,
	HP16500_16541A = 41,
	HP16500_16542A_MASTER = 42,
	HP16500_16542A_EXPANSION = 43
};

enum hp_16500b_err {
	/* Device Dependent Errors */
	
	HP16500_LABEL_NOT_FOUND = 200,
	HP16500_PATTERN_STRING_NOT_FOUND = 201,
	HP16500_QUALIFIER_INVALID = 202,
	HP16500_DATA_NOT_AVAILABLE = 203,
	HP16500_RS_232C_ERROR = 300,
	
	/* Command Errors */
	
	HP16500_COMMAND_ERROR = -100,
	HP16500_INVALID_CHAR_RECEIVED = -101,
	HP16500_COMMAND_HEADER_ERROR = -110,
	HP16500_HEADER_DELIM_ERROR = -111,
	HP16500_NUMERIC_ARG_ERROR = -120,
	HP16500_WRONG_DATA_TYPE_NUM_EXPECTED = -121,
	HP16500_NUMERIC_OVERFLOW = -123,
	HP16500_MISSING_NUMERIC_ARG = -129,
	HP16500_NON_NUMERIC_ARG_ERROR = -130,
	HP16500_WRONG_DATA_TYPE_CHAR_EXPECTED = -131,
	HP16500_WRONG_DATA_TYPE_STR_EXPECTED = -132,
	HP16500_WRONG_DATA_TYPE_BLK_TYPE_REQUIRED = -133,
	HP16500_DATA_OVERFLOW = -134,
	HP16500_MISSING_NON_NUMERIC_ARG = -139,
	HP16500_TOO_MANY_ARGS = -142,
	HP16500_ARG_DELIMITER_ERROR = -143,
	HP16500_INVALID_MESSAGE_UNIT_DELIMITER = -144,
	
	/* Execution Errors */
	HP16500_CAN_NOT_DO = -200,
	HP16500_NOT_EXECUTABLE_IN_LOCAL_MODE = -201,
	HP16500_SETTINGS_LOST = -202,
	HP16500_TRIGGER_IGNORED = -203,
	HP16500_LEGAL_COMMAND_SETTINGS_CONFLICT = -211,
	HP16500_ARG_OUT_OF_RANGE = -212,
	HP16500_BUSY_DOING_SOMETHING_ELSE = -221,
	HP16500_INSUFFICIENT_CAPABILITY = -222,
	HP16500_OUTPUT_BUFFER_FULL_OVERFLOW = -232,
	HP16500_MASS_MEMORY_ERROR = -240,
	HP16500_MASS_STORAGE_DEVICE_NOT_PRESENT = -241,
	HP16500_NO_MEDIA = -242,
	HP16500_BAD_MEDIA = -243,
	HP16500_MEDIA_FULL = -244,
	HP16500_DIRECTORY_FULL = -245,
	HP16500_FILENAME_NOT_FOUND = -246,
	HP16500_DUPLICATE_FILENAME = -247,
	HP16500_MEDIA_PROTECTED = -248,
	
	/* Internal Errors */
	HP16500_DEVICE_FAILURE = -300,
	HP16500_INTERRUPT_FAULT = -301,
	HP16500_SYSTEM_ERROR = -302,
	HP16500_TIME_OUT = -303,
	HP16500_RAM_ERROR = -310,
	HP16500_RAM_FAILURE = -311,
	HP16500_RAM_DATA_LOSS = -312,
	HP16500_CALIBRATION_DATA_LOSS = -313,
	HP16500_ROM_ERROR = -320,
	HP16500_ROM_CHECKSUM = -321,
	HP16500_HARDWARE_FIRMWARE_INCOMPATIBLE = -322,
	HP16500_POST_FAIL = -330,
	HP16500_SELF_TEST_FAIL = -340,
	HP16500_TOO_MANY_ERRORS_OVERFLOW = -350,
	
	/* Query Errors */
	HP16500_QUERY_ERROR = -400,
	HP16500_QUERY_INTERRUPTED = -410,
	HP16500_QUERY_UNTERMINATED = -420,
	HP16500_QUERY_RECEIVED_INDEFINITE_RESPONSE = -421,
	HP16500_ADDRESSED_TO_TALK_NOTHING_TO_SAY = -422,
	HP16500_QUERY_DEADLOCKED = -430
};

enum hp_16500b_sample_depth {
	HP16500_NO_SAMPLE_RATE = 0,
	HP16500_4K = 4096,
	HP16500_8K = 8192,
	HP16500_16K = 16384,
	HP16500_32K = 32768,
	HP16500_64K = 65536,
	HP16500_128K = 131072,
	HP16500_256K = 262144,
	HP16500_512K = 524288,
	HP16500_1M = 1040384
};

struct hp_16500b_supported_card {
	int num_channel_groups;
	int num_channels_per_group;
	int channel_type;
	enum hp_16500b_sample_depth supported_max_sample_rate;
	const char* name;
};

static const struct hp_16500b_supported_card supported_cards[] = {
	HP_CARD_NO_INFO,
	{2, 9, SR_CHANNEL_LOGIC, HP16500_NO_SAMPLE_RATE, "16515A"}, /* HP 16515A */
	{2, 9, SR_CHANNEL_LOGIC, HP16500_NO_SAMPLE_RATE, "16516A"}, /* HP 16516A */
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,

	HP_CARD_NO_INFO, /* HP 16530A, not supported */
	HP_CARD_NO_INFO, /* HP 16531A, not supported */
	{2, 1, SR_CHANNEL_ANALOG, HP16500_8K, "16532A"}, /* HP 16532A, 2CH, 250 MHz, 8K Sample Length */
	{2, 1, SR_CHANNEL_ANALOG, HP16500_32K, "16533/4A"}, /* HP 16533A/16534A, 2CH, 250/500 MHz, 32K Sample Length */
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO, /* HP 16520A, Pattern Generator, not supported */
	HP_CARD_NO_INFO, /* HP 16521A, Pattern Generator Expansion, not supported */
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO, /* HP 16511B, not supported */
	HP_CARD_NO_INFO, /* HP 16510A/B, not supported */
	{6, 17, SR_CHANNEL_LOGIC, HP16500_8K, "16550A_Master"}, /* HP 16550A Master */
	{6, 17, SR_CHANNEL_LOGIC, HP16500_NO_SAMPLE_RATE, "16550A_Expansion"}, /* HP 16550A Expansion */
	{4, 17, SR_CHANNEL_LOGIC, HP16500_1M, "16555A"}, /* HP 16555A */
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	HP_CARD_NO_INFO,
	{2, 9, SR_CHANNEL_LOGIC, HP16500_4K, "16540A"}, /* HP 16540A Unsure of sample rate, so default for now of max 4K*/
	{1, 50, SR_CHANNEL_LOGIC, HP16500_4K, "16541A"}, /* HP 16541A */
	HP_CARD_NO_INFO, /* HP 16542A Master*/
	HP_CARD_NO_INFO, /* HP 16542A Expansion*/
};

struct hp_16500b_card {
	enum hp_16500b_cardtype type;
	struct hp_16500b_card* card_master; /* Controlling card, can be self. */
};

struct hp_16500b_cardcage {
	struct hp_16500b_card slot0;
	struct hp_16500b_card slot1;
	struct hp_16500b_card slot2;
	struct hp_16500b_card slot3;
	struct hp_16500b_card slot4;
	/* Only possible with the expansion frame */
	struct hp_16500b_card slot5;
	struct hp_16500b_card slot6;
	struct hp_16500b_card slot7;
	struct hp_16500b_card slot8;
	struct hp_16500b_card slot9;
};

static const uint64_t samplerates[23] = {
	SR_HZ(100),
	SR_GHZ(2),
	SR_HZ(1),
};

static const uint64_t sample_rate_table[][2] = {
	/* picoseconds/div, sample_rate per second */
	{500,  2000000000},
	{1000,  2000000000},
	{2000,  2000000000},
	{5000,  2000000000},
	{10000, 2000000000},
	{20000, 2000000000},
	{50000, 2000000000},
	{100000, 2000000000},
	{200000, 2000000000},
	{500000, 1000000000},
	{1000000, 500000000},
	{2000000, 250000000},
	{5000000, 100000000},
	{10000000, 50000000},
	{20000000, 25000000},
	{50000000, 10000000},
	{100000000, 5000000},
	{200000000, 2500000},
	{500000000, 1000000},
	{1000000000, 500000},
	{2000000000, 250000},
	{5000000000, 100000},
	{10000000000, 50000},
	{20000000000, 25000},
	{50000000000, 10000},
	{100000000000, 5000},
	{200000000000, 2500},
	{500000000000, 1000},
	{1000000000000, 500},
	{2000000000000, 250},
	{5000000000000, 100},
};

struct dev_context {
	union {
		struct hp_16500b_cardcage cage;
		struct hp_16500b_card cards[MAX_CARDSLOTS];
	};
	unsigned int num_channels; // Number of channels attached to the device.
	
	uint64_t seconds_per_div_ps;
	uint64_t limit_frames;
	uint64_t average_samples;
	char *trigger_source;
	char *trigger_slope;
	double trigger_level;
	uint64_t cur_samplerate;
	uint64_t capture_ratio;
	uint64_t limit_samples;
	
	/* Device properties */
	const uint64_t (*timebases)[2];
	uint64_t num_timebases;
	gboolean avg;
};

SR_PRIV int hp_16500b_request_id(struct sr_serial_dev_inst *serial, GString *resp);
SR_PRIV int hp_16500b_send_command(struct sr_serial_dev_inst *serial, GString *cmd);
SR_PRIV int hp_16500b_read(struct sr_serial_dev_inst *serial, GString *data);
SR_PRIV int hp_16500b_send_command_then_read(struct sr_serial_dev_inst *serial, char *cmd, GString *data);
SR_PRIV struct sr_dev_inst* hp_16500b_get_metadata(struct sr_serial_dev_inst *serial);
SR_PRIV int hp_16500b_receive_data(int fd, int revents, void *cb_data);
SR_PRIV int hp_16500b_process_cardcage(struct sr_serial_dev_inst *serial, struct hp_16500b_card *cards);
SR_PRIV int hp_16500b_process_card(struct sr_dev_inst *sdi, struct hp_16500b_card *card);
SR_PRIV enum hp_16500b_sample_depth hp_16500b_card_get_max_sample_depth(struct hp_16500b_card *card);
SR_PRIV enum hp_16500b_sample_depth hp_16500b_get_max_sample_count(struct sr_dev_inst *sdi);
SR_PRIV const char* hp_16500b_lookup_cardname(enum hp_16500b_cardtype cardtype);
SR_PRIV char hp_16500b_get_next_channel_letter(void);
#endif
