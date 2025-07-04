/*
 * radius_proto.c
 *
 * Copyright (C) 2012-24 - ntop.org
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

#define NDPI_CURRENT_PROTO NDPI_PROTOCOL_RADIUS

#include "ndpi_api.h"
#include "ndpi_private.h"

#define RADIUS_PORT 			1812
#define RADIUS_PORT_ACC 		1813
#define RADIUS_PORT_ACC_ALTERNATIVE 	18013


struct radius_header {
  u_int8_t code;
  u_int8_t packet_id;
  u_int16_t len;
};

static void ndpi_check_radius(struct ndpi_detection_module_struct *ndpi_struct, struct ndpi_flow_struct *flow)
{
  struct ndpi_packet_struct *packet = ndpi_get_packet_struct(ndpi_struct);
  // const u_int8_t *packet_payload = packet->payload;
  u_int32_t payload_len = packet->payload_packet_len;

  if((packet->udp->dest == htons(RADIUS_PORT) || packet->udp->source == htons(RADIUS_PORT) ||
      packet->udp->dest == htons(RADIUS_PORT_ACC) || packet->udp->source == htons(RADIUS_PORT_ACC) ||
      packet->udp->dest == htons(RADIUS_PORT_ACC_ALTERNATIVE) || packet->udp->source == htons(RADIUS_PORT_ACC_ALTERNATIVE))) {
    struct radius_header *h = (struct radius_header*)packet->payload;
    /* RFC2865: The minimum length is 20 and maximum length is 4096. */
    if((payload_len < 20) || (payload_len > 4096)) {
      NDPI_EXCLUDE_DISSECTOR(ndpi_struct, flow);
      return;
    }
    
    if((h->code > 0)
       && (h->code <= 13)
       && (ntohs(h->len) == payload_len)) {
      NDPI_LOG_INFO(ndpi_struct, "Found radius\n");
      ndpi_set_detected_protocol(ndpi_struct, flow, NDPI_PROTOCOL_RADIUS, NDPI_PROTOCOL_UNKNOWN, NDPI_CONFIDENCE_DPI);
      return;
    }
  }
  if(flow->packet_counter > 3)
    NDPI_EXCLUDE_DISSECTOR(ndpi_struct, flow);
  return;
}

static void ndpi_search_radius(struct ndpi_detection_module_struct *ndpi_struct, struct ndpi_flow_struct *flow)
{
  NDPI_LOG_DBG(ndpi_struct, "search radius\n");

  ndpi_check_radius(ndpi_struct, flow);
}


void init_radius_dissector(struct ndpi_detection_module_struct *ndpi_struct)
{
  register_dissector("Radius", ndpi_struct,
                     ndpi_search_radius,
                     NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_UDP_WITH_PAYLOAD,
                     1, NDPI_PROTOCOL_RADIUS);
}
