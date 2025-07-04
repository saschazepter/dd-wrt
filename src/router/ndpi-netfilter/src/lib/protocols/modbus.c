
/*
 * modbus.c
 *
 * Copyright (C) 2018 - ntop.org
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
#define NDPI_CURRENT_PROTO NDPI_PROTOCOL_MODBUS
#include "ndpi_api.h"
#include "ndpi_private.h"

static void ndpi_search_modbus_tcp(struct ndpi_detection_module_struct *ndpi_struct,
                            struct ndpi_flow_struct *flow) {
  struct ndpi_packet_struct *packet = ndpi_get_packet_struct(ndpi_struct);
  u_int16_t modbus_port = htons(502); // port used by modbus
  NDPI_LOG_DBG(ndpi_struct, "search Modbus\n");

  /* Check connection over TCP */
    
  if(packet->tcp) {
    /* The payload of Modbus-TCP segment must be at least 8 bytes (7 bytes of header application 
       packet plus 1 byte of minimum payload of application packet)
    */
    if((packet->payload_packet_len >= 8) 
       &&((packet->tcp->dest == modbus_port) || (packet->tcp->source == modbus_port))) {
      // Modbus uses the port 502		
      u_int16_t modbus_len = htons(*((u_int16_t*)&packet->payload[4]));

      // the fourth parameter of the payload is the length of the segment            
      if(((modbus_len-1) == (packet->payload_packet_len - 7 /* ModbusTCP header len */))
	 && (packet->payload[2] == 0x0) && (packet->payload[3] == 0x0) /* Protocol identifier */) {
	/* Check Modbus function code. 0x5A (90) is reserved for UMAS protocol */
        if (packet->payload[7] == 0x5A) {
          NDPI_LOG_INFO(ndpi_struct, "found Schneider Electric UMAS\n");
          ndpi_set_detected_protocol(ndpi_struct, flow, NDPI_PROTOCOL_UMAS, NDPI_PROTOCOL_MODBUS, NDPI_CONFIDENCE_DPI);
          return;
        }

        NDPI_LOG_INFO(ndpi_struct, "found MODBUS\n");
        ndpi_set_detected_protocol(ndpi_struct, flow, NDPI_PROTOCOL_MODBUS, NDPI_PROTOCOL_UNKNOWN, NDPI_CONFIDENCE_DPI);
        return;
      }
    }
  }
  NDPI_EXCLUDE_DISSECTOR(ndpi_struct, flow);
   
}



void init_modbus_dissector(struct ndpi_detection_module_struct *ndpi_struct) {

  register_dissector("Modbus", ndpi_struct,
                     ndpi_search_modbus_tcp,
                     NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_WITH_PAYLOAD_WITHOUT_RETRANSMISSION,
                      1, NDPI_PROTOCOL_MODBUS);
}
