/*
 * Copyright (c) 2012, 2016, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


/**
 * @defgroup isisc_qos ISISC_QOS
 * @{
 */
#include "sw.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "hsl_port_prop.h"
#include "isisc_qos.h"
#include "isisc_reg.h"
#if defined(MHT)
#include "mht_reg.h"

#define MHT_PHYPORT_QUEUE_MAX	4
#define MHT_QUEUE_BUFFER_LEN	6
#define MHT_QOS_QUEUE_TX_BUFFER_MAX 504
#define MHT_QOS_PORT_TX_BUFFER_MAX  2040
#define MHT_QOS_PORT_RX_BUFFER_MAX  504
#endif

#define ISISC_QOS_QUEUE_TX_BUFFER_MAX 120
#define ISISC_QOS_PORT_TX_BUFFER_MAX  504
#define ISISC_QOS_PORT_RX_BUFFER_MAX  120
#define ISISC_QOS_REACT_BUFFER_MAX    64

#define ISISC_QOS_HOL_STEP       8
#define ISISC_QOS_HOL_MOD        3

//#define ISISC_MIN_QOS_MODE_PRI 0
#define ISISC_MAX_QOS_MODE_PRI   3
#define ISISC_MAX_PRI            7
#define ISISC_MAX_QUEUE          3
#define ISISC_MAX_EH_QUEUE       5


static sw_error_t
_isisc_qos_port_queue_check(fal_port_t port_id, fal_queue_t queue_id)
{
    if ((0 == port_id) || (5 == port_id) || (6 == port_id))
    {
        if (ISISC_MAX_EH_QUEUE < queue_id)
        {
            return SW_BAD_PARAM;
        }
    }
    else
    {
        if (ISISC_MAX_QUEUE < queue_id)
        {
            return SW_BAD_PARAM;
        }
    }

    return SW_OK;
}


#ifndef IN_QOS_MINI
static sw_error_t
_isisc_qos_queue_tx_buf_status_set(a_uint32_t dev_id, fal_port_t port_id,
                                  a_bool_t enable)
{
    a_uint32_t val = 0;
    sw_error_t rv;

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    if (A_TRUE == enable)
    {
        val = 1;
    }
    else if (A_FALSE == enable)
    {
        val = 0;
    }
    else
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_HOL_CTL1, port_id, QUEUE_DESC_EN,
                      (a_uint8_t *) (&val), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isisc_qos_queue_tx_buf_status_get(a_uint32_t dev_id, fal_port_t port_id,
                                  a_bool_t * enable)
{
    a_uint32_t val = 0;
    sw_error_t rv;

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_HOL_CTL1, port_id, QUEUE_DESC_EN,
                      (a_uint8_t *) (&val), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (1 == val)
    {
        *enable = A_TRUE;
    }
    else
    {
        *enable = A_FALSE;
    }

    return SW_OK;
}

static sw_error_t
_isisc_qos_port_tx_buf_status_set(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t enable)
{
    a_uint32_t val = 0;
    sw_error_t rv;

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    if (A_TRUE == enable)
    {
        val = 1;
    }
    else if (A_FALSE == enable)
    {
        val = 0;
    }
    else
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_HOL_CTL1, port_id, PORT_DESC_EN,
                      (a_uint8_t *) (&val), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isisc_qos_port_tx_buf_status_get(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t * enable)
{
    a_uint32_t val = 0;
    sw_error_t rv;

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_HOL_CTL1, port_id, PORT_DESC_EN,
                      (a_uint8_t *) (&val), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (1 == val)
    {
        *enable = A_TRUE;
    }
    else
    {
        *enable = A_FALSE;
    }

    return SW_OK;
}




static sw_error_t
_isisc_qos_port_red_en_get(a_uint32_t dev_id, fal_port_t port_id,
                          a_bool_t * enable)
{
    a_uint32_t val = 0;
    sw_error_t rv;

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_HOL_CTL1, port_id, PORT_RED_EN,
                      (a_uint8_t *) (&val), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (1 == val)
    {
        *enable = A_TRUE;
    }
    else
    {
        *enable = A_FALSE;
    }

    return SW_OK;
}

static sw_error_t
_isisc_qos_queue_tx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_queue_t queue_id, a_uint32_t * number)
{
    a_uint32_t data = 0, val = 0;
    sw_error_t rv;

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    rv = _isisc_qos_port_queue_check(port_id, queue_id);
    SW_RTN_ON_ERROR(rv);

    HSL_REG_ENTRY_GET(rv, dev_id, PORT_HOL_CTL0, port_id, (a_uint8_t *) (&data),
		    sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    val = (data >> (queue_id << 2)) & 0xf;

    *number = val << ISISC_QOS_HOL_MOD;
    return SW_OK;
}

#if defined(MHT)
static sw_error_t
_mht_qos_port_tx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id,
                             a_uint32_t * number)
{
	a_uint32_t val = 0;
	sw_error_t rv;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
		return SW_BAD_PARAM;

	HSL_REG_FIELD_GET(rv, dev_id, MHT_PORT_HOL_CTL0, port_id, PORT_DESC_NR,
			(a_uint8_t *) (&val), sizeof (a_uint32_t));
	SW_RTN_ON_ERROR(rv);

	*number = val << ISISC_QOS_HOL_MOD;
	return SW_OK;
}
#endif

static sw_error_t
_isisc_qos_port_tx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id,
                             a_uint32_t * number)
{
	a_uint32_t val = 0;
	sw_error_t rv;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
		return SW_BAD_PARAM;

	HSL_REG_FIELD_GET(rv, dev_id, PORT_HOL_CTL0, port_id, PORT_DESC_NR,
			(a_uint8_t *) (&val), sizeof (a_uint32_t));
	SW_RTN_ON_ERROR(rv);

	*number = val << ISISC_QOS_HOL_MOD;
	return SW_OK;
}
#endif

static sw_error_t
_isisc_qos_port_rx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id,
                             a_uint32_t * number, a_uint32_t * react_num)
{
    a_uint32_t val = 0, val0 = 0;
    sw_error_t rv;

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

#if defined(MHT)
    if (hsl_get_current_chip_type(dev_id) == CHIP_MHT) {
	    HSL_REG_FIELD_GET(rv, dev_id, MHT_PORT_HOL_CTL1, port_id, PORT_IN_DESC_EN,
			    (a_uint8_t *) (&val), sizeof (a_uint32_t));
    } else
#endif
    {
	    HSL_REG_FIELD_GET(rv, dev_id, PORT_HOL_CTL1, port_id, PORT_IN_DESC_EN,
			    (a_uint8_t *) (&val), sizeof (a_uint32_t));
    }
    SW_RTN_ON_ERROR(rv);

    *number = val << ISISC_QOS_HOL_MOD;

    HSL_REG_ENTRY_GET(rv, dev_id, QM_CTRL_REG, 0, (a_uint8_t *)(&val),
        sizeof(a_uint32_t));
    SW_RTN_ON_ERROR(rv);
    SW_GET_FIELD_BY_REG(QM_CTRL_REG, FLOW_DROP_EN, val0, val);
    if(val0 == 1)
    {
        SW_GET_FIELD_BY_REG(QM_CTRL_REG, FLOW_DROP_CNT, *react_num, val);
    }
    else
        *react_num = ISISC_QOS_REACT_BUFFER_MAX;

    return SW_OK;
}

static sw_error_t
_isisc_qos_port_red_en_set(a_uint32_t dev_id, fal_port_t port_id,
                          a_bool_t enable)
{
    a_uint32_t val = 0;
    sw_error_t rv;

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }
    if (A_TRUE == enable)
    {
        val = 1;
    }
    else if (A_FALSE == enable)
    {
        val = 0;
    }
    else
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_HOL_CTL1, port_id, PORT_RED_EN,
                      (a_uint8_t *) (&val), sizeof (a_uint32_t));
    return rv;
}

#if defined(MHT)
static sw_error_t
_mht_qos_queue_tx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_queue_t queue_id, a_uint32_t * number)
{
	a_uint32_t data = 0, val = 0;
	sw_error_t rv;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	if (MHT_QOS_QUEUE_TX_BUFFER_MAX < *number)
	{
		return SW_BAD_PARAM;
	}

	rv = _isisc_qos_port_queue_check(port_id, queue_id);
	SW_RTN_ON_ERROR(rv);

	val = *number / ISISC_QOS_HOL_STEP;
	*number = val << ISISC_QOS_HOL_MOD;

	if (queue_id >= MHT_PHYPORT_QUEUE_MAX) {
		HSL_REG_ENTRY_GET(rv, dev_id, MHT_PORT_HOL_CTL1, port_id,
				(a_uint8_t *)(&data), sizeof(a_uint32_t));
		SW_RTN_ON_ERROR(rv);

		data &= (~(0x3f << (MHT_PORT_HOL_CTL1_QUEUE4_DESC_NR_BOFFSET +
				(queue_id - MHT_PHYPORT_QUEUE_MAX) * MHT_QUEUE_BUFFER_LEN)));
		data |= (val << (MHT_PORT_HOL_CTL1_QUEUE4_DESC_NR_BOFFSET +
				(queue_id - MHT_PHYPORT_QUEUE_MAX) * MHT_QUEUE_BUFFER_LEN));

		HSL_REG_ENTRY_SET(rv, dev_id, MHT_PORT_HOL_CTL1, port_id, (a_uint8_t *) (&data),
				sizeof (a_uint32_t));
	} else {
		HSL_REG_ENTRY_GET(rv, dev_id, MHT_PORT_HOL_CTL0, port_id, (a_uint8_t *) (&data),
				sizeof (a_uint32_t));
		SW_RTN_ON_ERROR(rv);

		data &= (~(0x3f << (queue_id * MHT_QUEUE_BUFFER_LEN)));
		data |= (val << (queue_id * MHT_QUEUE_BUFFER_LEN));

		HSL_REG_ENTRY_SET(rv, dev_id, MHT_PORT_HOL_CTL0, port_id, (a_uint8_t *) (&data),
				sizeof (a_uint32_t));
	}

	return rv;
}
#endif

static sw_error_t
_isisc_qos_queue_tx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id,
                              fal_queue_t queue_id, a_uint32_t * number)
{
    a_uint32_t data = 0, val = 0;
    sw_error_t rv;

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    if (ISISC_QOS_QUEUE_TX_BUFFER_MAX < *number)
    {
        return SW_BAD_PARAM;
    }

    rv = _isisc_qos_port_queue_check(port_id, queue_id);
    SW_RTN_ON_ERROR(rv);

    val = *number / ISISC_QOS_HOL_STEP;
    *number = val << ISISC_QOS_HOL_MOD;

    HSL_REG_ENTRY_GET(rv, dev_id, PORT_HOL_CTL0, port_id, (a_uint8_t *) (&data),
		    sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    data &= (~(0xf << (queue_id << 2)));
    data |= (val << (queue_id << 2));

    HSL_REG_ENTRY_SET(rv, dev_id, PORT_HOL_CTL0, port_id, (a_uint8_t *) (&data),
		    sizeof (a_uint32_t));

    return rv;
}

#if defined(MHT)
static sw_error_t
_mht_qos_port_tx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id,
                             a_uint32_t * number)
{
	a_uint32_t val = 0;
	sw_error_t rv;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
		return SW_BAD_PARAM;

	if (MHT_QOS_PORT_TX_BUFFER_MAX < *number)
		return SW_BAD_PARAM;

	val = *number / ISISC_QOS_HOL_STEP;
	*number = val << ISISC_QOS_HOL_MOD;

	HSL_REG_FIELD_SET(rv, dev_id, MHT_PORT_HOL_CTL0, port_id, PORT_DESC_NR,
			(a_uint8_t *) (&val), sizeof (a_uint32_t));
	return rv;
}
#endif

static sw_error_t
_isisc_qos_port_tx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id,
                             a_uint32_t * number)
{
	a_uint32_t val = 0;
	sw_error_t rv;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
		return SW_BAD_PARAM;


	if (ISISC_QOS_PORT_TX_BUFFER_MAX < *number)
		return SW_BAD_PARAM;

	val = *number / ISISC_QOS_HOL_STEP;
	*number = val << ISISC_QOS_HOL_MOD;

	HSL_REG_FIELD_SET(rv, dev_id, PORT_HOL_CTL0, port_id, PORT_DESC_NR,
			(a_uint8_t *) (&val), sizeof (a_uint32_t));
	return rv;
}

static sw_error_t
_isisc_qos_port_rx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id,
                             a_uint32_t * number, a_uint32_t * react_num)
{
	a_uint32_t val = 0, buffer_max = ISISC_QOS_PORT_RX_BUFFER_MAX;
	ssdk_chip_type chip_type = CHIP_UNSPECIFIED;
	sw_error_t rv;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	chip_type = hsl_get_current_chip_type(dev_id);
#if defined(MHT)
	if (chip_type == CHIP_MHT)
		buffer_max = MHT_QOS_PORT_RX_BUFFER_MAX;
#endif

	if (buffer_max < *number)
		return SW_BAD_PARAM;

	val = *number / ISISC_QOS_HOL_STEP;
	*number = val << ISISC_QOS_HOL_MOD;

#if defined(MHT)
	if (chip_type == CHIP_MHT) {
		HSL_REG_FIELD_SET(rv, dev_id, MHT_PORT_HOL_CTL1, port_id, PORT_IN_DESC_EN,
				(a_uint8_t *) (&val), sizeof (a_uint32_t));
	} else
#endif
	{
		HSL_REG_FIELD_SET(rv, dev_id, PORT_HOL_CTL1, port_id, PORT_IN_DESC_EN,
				(a_uint8_t *) (&val), sizeof (a_uint32_t));
	}
	SW_RTN_ON_ERROR(rv);

	HSL_REG_ENTRY_GET(rv, dev_id, QM_CTRL_REG, 0, (a_uint8_t*)(&val), sizeof(a_uint32_t));
	SW_RTN_ON_ERROR(rv);

	if(*react_num < ISISC_QOS_REACT_BUFFER_MAX)
	{
		SW_SET_REG_BY_FIELD(QM_CTRL_REG, FLOW_DROP_EN, 0x1, val);
		SW_SET_REG_BY_FIELD(QM_CTRL_REG, FLOW_DROP_CNT, *react_num, val);
	}
	else if(*react_num == ISISC_QOS_REACT_BUFFER_MAX)
	{
		SW_SET_REG_BY_FIELD(QM_CTRL_REG, FLOW_DROP_EN, 0, val);
	}
	else
	{
		return SW_OUT_OF_RANGE;
	}
	HSL_REG_ENTRY_SET(rv, dev_id, QM_CTRL_REG, 0, (a_uint8_t *) (&val),
			sizeof (a_uint32_t));

	return rv;
}

static sw_error_t
_isisc_qos_port_mode_set(a_uint32_t dev_id, fal_port_t port_id,
                        fal_qos_mode_t mode, a_bool_t enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    if (A_TRUE == enable)
    {
        val = 1;
    }
    else if (A_FALSE == enable)
    {
        val = 0;
    }
    else
    {
        return SW_BAD_PARAM;
    }

    if (FAL_QOS_DA_MODE == mode)
    {
        HSL_REG_FIELD_SET(rv, dev_id, PRI_CTL, port_id, DA_PRI_EN,
                          (a_uint8_t *) (&val), sizeof (a_uint32_t));
    }
    else if (FAL_QOS_UP_MODE == mode)
    {
        HSL_REG_FIELD_SET(rv, dev_id, PRI_CTL, port_id, VLAN_PRI_EN,
                          (a_uint8_t *) (&val), sizeof (a_uint32_t));
    }
    else if (FAL_QOS_DSCP_MODE == mode)
    {
        HSL_REG_FIELD_SET(rv, dev_id, PRI_CTL, port_id, IP_PRI_EN,
                          (a_uint8_t *) (&val), sizeof (a_uint32_t));
    }
#if defined(MHT)
    else if (FAL_QOS_FLOW_MODE == mode)
    {
        HSL_REG_FIELD_SET(rv, dev_id, PRI_CTL, port_id, FLOW_PRI_EN,
                          (a_uint8_t *) (&val), sizeof (a_uint32_t));
    }
#endif
    else
    {
        return SW_BAD_PARAM;
    }

    return rv;
}

#ifndef IN_QOS_MINI
static sw_error_t
_isisc_qos_port_mode_get(a_uint32_t dev_id, fal_port_t port_id,
                        fal_qos_mode_t mode, a_bool_t * enable)
{
    sw_error_t rv;
    a_uint32_t val = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    if (FAL_QOS_DA_MODE == mode)
    {
        HSL_REG_FIELD_GET(rv, dev_id, PRI_CTL, port_id, DA_PRI_EN,
                          (a_uint8_t *) (&val), sizeof (a_uint32_t));
    }
    else if (FAL_QOS_UP_MODE == mode)
    {
        HSL_REG_FIELD_GET(rv, dev_id, PRI_CTL, port_id, VLAN_PRI_EN,
                          (a_uint8_t *) (&val), sizeof (a_uint32_t));
    }
    else if (FAL_QOS_DSCP_MODE == mode)
    {
        HSL_REG_FIELD_GET(rv, dev_id, PRI_CTL, port_id, IP_PRI_EN,
                          (a_uint8_t *) (&val), sizeof (a_uint32_t));
    }
#if defined(MHT)
    else if (FAL_QOS_FLOW_MODE == mode)
    {
        HSL_REG_FIELD_GET(rv, dev_id, PRI_CTL, port_id, FLOW_PRI_EN,
                          (a_uint8_t *) (&val), sizeof (a_uint32_t));
    }
#endif
    else
    {
        return SW_BAD_PARAM;
    }
    SW_RTN_ON_ERROR(rv);

    if (1 == val)
    {
        *enable = A_TRUE;
    }
    else
    {
        *enable = A_FALSE;
    }

    return SW_OK;
}

static sw_error_t
_isisc_qos_port_mode_pri_set(a_uint32_t dev_id, fal_port_t port_id,
                            fal_qos_mode_t mode, a_uint32_t pri)
{
    sw_error_t rv;
    a_uint32_t val = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    if (ISISC_MAX_QOS_MODE_PRI < pri)
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_ENTRY_GET(rv, dev_id, PRI_CTL, port_id, (a_uint8_t *) (&val),
                      sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (FAL_QOS_DA_MODE == mode)
    {
        SW_SET_REG_BY_FIELD(PRI_CTL, DA_PRI_SEL, pri, val);
    }
    else if (FAL_QOS_UP_MODE == mode)
    {
        SW_SET_REG_BY_FIELD(PRI_CTL, VLAN_PRI_SEL, pri, val);
    }
    else if (FAL_QOS_DSCP_MODE == mode)
    {
        SW_SET_REG_BY_FIELD(PRI_CTL, IP_PRI_SEL, pri, val);
    }
#if defined(MHT)
    else if (FAL_QOS_FLOW_MODE == mode)
    {
        SW_SET_REG_BY_FIELD(PRI_CTL, FLOW_PRI_SEL, pri, val);
    }
#endif
    else
    {
        return SW_NOT_SUPPORTED;
    }

    HSL_REG_ENTRY_SET(rv, dev_id, PRI_CTL, port_id, (a_uint8_t *) (&val),
                      sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isisc_qos_port_mode_pri_get(a_uint32_t dev_id, fal_port_t port_id,
                            fal_qos_mode_t mode, a_uint32_t * pri)
{
    sw_error_t rv;
    a_uint32_t entry = 0, f_val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_ENTRY_GET(rv, dev_id, PRI_CTL, port_id, (a_uint8_t *) (&entry),
                      sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    if (FAL_QOS_DA_MODE == mode)
    {
        SW_GET_FIELD_BY_REG(PRI_CTL, DA_PRI_SEL, f_val, entry);
    }
    else if (FAL_QOS_UP_MODE == mode)
    {
        SW_GET_FIELD_BY_REG(PRI_CTL, VLAN_PRI_SEL, f_val, entry);
    }
    else if (FAL_QOS_DSCP_MODE == mode)
    {
        SW_GET_FIELD_BY_REG(PRI_CTL, IP_PRI_SEL, f_val, entry);
    }
#if defined(MHT)
    else if (FAL_QOS_FLOW_MODE == mode)
    {
        SW_GET_FIELD_BY_REG(PRI_CTL, FLOW_PRI_SEL, f_val, entry);
    }
#endif
    else
    {
        return SW_NOT_SUPPORTED;
    }

    *pri = f_val;
    return SW_OK;
}

static sw_error_t
_isisc_qos_port_sch_mode_set(a_uint32_t dev_id, a_uint32_t port_id,
                            fal_sch_mode_t mode, const a_uint32_t weight[])
{
    sw_error_t rv;
    a_uint32_t reg = 0, val, w[6] = { 0 };
    a_int32_t i, _index;

    HSL_DEV_ID_CHECK(dev_id);

    if (FAL_SCH_SP_MODE == mode)
    {
        val = 0;
        _index = -1;
    }
    else if (FAL_SCH_WRR_MODE == mode)
    {
        val = 3;
        _index = 5;
    }
    else if (FAL_SCH_MIX_MODE == mode)
    {
        val = 1;
        _index = 4;
    }
    else if (FAL_SCH_MIX_PLUS_MODE == mode)
    {
        val = 2;
        _index = 3;
    }
    else
    {
        return SW_NOT_SUPPORTED;
    }

    for (i = _index; i >= 0; i--)
    {
        if (weight[i] > 0x1f)
        {
            return SW_BAD_PARAM;
        }
        w[i] = weight[i];
    }

    HSL_REG_ENTRY_GET(rv, dev_id, WRR_CTRL, port_id, (a_uint8_t *) (&reg),
                      sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    SW_SET_REG_BY_FIELD(WRR_CTRL, SCH_MODE, val, reg);
    SW_SET_REG_BY_FIELD(WRR_CTRL, Q5_W, w[5], reg);
    SW_SET_REG_BY_FIELD(WRR_CTRL, Q4_W, w[4], reg);
    SW_SET_REG_BY_FIELD(WRR_CTRL, Q3_W, w[3], reg);
    SW_SET_REG_BY_FIELD(WRR_CTRL, Q2_W, w[2], reg);
    SW_SET_REG_BY_FIELD(WRR_CTRL, Q1_W, w[1], reg);
    SW_SET_REG_BY_FIELD(WRR_CTRL, Q0_W, w[0], reg);

    HSL_REG_ENTRY_SET(rv, dev_id, WRR_CTRL, port_id,
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isisc_qos_port_sch_mode_get(a_uint32_t dev_id, a_uint32_t port_id,
                            fal_sch_mode_t * mode, a_uint32_t weight[])
{
    sw_error_t rv;
    a_uint32_t val = 0, sch, w[6], i;

    HSL_DEV_ID_CHECK(dev_id);

    HSL_REG_ENTRY_GET(rv, dev_id, WRR_CTRL, port_id, (a_uint8_t *) (&val),
                      sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    SW_GET_FIELD_BY_REG(WRR_CTRL, SCH_MODE, sch, val);
    SW_GET_FIELD_BY_REG(WRR_CTRL, Q5_W, w[5], val);
    SW_GET_FIELD_BY_REG(WRR_CTRL, Q4_W, w[4], val);
    SW_GET_FIELD_BY_REG(WRR_CTRL, Q3_W, w[3], val);
    SW_GET_FIELD_BY_REG(WRR_CTRL, Q2_W, w[2], val);
    SW_GET_FIELD_BY_REG(WRR_CTRL, Q1_W, w[1], val);
    SW_GET_FIELD_BY_REG(WRR_CTRL, Q0_W, w[0], val);

    if (0 == sch)
    {
        *mode = FAL_SCH_SP_MODE;
    }
    else if (1 == sch)
    {
        *mode = FAL_SCH_MIX_MODE;
    }
    else if (2 == sch)
    {
        *mode = FAL_SCH_MIX_PLUS_MODE;
    }
    else
    {
        *mode = FAL_SCH_WRR_MODE;
    }

    for (i = 0; i < 6; i++)
    {
        weight[i] = w[i];
    }

    return SW_OK;
}

static sw_error_t
_isisc_qos_port_default_spri_set(a_uint32_t dev_id, fal_port_t port_id,
                                a_uint32_t spri)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    if (ISISC_MAX_PRI < spri)
    {
        return SW_BAD_PARAM;
    }

    val = spri;
    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN0, port_id,
                      ING_SPRI, (a_uint8_t *) (&val), sizeof (a_uint32_t));

    return rv;
}

static sw_error_t
_isisc_qos_port_default_spri_get(a_uint32_t dev_id, fal_port_t port_id,
                                a_uint32_t * spri)
{
    sw_error_t rv;
    a_uint32_t val = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN0, port_id,
                      ING_SPRI, (a_uint8_t *) (&val), sizeof (a_uint32_t));

    *spri = val & 0x7;
    return rv;
}

static sw_error_t
_isisc_qos_port_default_cpri_set(a_uint32_t dev_id, fal_port_t port_id,
                                a_uint32_t cpri)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    if (ISISC_MAX_PRI < cpri)
    {
        return SW_BAD_PARAM;
    }

    val = cpri;
    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN0, port_id,
                      ING_CPRI, (a_uint8_t *) (&val), sizeof (a_uint32_t));

    return rv;
}

HSL_LOCAL sw_error_t
_isisc_qos_port_default_cpri_get(a_uint32_t dev_id, fal_port_t port_id,
                                a_uint32_t * cpri)
{
    sw_error_t rv;
    a_uint32_t val = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN0, port_id,
                      ING_CPRI, (a_uint8_t *) (&val), sizeof (a_uint32_t));

    *cpri = val & 0x7;
    return rv;
}

HSL_LOCAL sw_error_t
_isisc_qos_port_force_spri_status_set(a_uint32_t dev_id, fal_port_t port_id,
                                     a_bool_t enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    if (A_TRUE == enable)
    {
        val = 1;
    }
    else if (A_FALSE == enable)
    {
        val = 0;
    }
    else
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN0, port_id,
                      ING_FORCE_SPRI, (a_uint8_t *) (&val), sizeof (a_uint32_t));
    return rv;
}

HSL_LOCAL sw_error_t
_isisc_qos_port_force_spri_status_get(a_uint32_t dev_id, fal_port_t port_id,
                                     a_bool_t* enable)
{
    sw_error_t rv;
    a_uint32_t val = 0;

    HSL_DEV_ID_CHECK(dev_id);
    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN0, port_id,
                      ING_FORCE_SPRI, (a_uint8_t *) (&val), sizeof (a_uint32_t));

    if (1 == val)
    {
        *enable = A_TRUE;
    }
    else
    {
        *enable = A_FALSE;
    }

    return rv;
}

HSL_LOCAL sw_error_t
_isisc_qos_port_force_cpri_status_set(a_uint32_t dev_id, fal_port_t port_id,
                                     a_bool_t enable)
{
    sw_error_t rv;
    a_uint32_t val;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    val = enable ? 1 : 0;

    HSL_REG_FIELD_SET(rv, dev_id, PORT_VLAN0, port_id,
                      ING_FORCE_CPRI, (a_uint8_t *) (&val), sizeof (a_uint32_t));

    return rv;
}

HSL_LOCAL sw_error_t
_isisc_qos_port_force_cpri_status_get(a_uint32_t dev_id, fal_port_t port_id,
                                     a_bool_t* enable)
{
    sw_error_t rv;
    a_uint32_t val = 0;

    HSL_DEV_ID_CHECK(dev_id);

    if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
    {
        return SW_BAD_PARAM;
    }

    HSL_REG_FIELD_GET(rv, dev_id, PORT_VLAN0, port_id,
                      ING_FORCE_CPRI, (a_uint8_t *) (&val), sizeof (a_uint32_t));

    if (1 == val)
    {
        *enable = A_TRUE;
    }
    else
    {
        *enable = A_FALSE;
    }

    return rv;
}

static sw_error_t
_isisc_qos_queue_remark_table_set(a_uint32_t dev_id, fal_port_t port_id,
                                 fal_queue_t queue_id, a_uint32_t tbl_id, a_bool_t enable)
{
    sw_error_t rv;
    a_uint32_t addr, data = 0;
    a_uint32_t base[7] = {0x0c40, 0x0c48, 0x0c4c, 0x0c50, 0x0c54, 0x0c58, 0x0c60};

    rv = _isisc_qos_port_queue_check(port_id, queue_id);
    SW_RTN_ON_ERROR(rv);

    addr = base[port_id] + ((queue_id / 4) << 2);
    HSL_REG_ENTRY_GEN_GET(rv, dev_id, addr, sizeof (a_uint32_t),
                          (a_uint8_t *) (&data), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    data &= (~(0xff << ((queue_id % 4) << 3)));
    data |= (((enable << 7 ) | (tbl_id & 0xf)) << ((queue_id % 4) << 3));

    HSL_REG_ENTRY_GEN_SET(rv, dev_id, addr, sizeof (a_uint32_t),
                          (a_uint8_t *) (&data), sizeof (a_uint32_t));
    return rv;
}

HSL_LOCAL sw_error_t
_isisc_qos_queue_remark_table_get(a_uint32_t dev_id, fal_port_t port_id,
                                 fal_queue_t queue_id, a_uint32_t * tbl_id, a_bool_t * enable)
{
    sw_error_t rv;
    a_uint32_t addr, data = 0;
    a_uint32_t base[7] = {0x0c40, 0x0c48, 0x0c4c, 0x0c50, 0x0c54, 0x0c58, 0x0c60};

    rv = _isisc_qos_port_queue_check(port_id, queue_id);
    SW_RTN_ON_ERROR(rv);

    addr = base[port_id] + ((queue_id / 4) << 2);
    HSL_REG_ENTRY_GEN_GET(rv, dev_id, addr, sizeof (a_uint32_t),
                          (a_uint8_t *) (&data), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    *tbl_id = (data >> ((queue_id % 4) << 3)) & 0xf;
    *enable = ((data >> ((queue_id % 4) << 3)) & 0x80) >> 7;
    return SW_OK;
}

HSL_LOCAL sw_error_t
_isisc_port_static_thresh_get(a_uint32_t dev_id, fal_port_t port_id,
                                fal_bm_static_cfg_t *cfg)
{
	sw_error_t rv;
	a_uint32_t reg_value, xon_value, xoff_value;

	HSL_REG_ENTRY_GET(rv, dev_id, PORT_FLOW_CTRL_THRESHOLD, port_id,
			(a_uint8_t *) (&reg_value), sizeof (a_uint32_t));
	SW_RTN_ON_ERROR(rv);

	SW_GET_FIELD_BY_REG(PORT_FLOW_CTRL_THRESHOLD, XON_THRES, xon_value, reg_value);
	SW_GET_FIELD_BY_REG(PORT_FLOW_CTRL_THRESHOLD, XOFF_THRES, xoff_value, reg_value);

	cfg->max_thresh = xoff_value;
	cfg->resume_off = xoff_value - xon_value;

	return SW_OK;
}

#if defined(MHT)
HSL_LOCAL sw_error_t
_mht_port_static_thresh_get(a_uint32_t dev_id, fal_port_t port_id,
                                fal_bm_static_cfg_t *cfg)
{
	sw_error_t rv;
	a_uint32_t reg_value, xon_value, xoff_value;

	HSL_REG_ENTRY_GET(rv, dev_id, MHT_PORT_FLOW_CTRL_THRESHOLD, port_id,
			(a_uint8_t *) (&reg_value), sizeof (a_uint32_t));
	SW_RTN_ON_ERROR(rv);

	SW_GET_FIELD_BY_REG(MHT_PORT_FLOW_CTRL_THRESHOLD, XON_THRES, xon_value, reg_value);
	SW_GET_FIELD_BY_REG(MHT_PORT_FLOW_CTRL_THRESHOLD, XOFF_THRES, xoff_value, reg_value);
	cfg->max_thresh = xoff_value;
	cfg->resume_off = xoff_value - xon_value;

	return SW_OK;
}

HSL_LOCAL sw_error_t
_mht_port_static_thresh_set(a_uint32_t dev_id, fal_port_t port_id,
                                fal_bm_static_cfg_t *cfg)
{
	sw_error_t rv;
	a_uint32_t reg_value = 0;

	SW_SET_REG_BY_FIELD(MHT_PORT_FLOW_CTRL_THRESHOLD, XON_THRES,
			cfg->max_thresh - cfg->resume_off, reg_value);
	SW_SET_REG_BY_FIELD(MHT_PORT_FLOW_CTRL_THRESHOLD, XOFF_THRES,
			cfg->max_thresh, reg_value);

	HSL_REG_ENTRY_SET(rv, dev_id, MHT_PORT_FLOW_CTRL_THRESHOLD, port_id,
			(a_uint8_t *)(&reg_value), sizeof(a_uint32_t));

	return SW_OK;
}
#endif

HSL_LOCAL sw_error_t
_isisc_port_static_thresh_set(a_uint32_t dev_id, fal_port_t port_id,
                                fal_bm_static_cfg_t *cfg)
{
	sw_error_t rv;
	a_uint32_t reg_value = 0;

	SW_SET_REG_BY_FIELD(PORT_FLOW_CTRL_THRESHOLD, XON_THRES,
			cfg->max_thresh - cfg->resume_off, reg_value);
	SW_SET_REG_BY_FIELD(PORT_FLOW_CTRL_THRESHOLD, XOFF_THRES,
			cfg->max_thresh, reg_value);

	HSL_REG_ENTRY_SET(rv, dev_id, PORT_FLOW_CTRL_THRESHOLD, port_id,
			(a_uint8_t *)(&reg_value), sizeof(a_uint32_t));

	return SW_OK;
}

#if defined(MHT)
static sw_error_t
_mht_qos_queue_tx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_queue_t queue_id, a_uint32_t * number)
{
	a_uint32_t data = 0, val = 0;
	sw_error_t rv;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	rv = _isisc_qos_port_queue_check(port_id, queue_id);
	SW_RTN_ON_ERROR(rv);

	if (queue_id >= MHT_PHYPORT_QUEUE_MAX) {
		HSL_REG_ENTRY_GET(rv, dev_id, MHT_PORT_HOL_CTL1, port_id,
				(a_uint8_t *)(&data), sizeof(a_uint32_t));
		SW_RTN_ON_ERROR(rv);
		val = (data >> (MHT_PORT_HOL_CTL1_QUEUE4_DESC_NR_BOFFSET +
				(queue_id - MHT_PHYPORT_QUEUE_MAX) * MHT_QUEUE_BUFFER_LEN)) & 0x3f;
	} else {
		HSL_REG_ENTRY_GET(rv, dev_id, MHT_PORT_HOL_CTL0, port_id,
				(a_uint8_t *)(&data), sizeof(a_uint32_t));
		SW_RTN_ON_ERROR(rv);
		val = (data >> (queue_id * MHT_QUEUE_BUFFER_LEN)) & 0x3f;
	}

	*number = val << ISISC_QOS_HOL_MOD;
	return SW_OK;
}
#endif

/**
 * @brief Set buffer aggsinment status of transmitting queue on one particular port.
 *   @details  Comments:
 *    If enable queue tx buffer on one port that means each queue of this port
 *    will have fixed number buffers when transmitting packets. Otherwise they
 *    share the whole buffers with other queues in device.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_queue_tx_buf_status_set(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_queue_tx_buf_status_set(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get buffer aggsinment status of transmitting queue on particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_queue_tx_buf_status_get(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t * enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_queue_tx_buf_status_get(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set buffer aggsinment status of transmitting port on one particular port.
 *   @details  Comments:
    If enable tx buffer on one port that means this port will have fixed
    number buffers when transmitting packets. Otherwise they will
    share the whole buffers with other ports in device.
 *    function will return actual buffer numbers in hardware.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_tx_buf_status_set(a_uint32_t dev_id, fal_port_t port_id,
                                a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_tx_buf_status_set(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get buffer aggsinment status of transmitting port on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_tx_buf_status_get(a_uint32_t dev_id, fal_port_t port_id,
                                a_bool_t * enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_tx_buf_status_get(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}



/**
 * @brief Set status of port red on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_red_en_get(a_uint32_t dev_id, fal_port_t port_id,
                         a_bool_t* enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_red_en_get(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}




/**
 * @brief Get max occupied buffer number of transmitting queue on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] queue_id queue id
 * @param[out] number buffer number
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_queue_tx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id,
                             fal_queue_t queue_id, a_uint32_t * number)
{
	sw_error_t rv;

	HSL_API_LOCK;
#if defined(MHT)
	if (hsl_get_current_chip_type(dev_id) == CHIP_MHT)
		rv = _mht_qos_queue_tx_buf_nr_get(dev_id, port_id, queue_id, number);
	else
#endif
		rv = _isisc_qos_queue_tx_buf_nr_get(dev_id, port_id, queue_id, number);
	HSL_API_UNLOCK;

	return rv;
}



/**
 * @brief Get max occupied buffer number of transmitting port on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] number buffer number
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_tx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t * number)
{
    sw_error_t rv;

    HSL_API_LOCK;
#if defined(MHT)
    if (hsl_get_current_chip_type(dev_id) == CHIP_MHT)
	    rv = _mht_qos_port_tx_buf_nr_get(dev_id, port_id, number);
    else
#endif
	    rv = _isisc_qos_port_tx_buf_nr_get(dev_id, port_id, number);
    HSL_API_UNLOCK;
    return rv;
}
#endif

/**
 * @brief Get max occupied buffer number of receiving port on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] number buffer number
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_rx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t * number, a_uint32_t * react_num)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_rx_buf_nr_get(dev_id, port_id, number, react_num);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set status of port red on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_red_en_set(a_uint32_t dev_id, fal_port_t port_id,
                         a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_red_en_set(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}
/**
 * @brief Set max occupied buffer number of transmitting queue on one particular port.
 *   @details   Comments:
    The step of buffer number in Garuda is 4, function will return actual
    buffer numbers in hardware.
    The buffer number range for queue is 4 to 60.
 *    share the whole buffers with other ports in device.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] queue_id queue id
 * @param number buffer number
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_queue_tx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id,
                             fal_queue_t queue_id, a_uint32_t * number)
{
	sw_error_t rv;

	HSL_API_LOCK;
#if defined(MHT)
	if (hsl_get_current_chip_type(dev_id) == CHIP_MHT)
		rv = _mht_qos_queue_tx_buf_nr_set(dev_id, port_id, queue_id, number);
	else
#endif
		rv = _isisc_qos_queue_tx_buf_nr_set(dev_id, port_id, queue_id, number);
	HSL_API_UNLOCK;

	return rv;
}
/**
 * @brief Set max occupied buffer number of transmitting port on one particular port.
 *   @details   Comments:
    The step of buffer number in Garuda is four, function will return actual
    buffer numbers in hardware.
    The buffer number range for transmitting port is 4 to 124.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param number buffer number
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_tx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t * number)
{
    sw_error_t rv;

    HSL_API_LOCK;
#if defined(MHT)
    if (hsl_get_current_chip_type(dev_id) == CHIP_MHT)
	    rv = _mht_qos_port_tx_buf_nr_set(dev_id, port_id, number);
    else
#endif
	    rv = _isisc_qos_port_tx_buf_nr_set(dev_id, port_id, number);
    HSL_API_UNLOCK;
    return rv;
}
/**
 * @brief Set max occupied buffer number of receiving port on one particular port.
 *   @details   Comments:
    The step of buffer number in Shiva is four, function will return actual
    buffer numbers in hardware.
    The buffer number range for receiving port is 4 to 60.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param number buffer number
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_rx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t * number, a_uint32_t * react_num)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_rx_buf_nr_set(dev_id, port_id, number, react_num);
    HSL_API_UNLOCK;
    return rv;
}
/**
 * @brief Set port qos mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] mode qos mode
 * @param[in] enable A_TRUE of A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_mode_set(a_uint32_t dev_id, fal_port_t port_id,
                       fal_qos_mode_t mode, a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_mode_set(dev_id, port_id, mode, enable);
    HSL_API_UNLOCK;
    return rv;
}


#ifndef IN_QOS_MINI
/**
 * @brief Get port qos mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] mode qos mode
 * @param[out] enable A_TRUE of A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_mode_get(a_uint32_t dev_id, fal_port_t port_id,
                       fal_qos_mode_t mode, a_bool_t * enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_mode_get(dev_id, port_id, mode, enable);
    HSL_API_UNLOCK;
    return rv;
}
/**
 * @brief Set priority of one particular qos mode on one particular port.
 *   @details   Comments:
    If the priority of a mode is more small then the priority is more high.
    Differnet mode should have differnet priority.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] mode qos mode
 * @param[in] pri priority of one particular qos mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_mode_pri_set(a_uint32_t dev_id, fal_port_t port_id,
                           fal_qos_mode_t mode, a_uint32_t pri)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_mode_pri_set(dev_id, port_id, mode, pri);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get priority of one particular qos mode on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] mode qos mode
 * @param[out] pri priority of one particular qos mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_mode_pri_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_qos_mode_t mode, a_uint32_t * pri)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_mode_pri_get(dev_id, port_id, mode, pri);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set traffic scheduling mode on particular one port.
 *   @details   Comments:
 *   When scheduling mode is sp the weight is meaningless usually it's zero
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] fal_sch_mode_t traffic scheduling mode
 * @param[in] weight[] weight value for each queue when in wrr mode,
              the max value supported by ISIS is 0x1f.
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_sch_mode_set(a_uint32_t dev_id, a_uint32_t port_id,
                           fal_sch_mode_t mode, const a_uint32_t weight[])
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_sch_mode_set(dev_id, port_id, mode, weight);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get traffic scheduling mode on particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] fal_sch_mode_t traffic scheduling mode
 * @param[out] weight weight value for wrr mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_sch_mode_get(a_uint32_t dev_id, a_uint32_t port_id,
                           fal_sch_mode_t * mode, a_uint32_t weight[])
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_sch_mode_get(dev_id, port_id, mode, weight);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set default stag priority on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] spri vlan priority
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_default_spri_set(a_uint32_t dev_id, fal_port_t port_id,
                               a_uint32_t spri)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_default_spri_set(dev_id, port_id, spri);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get default stag priority on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] spri vlan priority
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_default_spri_get(a_uint32_t dev_id, fal_port_t port_id,
                               a_uint32_t * spri)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_default_spri_get(dev_id, port_id, spri);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set default ctag priority on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] cpri vlan priority
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_default_cpri_set(a_uint32_t dev_id, fal_port_t port_id,
                               a_uint32_t cpri)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_default_cpri_set(dev_id, port_id, cpri);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get default ctag priority on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] cpri vlan priority
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_default_cpri_get(a_uint32_t dev_id, fal_port_t port_id,
                               a_uint32_t * cpri)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_default_cpri_get(dev_id, port_id, cpri);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set port force stag priority enable flag one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_force_spri_status_set(a_uint32_t dev_id, fal_port_t port_id,
                                    a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_force_spri_status_set(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get port force stag priority enable flag one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_force_spri_status_get(a_uint32_t dev_id, fal_port_t port_id,
                                    a_bool_t* enable)
{
    sw_error_t rv;
    HSL_API_LOCK;
    rv = _isisc_qos_port_force_spri_status_get(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set port force ctag priority enable flag one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_force_cpri_status_set(a_uint32_t dev_id, fal_port_t port_id,
                                    a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_force_cpri_status_set(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get port force ctag priority enable flag one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_port_force_cpri_status_get(a_uint32_t dev_id, fal_port_t port_id,
                                    a_bool_t* enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_port_force_cpri_status_get(dev_id, port_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set egress queue based CoS remark on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] queue_id queue id
 * @param[in] tbl_id CoS remark table id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_queue_remark_table_set(a_uint32_t dev_id, fal_port_t port_id,
                                fal_queue_t queue_id, a_uint32_t tbl_id, a_bool_t enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_queue_remark_table_set(dev_id, port_id, queue_id, tbl_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get egress queue based CoS remark on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] queue_id queue id
 * @param[out] tbl_id CoS remark table id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_qos_queue_remark_table_get(a_uint32_t dev_id, fal_port_t port_id,
                                fal_queue_t queue_id, a_uint32_t * tbl_id, a_bool_t * enable)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isisc_qos_queue_remark_table_get(dev_id, port_id, queue_id, tbl_id, enable);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get static flow control threshold on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] static maximum threshold and resume offset
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_port_static_thresh_get(a_uint32_t dev_id, fal_port_t port_id,
                                fal_bm_static_cfg_t *cfg)
{
    sw_error_t rv;

    HSL_API_LOCK;
#if defined(MHT)
    if (hsl_get_current_chip_type(dev_id) == CHIP_MHT)
	    rv = _mht_port_static_thresh_get(dev_id, port_id, cfg);
    else
#endif
	    rv = _isisc_port_static_thresh_get(dev_id, port_id, cfg);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set static flow control threshold on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] static maximum threshold and resume offset
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isisc_port_static_thresh_set(a_uint32_t dev_id, fal_port_t port_id,
                                fal_bm_static_cfg_t *cfg)
{
    sw_error_t rv;

    HSL_API_LOCK;
#if defined(MHT)
    if (hsl_get_current_chip_type(dev_id) == CHIP_MHT)
	    rv = _mht_port_static_thresh_set(dev_id, port_id, cfg);
    else
#endif
	    rv = _isisc_port_static_thresh_set(dev_id, port_id, cfg);
    HSL_API_UNLOCK;
    return rv;
}
#endif

sw_error_t
isisc_qos_init(a_uint32_t dev_id)
{
    HSL_DEV_ID_CHECK(dev_id);

#ifndef HSL_STANDALONG
    {
        hsl_api_t *p_api;

        SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));
	p_api->qos_queue_tx_buf_nr_set = isisc_qos_queue_tx_buf_nr_set;
	p_api->qos_port_red_en_set = isisc_qos_port_red_en_set;
	p_api->qos_port_tx_buf_nr_set = isisc_qos_port_tx_buf_nr_set;
	p_api->qos_port_rx_buf_nr_set = isisc_qos_port_rx_buf_nr_set;
        p_api->qos_port_mode_set = isisc_qos_port_mode_set;
#ifndef IN_QOS_MINI
        p_api->qos_queue_tx_buf_status_set = isisc_qos_queue_tx_buf_status_set;
        p_api->qos_queue_tx_buf_status_get = isisc_qos_queue_tx_buf_status_get;
        p_api->qos_port_tx_buf_status_set = isisc_qos_port_tx_buf_status_set;
        p_api->qos_port_tx_buf_status_get = isisc_qos_port_tx_buf_status_get;
        p_api->qos_port_red_en_get = isisc_qos_port_red_en_get;
        p_api->qos_queue_tx_buf_nr_get = isisc_qos_queue_tx_buf_nr_get;
        p_api->qos_port_tx_buf_nr_get = isisc_qos_port_tx_buf_nr_get;
#endif
        p_api->qos_port_rx_buf_nr_get = isisc_qos_port_rx_buf_nr_get;
#ifndef IN_QOS_MINI
	p_api->qos_port_mode_get = isisc_qos_port_mode_get;
        p_api->qos_port_mode_pri_set = isisc_qos_port_mode_pri_set;
        p_api->qos_port_mode_pri_get = isisc_qos_port_mode_pri_get;
        p_api->qos_port_sch_mode_set = isisc_qos_port_sch_mode_set;
        p_api->qos_port_sch_mode_get = isisc_qos_port_sch_mode_get;
        p_api->qos_port_default_spri_set = isisc_qos_port_default_spri_set;
        p_api->qos_port_default_spri_get = isisc_qos_port_default_spri_get;
        p_api->qos_port_default_cpri_set = isisc_qos_port_default_cpri_set;
        p_api->qos_port_default_cpri_get = isisc_qos_port_default_cpri_get;
        p_api->qos_port_force_spri_status_set = isisc_qos_port_force_spri_status_set;
        p_api->qos_port_force_spri_status_get = isisc_qos_port_force_spri_status_get;
        p_api->qos_port_force_cpri_status_set = isisc_qos_port_force_cpri_status_set;
        p_api->qos_port_force_cpri_status_get = isisc_qos_port_force_cpri_status_get;
        p_api->qos_queue_remark_table_set = isisc_qos_queue_remark_table_set;
        p_api->qos_queue_remark_table_get = isisc_qos_queue_remark_table_get;
	p_api->port_static_thresh_get = isisc_port_static_thresh_get;
	p_api->port_static_thresh_set = isisc_port_static_thresh_set;
#endif
    }
#endif

    return SW_OK;
}

/**
 * @}
 */
