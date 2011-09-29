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
#ifndef __HNJ_JUST_H__
#define __HNJ_JUST_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _HnjBreak HnjBreak;
typedef struct _HnjParams HnjParams;

/* A potential line break (input to the justification routine).

   x0 and x1 are calculated on the basis that the paragraph is
   broken only at the line break (i.e. into two lines). x0 is the
   length of the first line, and x1 is x0 plus the length of the
   unbroken line minus the sum of the lengths of the two lines.

   I'll probably want to put in a "hyphen" flag which causes extra
   penalty if there are two in a row.
 */
struct _HnjBreak {
  int x0;
  int x1;
  int penalty;
  int flags;
};

#define HNJ_JUST_FLAG_ISSPACE 1
#define HNJ_JUST_FLAG_ISHYPHEN 2
#define HNJ_JUST_FLAG_ISTAB 4

/* The justification parameters.

   max_neg_space is the maximum amount that can be subtracted from a
   space (i.e. if more is subtracted from a space, the penalty is
   infinite).

   Otherwise, the penalty for a line is simply the square of the
   deviation from the set_width.

   This structure will probably grow. For example, extra penalties for
   very short last lines, lists of line lengths (possibly lazy; for
   doing shapes), other junk. But this will do for now.

   */
struct _HnjParams {
  int set_width;
  int max_neg_space;
  int tab_width;
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __HNJ_JUST_H__ */
