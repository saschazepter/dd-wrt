/*
 * ldap.c
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

#define NDPI_CURRENT_PROTO NDPI_PROTOCOL_LDAP

#include "ndpi_api.h"
#include "ndpi_private.h"


static void ndpi_int_ldap_add_connection(struct ndpi_detection_module_struct *ndpi_struct,
					 struct ndpi_flow_struct *flow)
{
  ndpi_set_detected_protocol(ndpi_struct, flow, NDPI_PROTOCOL_LDAP, NDPI_PROTOCOL_UNKNOWN, NDPI_CONFIDENCE_DPI);
}

static void ndpi_search_ldap(struct ndpi_detection_module_struct *ndpi_struct, struct ndpi_flow_struct *flow)
{
  struct ndpi_packet_struct *packet = ndpi_get_packet_struct(ndpi_struct);
  int64_t length;
  u_int16_t length_len = 0, msg_id_len;
  u_int8_t op;
	
  NDPI_LOG_DBG(ndpi_struct, "search ldap\n");

  if(packet->payload_packet_len > 1 &&
     packet->payload[0] == 0x30) {
    length = asn1_ber_decode_length(&packet->payload[1], packet->payload_packet_len - 1, &length_len);
    NDPI_LOG_DBG(ndpi_struct, "length %d (%d bytes)\n", length, length_len);
    if(length > 0 &&
       packet->payload_packet_len > 1 + length_len + 1 &&
       packet->payload[1 + length_len] == 0x02 /* Integer */) {
      msg_id_len = packet->payload[1 + length_len + 1];
      if(packet->payload_packet_len > 1 + length_len + 1 + msg_id_len + 1) {
        op = packet->payload[1 + length_len + 1 + msg_id_len + 1];
        NDPI_LOG_DBG(ndpi_struct, "Op 0x%x\n", op);
        if((op & 0x60) == 0x60 && /* Application */
           (op & 0x1F) <= 25) {
          NDPI_LOG_INFO(ndpi_struct, "found ldap\n");
          ndpi_int_ldap_add_connection(ndpi_struct, flow);
          return;
        }
      }
    }
  }

  NDPI_EXCLUDE_DISSECTOR(ndpi_struct, flow);
}


void init_ldap_dissector(struct ndpi_detection_module_struct *ndpi_struct)
{
  register_dissector("LDAP", ndpi_struct,
                     ndpi_search_ldap,
                     NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_OR_UDP_WITH_PAYLOAD_WITHOUT_RETRANSMISSION,
                      1, NDPI_PROTOCOL_LDAP);
}

