/* LibHnj is dual licensed under LGPL and MPL. Boilerplate for both
 * licenses follows.
 */

/* LibHnj - a library for high quality hyphenation and justification
 * Copyright (C) 1998 Raph Levien
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA  02111-1307  USA.
*/

/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 */
/* A module for a simple "name context", i.e. lisp-style atoms */
#include <string.h>
#include "z_misc.h"

#include "namecontext.h"

/* btw, I do not know who wrote the following comment. I modified this
   file somewhat from gimp's app/procedural_db.c hash function. */

static unsigned int
name_context_hash_func (char *string)
{
  unsigned int result;
  int c;
  int i;

  /*
   * I tried a zillion different hash functions and asked many other
   * people for advice.  Many people had their own favorite functions,
   * all different, but no-one had much idea why they were good ones.
   * I chose the one below (multiply by 9 and add new character)
   * because of the following reasons:
   *
   * 1. Multiplying by 10 is perfect for keys that are decimal strings,
   *    and multiplying by 9 is just about as good.
   * 2. Times-9 is (shift-left-3) plus (old).  This means that each
   *    character's bits hang around in the low-order bits of the
   *    hash value for ever, plus they spread fairly rapidly up to
   *    the high-order bits to fill out the hash value.  This seems
   *    works well both for decimal and non-decimal strings.
   *
   * tclHash.c --
   *
   *      Implementation of in-memory hash tables for Tcl and Tcl-based
   *      applications.
   *
   * Copyright (c) 1991-1993 The Regents of the University of California.
   * Copyright (c) 1994 Sun Microsystems, Inc.
   */

  result = 0;
  for (i = 0; (c = ((unsigned char *)string)[i]) != '\0'; i++)
    result += (result << 3) + c;

  return result;
}

static unsigned int
name_context_hash_func_size (char *string, int size)
{
  unsigned int result;
  int i;

  result = 0;
  for (i = 0; i < size; i++)
    result += (result << 3) + ((unsigned char *)string)[i];

  return result;
}

NameContext *
name_context_new (void)
{
  NameContext *nc;
  int i;

  nc = z_new (NameContext, 1);
  nc->num_entries = 0;
  nc->table_size = 16;
  nc->table = z_new (NameContextHashEntry, nc->table_size);
  for (i = 0; i < nc->table_size; i++)
    nc->table[i].name = NULL;

  return nc;
}

void
name_context_free (NameContext *nc)
{
  int i;

  for (i = 0; i < nc->table_size; i++)
    if (nc->table[i].name != NULL)
      z_free (nc->table[i].name);
  z_free (nc->table);
  z_free (nc);
}

static char *
name_context_strdup (char *s)
{
  int len;
  char *new;

  len = strlen (s);
  new = z_new (char, len + 1);
  memcpy (new, s, len);
  new[len] = '\0';
  return new;
}

static z_boolean
name_context_streq_size (char *s1, char *s2, int size2)
{
  int i;

  /* could rewrite for 32 bits at a time, but I wouldn't worry */
  for (i = 0; i < size2; i++)
    if (s1[i] != s2[i])
      return z_false;
  return s1[i] == 0;
}

static char *
name_context_strdup_size (char *s, int size)
{
  char *new;

  new = z_new (char, size + 1);
  memcpy (new, s, size);
  new[size] = '\0';
  return new;
}

/* double the size of the hash table, rehashing as needed */
static void
name_context_double (NameContext *nc)
{
  int i, j;
  int oldsize, newmask;
  NameContextHashEntry *old_table, *new_table;

  oldsize = nc->table_size;
  old_table = nc->table;
  nc->table_size = oldsize << 1;
  newmask = nc->table_size - 1;
  new_table = z_new (NameContextHashEntry, nc->table_size);

  for (j = 0; j < nc->table_size; j++)
    new_table[j].name = NULL;

  for (i = 0; i < oldsize; i++)
    {
      if (old_table[i].name)
	{
	  for (j = name_context_hash_func(old_table[i].name);
	       new_table[j & newmask].name;
	       j++);
	  new_table[j & newmask] = old_table[i];
	}
    }

  z_free (old_table);
  nc->table = new_table;
}

/* Return the unique (to this name context) nameid for the given string,
   allocating a new one if necessary. */
NameId
name_context_intern (NameContext *nc, char *name)
{
  int i;
  int mask;

  mask = nc->table_size - 1;
  for (i = name_context_hash_func (name); nc->table[i & mask].name; i++)
    if (!strcmp (nc->table[i & mask].name, name))
      return nc->table[i & mask].nameid;

  /* not found, allocate a new one */
  if (nc->num_entries >= nc->table_size >> 1)
    {
      name_context_double (nc);
      mask = nc->table_size - 1;
      for (i = name_context_hash_func (name); nc->table[i & mask].name; i++);
    }

  i &= mask;
  nc->table[i].name = name_context_strdup (name);
  nc->table[i].nameid = nc->num_entries;

  return nc->num_entries++;
}

/* Return the unique (to this name context) nameid for the given
   string, allocating a new one if necessary. The string is not
   necessarily null-terminated; the size is given explicitly. */
NameId
name_context_intern_size (NameContext *nc, char *name, int size)
{
  int i;
  int mask;

  mask = nc->table_size - 1;
  for (i = name_context_hash_func_size (name, size);
       nc->table[i & mask].name;
       i++)
    if (name_context_streq_size (nc->table[i & mask].name, name, size))
      return nc->table[i & mask].nameid;

  /* not found, allocate a new one */
  if (nc->num_entries >= nc->table_size >> 1)
    {
      name_context_double (nc);
      mask = nc->table_size - 1;
      for (i = name_context_hash_func_size (name, size);
	   nc->table[i & mask].name;
	   i++);
    }

  i &= mask;
  nc->table[i].name = name_context_strdup_size (name, size);
  nc->table[i].nameid = nc->num_entries;

  return nc->num_entries++;
}

/* This one is slow - it's intended for debugging only */
char *
name_context_string (NameContext *nc, NameId nameid)
{
  int j;

  for (j = 0; j < nc->table_size; j++)
    if (nc->table[j].name && nc->table[j].nameid == nameid)
      return nc->table[j].name;

  return NULL;
}
