/*
** Zabbix
** Copyright (C) 2001-2024 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#ifndef ZABBIX_ZBXHISTORY_H
#define ZABBIX_ZBXHISTORY_H

#include "zbxvariant.h"
#include "zbxjson.h"
#include "zbxtime.h"

/* the item history value */
typedef struct
{
	zbx_timespec_t		timestamp;
	zbx_history_value_t	value;
}
zbx_history_record_t;

ZBX_VECTOR_DECL(history_record, zbx_history_record_t)

int     history_record_float_compare(const zbx_history_record_t *d1, const zbx_history_record_t *d2);

void	zbx_history_record_vector_clean(zbx_vector_history_record_t *vector, int value_type);
void	zbx_history_record_vector_destroy(zbx_vector_history_record_t *vector, int value_type);
void	zbx_history_record_clear(zbx_history_record_t *value, int value_type);

int	zbx_history_record_compare_asc_func(const zbx_history_record_t *d1, const zbx_history_record_t *d2);
int	zbx_history_record_compare_desc_func(const zbx_history_record_t *d1, const zbx_history_record_t *d2);

void	zbx_history_value2str(char *buffer, size_t size, const zbx_history_value_t *value, int value_type);
char	*zbx_history_value2str_dyn(const zbx_history_value_t *value, int value_type);
void	zbx_history_value_print(char *buffer, size_t size, const zbx_history_value_t *value, int value_type);
void	zbx_history_value2variant(const zbx_history_value_t *value, unsigned char value_type, zbx_variant_t *var);

/* In most cases zbx_history_record_vector_destroy() function should be used to free the  */
/* value vector filled by zbx_vc_get_value* functions. This define simply better          */
/* mirrors the vector creation function to vector destroying function.                    */
#define zbx_history_record_vector_create(vector)	zbx_vector_history_record_create(vector)

int	zbx_history_init(char **error);
void	zbx_history_destroy(void);

int	zbx_history_add_values(const zbx_vector_ptr_t *history, int *ret_flush);
int	zbx_history_get_values(zbx_uint64_t itemid, int value_type, int start, int count, int end,
		zbx_vector_history_record_t *values);

int	zbx_history_requires_trends(int value_type);
void	zbx_history_check_version(struct zbx_json *json, int *result);

#define FLUSH_SUCCEED		0
#define FLUSH_FAIL		-1
#define FLUSH_DUPL_REJECTED	-2

#define ZBX_DC_FLAG_META	0x01	/* contains meta information (lastlogsize and mtime) */
#define ZBX_DC_FLAG_NOVALUE	0x02	/* entry contains no value */
#define ZBX_DC_FLAG_LLD		0x04	/* low-level discovery value */
#define ZBX_DC_FLAG_UNDEF	0x08	/* unsupported or undefined (delta calculation failed) value */
#define ZBX_DC_FLAG_NOHISTORY	0x10	/* values should not be kept in history */
#define ZBX_DC_FLAG_NOTRENDS	0x20	/* values should not be kept in trends */
#define ZBX_DC_FLAG_HASTRIGGER	0x40	/* value is used in trigger expression */

typedef struct
{
	zbx_uint64_t		itemid;
	zbx_history_value_t	value;
	zbx_uint64_t		lastlogsize;
	zbx_timespec_t		ts;
	int			mtime;
	unsigned char		value_type;
	unsigned char		flags;		/* see ZBX_DC_FLAG_* */
	unsigned char		state;
	int			ttl;		/* time-to-live of the history value */
}
ZBX_DC_HISTORY;

#endif
