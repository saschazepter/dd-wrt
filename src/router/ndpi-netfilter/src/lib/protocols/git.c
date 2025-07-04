/*
 * git.c
 *
 * Copyright (C) 2012-22 - ntop.org
 *
 * This module is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ndpi_protocol_ids.h"

#define NDPI_CURRENT_PROTO NDPI_PROTOCOL_GIT

#include "ndpi_api.h"
#include "ndpi_private.h"


#define GIT_PORT 9418

static void ndpi_search_git(struct ndpi_detection_module_struct *ndpi_struct,
			    struct ndpi_flow_struct *flow)
{
  struct ndpi_packet_struct * packet = ndpi_get_packet_struct(ndpi_struct);

  NDPI_LOG_DBG(ndpi_struct, "search Git\n");

  if((packet->tcp != NULL) && (packet->payload_packet_len > 4)) {
    if((ntohs(packet->tcp->source) == GIT_PORT)
       || (ntohs(packet->tcp->dest) == GIT_PORT)) {
      const u_int8_t * pp = packet->payload;
      u_int16_t payload_len = packet->payload_packet_len;  
      u_int8_t found_git = 1;
      u_int16_t offset = 0;
      
      while((offset+4) < payload_len) {
	char len[5];
	u_int32_t git_pkt_len;

	memcpy(&len, &pp[offset], 4), len[4] = 0;
	if(sscanf(len, "%x", &git_pkt_len) != 1) {
	  found_git = 0;
	  break;
	}

	if((payload_len < git_pkt_len) || (git_pkt_len == 0 /* Bad */)) {
	  found_git = 0;
	  break;
	} else
	  offset += git_pkt_len, payload_len -= git_pkt_len;      
      }

      if(found_git) {
	NDPI_LOG_INFO(ndpi_struct, "found Git\n");
	ndpi_set_detected_protocol(ndpi_struct, flow, NDPI_PROTOCOL_GIT, NDPI_PROTOCOL_UNKNOWN, NDPI_CONFIDENCE_DPI);
	return;
      }
    }
  }
  
  NDPI_EXCLUDE_DISSECTOR(ndpi_struct, flow);
}


/* ***************************************************************** */

void init_git_dissector(struct ndpi_detection_module_struct *ndpi_struct)
{
  register_dissector("Git", ndpi_struct,
                     ndpi_search_git,
                     NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_WITH_PAYLOAD_WITHOUT_RETRANSMISSION,
                     1, NDPI_PROTOCOL_GIT);
}
