/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2015  Andrzej Kaczmarek <andrzej.kaczmarek@codecoup.pl>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/bluetooth.h"

#include "src/shared/util.h"
#include "bt.h"
#include "packet.h"
#include "display.h"
#include "l2cap.h"
#include "avdtp.h"

/* Message Types */
#define AVDTP_MSG_TYPE_COMMAND		0x00
#define AVDTP_MSG_TYPE_GENERAL_REJECT	0x01
#define AVDTP_MSG_TYPE_RESPONSE_ACCEPT	0x02
#define AVDTP_MSG_TYPE_RESPONSE_REJECT	0x03

/* Signal Identifiers */
#define AVDTP_DISCOVER			0x01
#define AVDTP_GET_CAPABILITIES		0x02
#define AVDTP_SET_CONFIGURATION		0x03
#define AVDTP_GET_CONFIGURATION		0x04
#define AVDTP_RECONFIGURE		0x05
#define AVDTP_OPEN			0x06
#define AVDTP_START			0x07
#define AVDTP_CLOSE			0x08
#define AVDTP_SUSPEND			0x09
#define AVDTP_ABORT			0x0a
#define AVDTP_SECURITY_CONTROL		0x0b
#define AVDTP_GET_ALL_CAPABILITIES	0x0c
#define AVDTP_DELAYREPORT		0x0d

struct avdtp_frame {
	uint8_t hdr;
	uint8_t sig_id;
	struct l2cap_frame l2cap_frame;
};

static const char *msgtype2str(uint8_t msgtype)
{
	switch (msgtype) {
	case 0:
		return "Command";
	case 1:
		return "General Reject";
	case 2:
		return "Response Accept";
	case 3:
		return "Response Reject";
	}

	return "";
}

static const char *sigid2str(uint8_t sigid)
{
	switch (sigid) {
	case AVDTP_DISCOVER:
		return "Discover";
	case AVDTP_GET_CAPABILITIES:
		return "Get Capabilities";
	case AVDTP_SET_CONFIGURATION:
		return "Set Configuration";
	case AVDTP_GET_CONFIGURATION:
		return "Get Configuration";
	case AVDTP_RECONFIGURE:
		return "Reconfigure";
	case AVDTP_OPEN:
		return "Open";
	case AVDTP_START:
		return "Start";
	case AVDTP_CLOSE:
		return "Close";
	case AVDTP_SUSPEND:
		return "Suspend";
	case AVDTP_ABORT:
		return "Abort";
	case AVDTP_SECURITY_CONTROL:
		return "Security Control";
	case AVDTP_GET_ALL_CAPABILITIES:
		return "Get All Capabilities";
	case AVDTP_DELAYREPORT:
		return "Delay Report";
	default:
		return "Reserved";
	}
}

static const char *error2str(uint8_t error)
{
	switch (error) {
	case 0x01:
		return "BAD_HEADER_FORMAT";
	case 0x11:
		return "BAD_LENGTH";
	case 0x12:
		return "BAD_ACP_SEID";
	case 0x13:
		return "SEP_IN_USE";
	case 0x14:
		return "SEP_NOT_IN_USER";
	case 0x17:
		return "BAD_SERV_CATEGORY";
	case 0x18:
		return "BAD_PAYLOAD_FORMAT";
	case 0x19:
		return "NOT_SUPPORTED_COMMAND";
	case 0x1a:
		return "INVALID_CAPABILITIES";
	case 0x22:
		return "BAD_RECOVERY_TYPE";
	case 0x23:
		return "BAD_MEDIA_TRANSPORT_FORMAT";
	case 0x25:
		return "BAD_RECOVERY_FORMAT";
	case 0x26:
		return "BAD_ROHC_FORMAT";
	case 0x27:
		return "BAD_CP_FORMAT";
	case 0x28:
		return "BAD_MULTIPLEXING_FORMAT";
	case 0x29:
		return "UNSUPPORTED_CONFIGURATION";
	case 0x31:
		return "BAD_STATE";
	default:
		return "Unknown";
	}
}

static const char *mediatype2str(uint8_t media_type)
{
	switch (media_type) {
	case 0x00:
		return "Audio";
	case 0x01:
		return "Video";
	case 0x02:
		return "Multimedia";
	default:
		return "Reserved";
	}
}

static bool avdtp_reject_common(struct avdtp_frame *avdtp_frame)
{
	struct l2cap_frame *frame = &avdtp_frame->l2cap_frame;
	uint8_t error;

	if (!l2cap_frame_get_u8(frame, &error))
		return false;

	print_field("Error code: %s (0x%02x)", error2str(error), error);

	return true;
}

static bool avdtp_discover(struct avdtp_frame *avdtp_frame)
{
	struct l2cap_frame *frame = &avdtp_frame->l2cap_frame;
	uint8_t type = avdtp_frame->hdr & 0x03;
	uint8_t seid;
	uint8_t info;

	switch (type) {
	case AVDTP_MSG_TYPE_COMMAND:
		return true;
	case AVDTP_MSG_TYPE_RESPONSE_ACCEPT:
		while (l2cap_frame_get_u8(frame, &seid)) {
			print_field("ACP SEID: %d", seid >> 2);

			if (!l2cap_frame_get_u8(frame, &info))
				return false;

			print_field("%*cMedia Type: %s (0x%02x)", 2, ' ',
					mediatype2str(info >> 4), info >> 4);
			print_field("%*cSEP Type: %s (0x%02x)", 2, ' ',
						info & 0x04 ? "SNK" : "SRC",
						(info >> 3) & 0x01);
			print_field("%*cIn use: %s", 2, ' ',
						seid & 0x02 ? "Yes" : "No");
		}
		return true;
	case AVDTP_MSG_TYPE_RESPONSE_REJECT:
		return avdtp_reject_common(avdtp_frame);
	}

	return false;
}

static bool avdtp_signalling_packet(struct avdtp_frame *avdtp_frame)
{
	struct l2cap_frame *frame = &avdtp_frame->l2cap_frame;
	const char *pdu_color;
	uint8_t hdr;
	uint8_t sig_id;
	uint8_t nosp = 0;

	if (frame->in)
		pdu_color = COLOR_MAGENTA;
	else
		pdu_color = COLOR_BLUE;

	if (!l2cap_frame_get_u8(frame, &hdr))
		return false;

	avdtp_frame->hdr = hdr;

	/* Continue Packet || End Packet */
	if (((hdr & 0x0c) == 0x08) || ((hdr & 0x0c) == 0x0c)) {
		/* TODO: handle fragmentation */
		packet_hexdump(frame->data, frame->size);
		return true;
	}

	/* Start Packet */
	if ((hdr & 0x0c) == 0x04) {
		if (!l2cap_frame_get_u8(frame, &nosp))
			return false;
	}

	if (!l2cap_frame_get_u8(frame, &sig_id))
		return false;

	sig_id &= 0x3f;

	avdtp_frame->sig_id = sig_id;

	print_indent(6, pdu_color, "AVDTP: ", sigid2str(sig_id), COLOR_OFF,
			" (0x%02x) %s (0x%02x) type 0x%02x label %d nosp %d",
			sig_id, msgtype2str(hdr & 0x03), hdr & 0x03,
			hdr & 0x0c, hdr >> 4, nosp);

	/* Start Packet */
	if ((hdr & 0x0c) == 0x04) {
		/* TODO: handle fragmentation */
		packet_hexdump(frame->data, frame->size);
		return true;
	}

	/* General Reject */
	if ((hdr & 0x03) == 0x03)
		return true;

	switch (sig_id) {
	case AVDTP_DISCOVER:
		return avdtp_discover(avdtp_frame);
	}

	packet_hexdump(frame->data, frame->size);

	return true;
}

void avdtp_packet(const struct l2cap_frame *frame)
{
	struct avdtp_frame avdtp_frame;
	bool ret;

	l2cap_frame_pull(&avdtp_frame.l2cap_frame, frame, 0);

	switch (frame->seq_num) {
	case 1:
		ret = avdtp_signalling_packet(&avdtp_frame);
		break;
	default:
		packet_hexdump(frame->data, frame->size);
		return;
	}

	if (!ret) {
		print_text(COLOR_ERROR, "PDU malformed");
		packet_hexdump(frame->data, frame->size);
	}
}
