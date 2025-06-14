/*
 * openwire.c
 *
 * Copyright (C) 2024 - ntop.org
 * Copyright (C) 2024 - V.G <v.gavrilov@securitycode.ru>
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

#define NDPI_CURRENT_PROTO NDPI_PROTOCOL_OPENWIRE

#include "ndpi_api.h"
#include "ndpi_private.h"

static void ndpi_search_openwire(struct ndpi_detection_module_struct *ndpi_struct, struct ndpi_flow_struct *flow)
{
  struct ndpi_packet_struct const * const packet = ndpi_get_packet_struct(ndpi_struct);

  NDPI_LOG_DBG(ndpi_struct, "search OpenWire\n");

  if (packet->payload_packet_len > 22 &&
      ntohl(get_u_int32_t(packet->payload, 0)) == (u_int32_t)(packet->payload_packet_len-4) &&
      packet->payload[4] == 0x01)
  {
    if (memcmp(&packet->payload[5], "ActiveMQ", 8) == 0) {
      NDPI_LOG_INFO(ndpi_struct, "found OpenWire\n");
      ndpi_set_detected_protocol(ndpi_struct, flow, NDPI_PROTOCOL_OPENWIRE,
                                 NDPI_PROTOCOL_UNKNOWN, NDPI_CONFIDENCE_DPI);
      return;
    }
  }

  NDPI_EXCLUDE_DISSECTOR(ndpi_struct, flow);
}

void init_openwire_dissector(struct ndpi_detection_module_struct *ndpi_struct)
{
  register_dissector("OpenWire", ndpi_struct,
                     ndpi_search_openwire,
                     NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_WITH_PAYLOAD_WITHOUT_RETRANSMISSION,
                     1, NDPI_PROTOCOL_OPENWIRE);
}
