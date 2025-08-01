/*
 * armagetron.c
 *
 * Copyright (C) 2009-11 - ipoque GmbH
 * Copyright (C) 2011-25 - ntop.org
 *
 * This file is part of nDPI, an open source deep packet inspection
 * library based on the OpenDPI and PACE technology by ipoque GmbH
 *
 * nDPI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nDPI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with nDPI.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ndpi_protocol_ids.h"

#define NDPI_CURRENT_PROTO NDPI_PROTOCOL_ARMAGETRON

#include "ndpi_api.h"
#include "ndpi_private.h"


static void ndpi_int_armagetron_add_connection(struct ndpi_detection_module_struct *ndpi_struct,
					       struct ndpi_flow_struct *flow)
{
  ndpi_set_detected_protocol(ndpi_struct, flow, NDPI_PROTOCOL_ARMAGETRON, NDPI_PROTOCOL_UNKNOWN, NDPI_CONFIDENCE_DPI);
}

static void ndpi_search_armagetron_udp(struct ndpi_detection_module_struct *ndpi_struct, struct ndpi_flow_struct *flow)
{
  struct ndpi_packet_struct *packet = ndpi_get_packet_struct(ndpi_struct);

  NDPI_LOG_DBG(ndpi_struct, "search armagetron\n");

  if (packet->payload_packet_len >= 8) {
    /* login request */
    if (get_u_int32_t(packet->payload, 0) == htonl(0x000b0000)) {
      const u_int16_t dataLength = ntohs(get_u_int16_t(packet->payload, 4));
      if (dataLength * 2 + 8 == packet->payload_packet_len &&
          get_u_int16_t(packet->payload, packet->payload_packet_len - 2) == 0) {
	NDPI_LOG_INFO(ndpi_struct, "found armagetron\n");
	ndpi_int_armagetron_add_connection(ndpi_struct, flow);
	return;
      }
    }
    /* big_server/big_request */
    if (get_u_int32_t(packet->payload, 0) == htonl(0x00330000) ||
        get_u_int32_t(packet->payload, 0) == htonl(0x00350000)) {
      const u_int16_t dataLength = ntohs(get_u_int16_t(packet->payload, 4));
      if (dataLength * 2 + 8 == packet->payload_packet_len &&
          get_u_int16_t(packet->payload, packet->payload_packet_len - 2) == 0) {
	NDPI_LOG_INFO(ndpi_struct, "found armagetron\n");
	ndpi_int_armagetron_add_connection(ndpi_struct, flow);
	return;
      }
    }
  }

  NDPI_EXCLUDE_DISSECTOR(ndpi_struct, flow);
}



void init_armagetron_dissector(struct ndpi_detection_module_struct *ndpi_struct)
{
  register_dissector("Armagetron", ndpi_struct,
                     ndpi_search_armagetron_udp,
                     NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_UDP_WITH_PAYLOAD,
                     1, NDPI_PROTOCOL_ARMAGETRON);
}
