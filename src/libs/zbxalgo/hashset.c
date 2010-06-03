/*
** ZABBIX
** Copyright (C) 2000-2010 SIA Zabbix
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**/

#include "common.h"
#include "log.h"

#include "zbxalgo.h"

static void	__hashset_free_entry(zbx_hashset_t *hs, ZBX_HASHSET_ENTRY_T *entry);

#define	CRIT_LOAD_FACTOR	4/5
#define	SLOT_GROWTH_FACTOR	3/2

/* private hashset functions */

static void	__hashset_free_entry(zbx_hashset_t *hs, ZBX_HASHSET_ENTRY_T *entry)
{
	hs->mem_free_func(entry->data);
	hs->mem_free_func(entry);
}

/* public hashset interface */

void	zbx_hashset_create(zbx_hashset_t *hs, size_t init_size,
				zbx_hash_func_t hash_func,
				zbx_compare_func_t compare_func)
{
	zbx_hashset_create_ext(hs, init_size, hash_func, compare_func,
					ZBX_DEFAULT_MEM_MALLOC_FUNC,
					ZBX_DEFAULT_MEM_REALLOC_FUNC,
					ZBX_DEFAULT_MEM_FREE_FUNC);
}

void	zbx_hashset_create_ext(zbx_hashset_t *hs, size_t init_size,
				zbx_hash_func_t hash_func,
				zbx_compare_func_t compare_func,
				zbx_mem_malloc_func_t mem_malloc_func,
				zbx_mem_realloc_func_t mem_realloc_func,
				zbx_mem_free_func_t mem_free_func)
{
	const char	*__function_name = "zbx_hashset_create";

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __function_name);

	hs->num_data = 0;
	hs->num_slots = next_prime(init_size);

	hs->slots = mem_malloc_func(NULL, hs->num_slots * sizeof(ZBX_HASHSET_ENTRY_T *));
	memset(hs->slots, 0, hs->num_slots * sizeof(ZBX_HASHSET_ENTRY_T *));

	hs->hash_func = hash_func;
	hs->compare_func = compare_func;
	hs->mem_malloc_func = mem_malloc_func;
	hs->mem_realloc_func = mem_realloc_func;
	hs->mem_free_func = mem_free_func;

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);
}

void	zbx_hashset_destroy(zbx_hashset_t *hs)
{
	const char		*__function_name = "zbx_hashset_destroy";

	int			i;
	ZBX_HASHSET_ENTRY_T	*entry, *next_entry;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __function_name);

	for (i = 0; i < hs->num_slots; i++)
	{
		entry = hs->slots[i];

		while (NULL != entry)
		{
			next_entry = entry->next;
			__hashset_free_entry(hs, entry);
			entry = next_entry;
		}
	}

	hs->num_data = 0;
	hs->num_slots = 0;

	hs->mem_free_func(hs->slots);
	hs->slots = NULL;

	hs->hash_func = NULL;
	hs->compare_func = NULL;
	hs->mem_malloc_func = NULL;
	hs->mem_realloc_func = NULL;
	hs->mem_free_func = NULL;

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);
}

void	*zbx_hashset_insert(zbx_hashset_t *hs, const void *data, size_t size)
{
	const char		*__function_name = "zbx_hashset_insert";

	int			slot;
	zbx_hash_t		hash;
	ZBX_HASHSET_ENTRY_T	*entry;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __function_name);

	hash = hs->hash_func(data);

	slot = hash % hs->num_slots;
	entry = hs->slots[slot];
	while (NULL != entry)
	{
		if (entry->hash == hash && hs->compare_func(entry->data, data) == 0)
			break;

		entry = entry->next;
	}

	if (NULL == entry)
	{
		entry = hs->mem_malloc_func(NULL, sizeof(ZBX_HASHSET_ENTRY_T));
		entry->data = hs->mem_malloc_func(NULL, size);
		memcpy(entry->data, data, size);
		entry->hash = hash;
		entry->next = hs->slots[slot];
		hs->slots[slot] = entry;
		hs->num_data++;
		
		if (hs->num_data >= hs->num_slots * CRIT_LOAD_FACTOR)
		{
			int			inc_slots, new_slot;
			ZBX_HASHSET_ENTRY_T	**prev_next, *curr_entry, *tmp;

			inc_slots = next_prime(hs->num_slots * SLOT_GROWTH_FACTOR);

			hs->slots = hs->mem_realloc_func(hs->slots, inc_slots * sizeof(ZBX_HASHSET_ENTRY_T *));
			memset(hs->slots + hs->num_slots, 0, (inc_slots - hs->num_slots) * sizeof(ZBX_HASHSET_ENTRY_T *));

			for (slot = 0; slot < hs->num_slots; slot++)
			{
				prev_next = &hs->slots[slot];
				curr_entry = hs->slots[slot];

				while (NULL != curr_entry)
				{
					if (slot != (new_slot = curr_entry->hash % inc_slots))
					{
						tmp = curr_entry->next;
						curr_entry->next = hs->slots[new_slot];
						hs->slots[new_slot] = curr_entry;

						*prev_next = tmp;
						curr_entry = tmp;
					}
					else
					{
						prev_next = &curr_entry->next;
						curr_entry = curr_entry->next;
					}
				}
			}

			hs->num_slots = inc_slots;
		}
	}

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);

	return entry->data;
}

void	*zbx_hashset_search(zbx_hashset_t *hs, const void *data)
{
	const char		*__function_name = "zbx_hashset_search";

	int			slot;
	zbx_hash_t		hash;
	ZBX_HASHSET_ENTRY_T	*entry;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __function_name);

	hash = hs->hash_func(data);

	slot = hash % hs->num_slots;
	entry = hs->slots[slot];
	while (NULL != entry)
	{
		if (entry->hash == hash && hs->compare_func(entry->data, data) == 0)
			break;

		entry = entry->next;
	}

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);

	return (NULL != entry ? entry->data : NULL);
}

void	zbx_hashset_remove(zbx_hashset_t *hs, const void *data)
{
	const char		*__function_name = "zbx_hashset_remove";

	int			slot;
	zbx_hash_t		hash;
	ZBX_HASHSET_ENTRY_T	*entry;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __function_name);

	hash = hs->hash_func(data);

	slot = hash % hs->num_slots;
	entry = hs->slots[slot];

	if (NULL != entry)
	{
		if (entry->hash == hash && hs->compare_func(entry->data, data) == 0)
		{
			hs->slots[slot] = entry->next;
			__hashset_free_entry(hs, entry);
			hs->num_data--;
		}
		else
		{
			ZBX_HASHSET_ENTRY_T	*prev_entry;

			prev_entry = entry;
			entry = entry->next;

			while (NULL != entry)
			{
				if (entry->hash == hash && hs->compare_func(entry->data, data) == 0)
				{
					prev_entry->next = entry->next;
					__hashset_free_entry(hs, entry);
					hs->num_data--;
					break;
				}

				prev_entry = entry;
				entry = entry->next;
			}
		}
	}

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);
}

void	zbx_hashset_clear(zbx_hashset_t *hs)
{
	const char		*__function_name = "zbx_hashset_clear";

	int			slot;
	ZBX_HASHSET_ENTRY_T	*entry;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __function_name);

	for (slot = 0; slot < hs->num_slots; slot++)
	{
		while (NULL != hs->slots[slot])
		{
			entry = hs->slots[slot];
			hs->slots[slot] = entry->next;
			__hashset_free_entry(hs, entry);
		}
	}

	hs->num_data = 0;

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);
}

#define	ITER_START	(-1)
#define	ITER_FINISH	(-2)

void	zbx_hashset_iter_reset(zbx_hashset_t *hs, zbx_hashset_iter_t *iter)
{
	iter->hashset = hs;
	iter->slot = ITER_START;
}

void	*zbx_hashset_iter_next(zbx_hashset_iter_t *iter)
{
	if (ITER_FINISH == iter->slot)
		return NULL;

	if (ITER_START != iter->slot && NULL != iter->entry && NULL != iter->entry->next)
	{
		iter->entry = iter->entry->next;
		return iter->entry->data;
	}

	while (1)
	{
		iter->slot++;

		if (iter->slot == iter->hashset->num_slots)
		{
			iter->slot = ITER_FINISH;
			return NULL;
		}

		if (NULL != iter->hashset->slots[iter->slot])
		{
			iter->entry = iter->hashset->slots[iter->slot];
			return iter->entry->data;
		}
	}
}

void	zbx_hashset_iter_remove(zbx_hashset_iter_t *iter)
{
	if (ITER_START == iter->slot || ITER_FINISH == iter->slot || NULL == iter->entry)
	{
		zabbix_log(LOG_LEVEL_CRIT, "Removing a hashset entry through a bad iterator.");
		exit(FAIL);
	}

	if (iter->hashset->slots[iter->slot] == iter->entry)
	{
		iter->hashset->slots[iter->slot] = iter->entry->next;
		__hashset_free_entry(iter->hashset, iter->entry);
		iter->hashset->num_data--;

		iter->slot--;
		iter->entry = NULL;
	}
	else
	{
		ZBX_HASHSET_ENTRY_T	*prev_entry = iter->hashset->slots[iter->slot];

		while (prev_entry->next != iter->entry)
			prev_entry = prev_entry->next;

		prev_entry->next = iter->entry->next;		
		__hashset_free_entry(iter->hashset, iter->entry);
		iter->hashset->num_data--;

		iter->entry = prev_entry;
	}
}
