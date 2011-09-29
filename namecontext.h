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
#ifndef __NAMECONTEXT_H__
#define __NAMECONTEXT_H__

typedef struct _NameContext NameContext;
typedef struct _NameContextHashEntry NameContextHashEntry;

typedef int NameId;

struct _NameContextHashEntry {
  char *name;
  int nameid;
};

struct _NameContext {
  int num_entries;
  int table_size;
  NameContextHashEntry *table;
};

NameContext *name_context_new (void);
void name_context_free (NameContext *nc);
NameId name_context_intern (NameContext *nc, char *name);
NameId name_context_intern_size (NameContext *nc, char *name, int size);
char *name_context_string (NameContext *nc, NameId nameid);

#endif
