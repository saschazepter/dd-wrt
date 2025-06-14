/*
 * dropbox.c
 *
 * Copyright (C) 2012-18 by ntop.org
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

#define NDPI_CURRENT_PROTO NDPI_PROTOCOL_DROPBOX

#include "ndpi_api.h"
#include "ndpi_private.h"


#define DB_LSP_PORT 17500

static void ndpi_int_dropbox_add_connection(struct ndpi_detection_module_struct *ndpi_struct,
					    struct ndpi_flow_struct *flow) {
  ndpi_set_detected_protocol(ndpi_struct, flow, NDPI_PROTOCOL_DROPBOX, NDPI_PROTOCOL_UNKNOWN, NDPI_CONFIDENCE_DPI);
}


static void ndpi_check_dropbox(struct ndpi_detection_module_struct *ndpi_struct, struct ndpi_flow_struct *flow)
{
  struct ndpi_packet_struct *packet = ndpi_get_packet_struct(ndpi_struct);
  u_int32_t payload_len = packet->payload_packet_len;
  u_int16_t dropbox_port = htons(DB_LSP_PORT);

  if(packet->udp->dest == dropbox_port) {    
    if(packet->udp->source == dropbox_port) {	
      if(payload_len > 10) {
        if(ndpi_strnstr((const char *)packet->payload, "\"host_int\"", payload_len) != NULL) {	    
          NDPI_LOG_INFO(ndpi_struct, "found dropbox\n");
          ndpi_int_dropbox_add_connection(ndpi_struct, flow);
          return;
	}
      }
    } else {
      if(payload_len > 10) {
        if(ndpi_strnstr((const char *)packet->payload, "Bus17Cmd", payload_len) != NULL) {	    
          NDPI_LOG_INFO(ndpi_struct, "found dropbox\n");
          ndpi_int_dropbox_add_connection(ndpi_struct, flow);
          return;
	}
      }
    }
  }
  
  NDPI_EXCLUDE_DISSECTOR(ndpi_struct, flow);
}

static void ndpi_search_dropbox(struct ndpi_detection_module_struct *ndpi_struct, struct ndpi_flow_struct *flow)
{
  NDPI_LOG_DBG(ndpi_struct, "search dropbox\n");

  ndpi_check_dropbox(ndpi_struct, flow);
}


void init_dropbox_dissector(struct ndpi_detection_module_struct *ndpi_struct)
{
  register_dissector("DROPBOX", ndpi_struct,
                     ndpi_search_dropbox,
                     NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_UDP_WITH_PAYLOAD,
                     1, NDPI_PROTOCOL_DROPBOX);
}
