/*
 * ethersio.c
 *
 * Copyright (C) 2023 - ntop.org
 * Copyright (C) 2023 - V.G <v.gavrilov@securitycode.ru>
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

#define NDPI_CURRENT_PROTO NDPI_PROTOCOL_ETHERSIO

#include "ndpi_api.h"
#include "ndpi_private.h"

static void ndpi_int_ethersio_add_connection(struct ndpi_detection_module_struct *ndpi_struct, 
                                             struct ndpi_flow_struct *flow)
{
  NDPI_LOG_INFO(ndpi_struct, "found EtherSIO\n");
  ndpi_set_detected_protocol(ndpi_struct, flow, NDPI_PROTOCOL_ETHERSIO,
                             NDPI_PROTOCOL_UNKNOWN, NDPI_CONFIDENCE_DPI);
}

static void ndpi_search_ethersio(struct ndpi_detection_module_struct *ndpi_struct,
                                 struct ndpi_flow_struct *flow)
{
  struct ndpi_packet_struct const * const packet = ndpi_get_packet_struct(ndpi_struct);

  NDPI_LOG_DBG(ndpi_struct, "search Ether-SIO\n");

  if (packet->payload_packet_len >= 20) {
    if ((memcmp(packet->payload, "ESIO", 4) == 0) &&
        (packet->payload[4] == 0) && (packet->payload[5] <= 0x2) &&
        (packet->payload[6] == 0))
    {
      ndpi_int_ethersio_add_connection(ndpi_struct, flow);
      return;
    }
  }

  NDPI_EXCLUDE_DISSECTOR(ndpi_struct, flow);
}
void init_ethersio_dissector(struct ndpi_detection_module_struct *ndpi_struct)
{
  register_dissector("EtherSIO", ndpi_struct,
                     ndpi_search_ethersio,
                     NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_UDP_WITH_PAYLOAD,
                     1, NDPI_PROTOCOL_ETHERSIO);
}
