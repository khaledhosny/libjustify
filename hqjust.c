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
/* High quality justification.

   This module implements justification on an entire paragraph basis.
   The input is a series of potential line breaks, as well as a set
   width. In the future, the set width may be implemented as a list of
   widths, one for each line, to support non-rectangular paragraphs.

   The basic approach is to use Dijkstra's shortest path graph
   algorithm to find the sequence of line breaks that results in
   least overall penalty for the paragraph. Each line break has its
   own penalty, and each line also has a penalty which depends on
   the variation from the ideal set width.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h> /* for fprintf debugging output */
#include "hqjust.h"

/* Intermediate data structures. */

typedef struct _Scratch Scratch;
typedef struct _QueueEntry QueueEntry;

#define HNJ_INF 0x7fffffff

/* Scratch area for each break. dist is the minimum distance (i.e.
   total penalty so far) from the beginning of the paragraph to this
   break, based on edges already visited (or HNJ_INF if the break has
   not yet been visited. pred is the predecessor of this break on such
   a shortest distance sequence.*/
struct _Scratch {
  int total_space;
  int dist;
  int pred;
  /* Indexes to potential breaks in next line to the left and right
     of the least deviation from ideal line width. All values between
     left and right (exclusive) have already been visited. -1 means
     unvisited. */
  int nl_left;
  int nl_right;
};


typedef enum {
  Q_VISIT,
  Q_LEFT,
  Q_RIGHT
} QueueType;

struct _QueueEntry {
  int dist;
  int break_idx;
  QueueType type;
};

/* Find the point at which deviation stops decreasing and starts
   increasing. For the returned break, the line is just too short,
   and for the next break, just too long. */
static int
find_min_dev_pt (int break_idx, const HnjBreak *breaks, int n_breaks,
		 const HnjParams *params)
{
  int i;
  int x;
  int set_width = params->set_width;
  int x_target;

  if (break_idx == -1)
    x = 0;
  else
    x = breaks[break_idx].x1;
  x_target = x + set_width;
  for (i = break_idx + 1; i < n_breaks; i++)
    if (breaks[i].x0 > x_target)
      break;
  return i - 1;
}

/* Return the square of the deviation for a break. This is one
   component of the penalty for a break (the other being the penalty
   field stored in the break itself). */
static int
dev2 (int x, int break_idx, const HnjBreak *breaks, const HnjParams *params)
{
  int set_width = params->set_width;
  int dev;

  if (!(breaks[break_idx].flags & (HNJ_JUST_FLAG_ISHYPHEN |
				   HNJ_JUST_FLAG_ISSPACE)))
    return 0;

  dev = breaks[break_idx].x0 - (x + set_width);
  /* todo: check for integer overflow */
  return dev * dev;
}

/* Free up ins_pt for insertion, q_end increments */
static void
queue_insert (QueueEntry *queue, int ins_pt, int q_end)
{
  int i;

  for (i = q_end; i > ins_pt; i--)
    queue[i] = queue[i - 1];
}

/* Insert based on distance, return ins point, q_end increments */
static int
queue_insert_dist (QueueEntry *queue, int dist, int q_beg, int q_end)
{
  int ins_pt;

  for (ins_pt = q_beg; ins_pt < q_end; ins_pt++)
    if (queue[ins_pt].dist > dist)
      break;
  queue_insert (queue, ins_pt, q_end);
  return ins_pt;
}

/* Change the dist on queue[pos] to dist, maintaining the queue invariant. */
static void
queue_move (QueueEntry *queue,
	    int old_dist, int break_idx, QueueType type,
	    int dist, int q_beg, int q_end)
{
  QueueEntry tmp;
  int i;
  int pos;

  for (pos = q_beg; pos < q_end; pos++)
    if (queue[pos].dist == old_dist && queue[pos].break_idx == break_idx &&
	queue[pos].type == type)
      break;

  if (pos == q_end)
    {
      fprintf (stderr, "queue_move: not found!\n");
      return;
    }

  tmp = queue[pos];
  if (pos > q_beg && queue[pos - 1].dist > dist)
    {
      /* it moves to the left */
      for (i = pos; i > q_beg && queue[i - 1].dist > dist; i--)
	queue[i] = queue[i - 1];
    }
  else
    {
      /* it moves to the right */
      for (i = pos; i + 1 < q_end && queue[i + 1].dist < dist; i++)
	queue[i] = queue[i + 1];
    }
  tmp.dist = dist;
  queue[i] = tmp;
}

/* Compute a high quality justification. The input is a sequence of
   potential line breaks as well as justification parameters. The
   result is a sequence of indices to the line breaks actually
   chosen. The return value is the length of the result sequence
   (i.e. [one less than] the number of lines in the paragraph).

   The resulting sequence minimizes the total penalty for the
   paragraph. */

int
hnj_hq_just (const HnjBreak *breaks, int n_breaks,
	     const HnjParams *params, int *result)
{
  Scratch *scratch;
  Scratch *s;
  int i;
  int min_dev_pt;
  QueueEntry *queue;
  int q_beg, q_end;
  int dist;
  int break_idx;
  int x_prev;
  int new_dist, new_break_idx;
  int ins_pt;
  QueueType type;
  int n_result;
  int total_space;
  int set_width = params->set_width;
  int max_neg_space = params->max_neg_space;

  scratch = malloc ((n_breaks + 1) * sizeof (Scratch));
  s = scratch + 1; /* so that s[-1] is valid */

  total_space = 0;
  for (i = 0; i < n_breaks; i++)
    {
      if (breaks[i].flags & HNJ_JUST_FLAG_ISSPACE)
	total_space += breaks[i].x1 - breaks[i].x0;
      s[i].total_space = total_space;
      s[i].dist = HNJ_INF;
      s[i].pred = -1;
    }

  s[-1].total_space = 0;
  s[-1].dist = 0;
  s[-1].pred = -1;

  queue = malloc ((n_breaks * 3 + 1) * sizeof (QueueEntry));
  q_beg = 0;
  q_end = 1;
  queue[0].dist = 0;
  queue[0].break_idx = -1;
  queue[0].type = Q_VISIT;

  while (q_beg != q_end) {
    dist = queue[q_beg].dist;
    break_idx = queue[q_beg].break_idx;
    type = queue[q_beg].type;
    switch (type) {
    case Q_VISIT:
      if (break_idx == n_breaks - 1)
	/* Reached the end! */
	goto done;
      q_beg++;
      if (break_idx == -1)
	x_prev = 0;
      else
	x_prev = breaks[break_idx].x1;

      min_dev_pt = find_min_dev_pt (break_idx, breaks, n_breaks, params);

      /* insert left scan */
      if (min_dev_pt > break_idx)
	{
	  new_dist = dist + dev2 (x_prev, min_dev_pt, breaks, params);
	  ins_pt = queue_insert_dist (queue, new_dist, q_beg, q_end++);
	  queue[ins_pt].dist = new_dist;
	  queue[ins_pt].break_idx = break_idx;
	  queue[ins_pt].type = Q_LEFT;
	  s[break_idx].nl_left = min_dev_pt;
	}

      /* insert right scan */
      total_space = s[min_dev_pt].total_space -
	s[break_idx].total_space;
      if (min_dev_pt + 1 < n_breaks &&
	  breaks[min_dev_pt + 1].x0 <=
	  x_prev + set_width + ((total_space * max_neg_space + 0x80) >> 8))
	{
	  new_dist = dist + dev2 (x_prev, min_dev_pt + 1, breaks, params);
	  ins_pt = queue_insert_dist (queue, new_dist, q_beg, q_end++);
	  queue[ins_pt].dist = new_dist;
	  queue[ins_pt].break_idx = break_idx;
	  queue[ins_pt].type = Q_RIGHT;
	  s[break_idx].nl_right = min_dev_pt + 1;
	}

#ifdef VERBOSE
      fprintf (stderr, "visit %d, dist %d, pred %d, min_dev_pt = %d\n",
	       break_idx, dist, s[break_idx].pred, min_dev_pt);
#endif
      break;
    case Q_LEFT:
    case Q_RIGHT:
      new_dist = dist + breaks[break_idx].penalty;
      if (type == Q_LEFT)
	new_break_idx = s[break_idx].nl_left;
      else
	new_break_idx = s[break_idx].nl_right;
#ifdef VERBOSE
      fprintf (stderr, "%s scan %d, new_break_idx = %d\n",
	       type == Q_LEFT ? "left": "right", break_idx, new_break_idx);
#endif
      if (s[new_break_idx].dist == HNJ_INF)
	{
#ifdef VERBOSE
	  fprintf (stderr, "inserting %d at dist %d\n",
		   new_break_idx, new_dist);
#endif
	  ins_pt = queue_insert_dist (queue, new_dist, q_beg, q_end++);
	  queue[ins_pt].dist = new_dist;
	  queue[ins_pt].break_idx= new_break_idx;
	  queue[ins_pt].type = Q_VISIT;
	  s[new_break_idx].dist = new_dist;
	  s[new_break_idx].pred = break_idx;
	}
      else if (new_dist < s[new_break_idx].dist)
	{
#ifdef VERBOSE
	fprintf (stderr, "reducing %d dist from %d to %d\n",
		 new_break_idx, s[new_break_idx].dist, new_dist);
#endif
	  queue_move (queue, s[new_break_idx].dist, new_break_idx, Q_VISIT,
		      new_dist, q_beg, q_end);
	  s[new_break_idx].dist = new_dist;
	  s[new_break_idx].pred = break_idx;
	}
      if (type == Q_LEFT)
	{
	  new_break_idx--;
	  s[break_idx].nl_left = new_break_idx;
	}
      else /* type == Q_RIGHT */
	{
	  total_space = s[new_break_idx].total_space -
	    s[break_idx].total_space;
	  new_break_idx++;
	  if (break_idx == -1)
	    x_prev = 0;
	  else
	    x_prev = breaks[break_idx].x1;
	  if (breaks[new_break_idx].x0 >
	      x_prev + set_width + ((total_space * max_neg_space + 0x80) >> 8))
	    new_break_idx = n_breaks;
	  s[break_idx].nl_right = new_break_idx;
	}
      if (new_break_idx > break_idx && new_break_idx < n_breaks)
	{
	  if (break_idx == -1)
	    x_prev = 0;
	  else
	    x_prev = breaks[break_idx].x1;
	  new_dist = s[break_idx].dist +
	    dev2 (x_prev, new_break_idx, breaks, params);
	  queue_move (queue, dist, break_idx, type, new_dist, q_beg, q_end);
	}
      else
	{
	  q_beg++;
	}
      break;
    default:
      assert(0); /* shouldn't reach here */
    }
  }

done:
  free (scratch);

  /* Read out the results (in reverse order) */
  for (n_result = 0; break_idx != -1; break_idx = s[break_idx].pred)
    n_result++;

  break_idx = n_breaks - 1;
  for (i = n_result - 1; i >= 0; i--)
    {
#ifdef VERBOSE
      fprintf (stderr, " %d", break_idx);
#endif
      result[i] = break_idx;
      break_idx = s[break_idx].pred;
    }
#ifdef VERBOSE
  fprintf (stderr, "\n");
#endif

  return n_result;
}
