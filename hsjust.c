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
#include "hsjust.h"
#include <limits.h>

/* A simple, high speed justification algorithm. Uses the greedy
   approach.

   */

int
hnj_hs_just (const HnjBreak *breaks, int n_breaks,
	     const HnjParams *params, int *result)
{
  int set_width = params->set_width;
  int max_neg_space = params->max_neg_space;
  int tab_width = params->tab_width;
  int break_in_idx;
  int result_idx;
  int x;
  int total_space; /* total space seen so far */
  int best_penalty;
  int best_idx;
  int space_err;
  int penalty;
  int tab_offset;

  if (tab_width == 0)
    tab_width = 1;

  for ( break_in_idx = 1; break_in_idx < n_breaks; break_in_idx ++ )
    {
      if ( breaks[break_in_idx].x0 < breaks[break_in_idx - 1].x0 )
	{
	  int j;
	  for ( j = break_in_idx - 1; j >= 0; j-- )
	    {
	      if ( breaks[j].x0 <= breaks[break_in_idx].x0 )
		break;
	      if ( breaks[j].penalty < INT_MAX / 2 )
		breaks[j].penalty += INT_MAX / 2;
	    }
	}
    }

  break_in_idx = 0;
  result_idx = 0;
  x = 0;
  while (break_in_idx != n_breaks)
    {
      total_space = 0;
      tab_offset = 0;

      /* Calculate penalty for first possible break. */
      space_err = breaks[break_in_idx].x0 - (x + set_width);
      best_penalty = space_err * space_err + breaks[break_in_idx].penalty;
      best_idx = break_in_idx;

      /* Check for a tab. */
      if (breaks[break_in_idx].flags & HNJ_JUST_FLAG_ISTAB)
	{
	  int next_stop = ((breaks[break_in_idx].x0 + tab_offset - x)
			   / tab_width + 1) * tab_width;
	  tab_offset = x + next_stop - breaks[break_in_idx].x0;
	}

      /* Now, keep trying to find a better break until either alll
	 breaks are exhausted, or the maximum negative space
	 constraint is violated, or the distance penalty is larger
	 than the best total penalty so far. */

      if (breaks[break_in_idx].flags & HNJ_JUST_FLAG_ISSPACE)
	total_space += breaks[break_in_idx].x1 - breaks[break_in_idx].x0;
      break_in_idx++;

      while (break_in_idx < n_breaks &&
	     breaks[break_in_idx].x0 + tab_offset <=
	     x + set_width + ((total_space * max_neg_space + 0x80) >> 8))
	{
	  /* Calculate penalty of this break. */
	  space_err = breaks[break_in_idx].x0 + tab_offset - (x + set_width);
	  penalty = space_err * space_err;

	  /* Check for a tab. */
	  if (breaks[break_in_idx].flags & HNJ_JUST_FLAG_ISTAB)
	    {
	      int next_stop = ((breaks[break_in_idx].x0 + tab_offset - x)
			       / tab_width + 1) * tab_width;
	      tab_offset = x + next_stop - breaks[break_in_idx].x0;
	      total_space = 0;
	    }

	  /* Continue penalty calculation. */
	  if (penalty > best_penalty)
	    break;
	  penalty += breaks[break_in_idx].penalty;
	  if (penalty <= best_penalty)
	    {
	      best_penalty = penalty;
	      best_idx = break_in_idx;
	    }

	  if (breaks[break_in_idx].flags & HNJ_JUST_FLAG_ISSPACE)
	    total_space += breaks[break_in_idx].x1 - breaks[break_in_idx].x0;
	  break_in_idx++;
	}

      result[result_idx++] = best_idx;
      x = breaks[best_idx].x1;
      break_in_idx = best_idx + 1;
    }

  return result_idx;
}
