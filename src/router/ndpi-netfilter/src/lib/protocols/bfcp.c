/*
 * bfcp.c
 *
 * Binary Floor Control Protocol
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

#define NDPI_CURRENT_PROTO NDPI_PROTOCOL_BFCP

#include "ndpi_api.h"
#include "ndpi_private.h"

static void ndpi_search_bfcp(struct ndpi_detection_module_struct *ndpi_struct,
                             struct ndpi_flow_struct *flow)
{
  struct ndpi_packet_struct const * const packet = ndpi_get_packet_struct(ndpi_struct);

  NDPI_LOG_DBG(ndpi_struct, "search BFCP\n");

  if (packet->payload_packet_len < 12) {
    goto not_bfcp;
  }

  u_int8_t version = (packet->payload[0] >> 5) & 0x07;
  u_int8_t reserved = (packet->payload[0] >> 3) & 0x01;

  if (version != 1 || reserved != 0) {
    goto not_bfcp;
  }

  u_int8_t primitive = packet->payload[1];
  if (primitive < 1 || primitive > 18) {
    goto not_bfcp;
  }

  u_int16_t bfcp_payload_len = packet->payload_packet_len - 12;
  if (bfcp_payload_len != ntohs(get_u_int16_t(packet->payload, 2))) {
    goto not_bfcp;
  }

  u_int32_t conference_id = ntohl(get_u_int32_t(packet->payload, 4));
  if (!flow->bfcp_stage) {
    flow->bfcp_conference_id = conference_id;
    flow->bfcp_stage = 1;
    return;
  }

  if (flow->bfcp_stage && flow->bfcp_conference_id == conference_id) {
    NDPI_LOG_INFO(ndpi_struct, "found BFCP\n");
    ndpi_set_detected_protocol(ndpi_struct, flow, NDPI_PROTOCOL_BFCP,
                               NDPI_PROTOCOL_UNKNOWN, NDPI_CONFIDENCE_DPI);
    return;
  }

not_bfcp:
  NDPI_EXCLUDE_PROTO(ndpi_struct, flow);
}

void init_bfcp_dissector(struct ndpi_detection_module_struct *ndpi_struct)
{
  ndpi_set_bitmask_protocol_detection("BFCP", ndpi_struct,
                                      NDPI_PROTOCOL_BFCP,
                                      ndpi_search_bfcp,
                                      NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_OR_UDP_WITH_PAYLOAD_WITHOUT_RETRANSMISSION,
                                      SAVE_DETECTION_BITMASK_AS_UNKNOWN,
                                      ADD_TO_DETECTION_BITMASK);
}
