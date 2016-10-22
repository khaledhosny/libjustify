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
/* A simple program to do PostScript h&j and setting of vanilla
   plaintext files. */

#define VERBOSE

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <hyphen.h>
#include "hsjust.h"
#include "hqjust.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H

typedef struct _PSOContext PSOContext;

#define SCALE 50

/* PostScript output context */
struct _PSOContext {
  FILE *f;
  FT_Face face;
  double fontsize;
  double linespace;
  double left;
  double right;
  double top;
  double bot;

  double y;
  int page_num;
};

/* get xamt of kern pair */
static int
get_kern_pair (PSOContext *pso, int c1, int c2)
{
  unsigned int glyph1, glyph2;
  FT_Vector kern;
  double scale;

  scale = pso->fontsize * 0.001 * SCALE;

  glyph1 = FT_Get_Char_Index (pso->face, c1);
  glyph2 = FT_Get_Char_Index (pso->face, c2);
  if (FT_Get_Kerning (pso->face, glyph1, glyph2, FT_KERNING_UNSCALED, &kern))
    return 0;
  if (kern.x)
    return floor (kern.x * scale + 0.5);

  return 0;
}

static int
get_width (PSOContext *pso, int c)
{
  unsigned int glyph;
  FT_Fixed advance;
  double scale;

  scale = pso->fontsize * 0.001 * SCALE;

  glyph = FT_Get_Char_Index (pso->face, c);
  if (FT_Get_Advance (pso->face, glyph, FT_LOAD_NO_SCALE, &advance))
    return 0;

  return floor (advance * scale + 0.5);

}

static void
pso_begin_page (PSOContext *pso)
{
  fprintf (pso->f, "%%%%Page: %d %d\n", pso->page_num, pso->page_num);
}

static void
pso_end_page (PSOContext *pso)
{
#if 0
  fprintf (pso->f, "0.25 setlinewidth\n");
  fprintf (pso->f, "%g %g moveto %g %g lineto stroke\n",
	   pso->left, pso->top, pso->left, pso->bot);
  fprintf (pso->f, "%g %g moveto %g %g lineto stroke\n",
	   pso->right, pso->top, pso->right, pso->bot);
#endif
  fprintf (pso->f, "showpage\n");
  pso->page_num++;
}

static void
pso_begin_line (PSOContext *pso, double space)
{
  if (pso->y < pso->bot + 0.34 * pso->fontsize)
    {
      pso_end_page (pso);
      pso->y = floor (pso->top - .66 * pso->fontsize);
      pso_begin_page (pso);
    }
  fprintf (pso->f, "%g %g *", pso->y, space);
}

static void
pso_end_line (PSOContext *pso)
{
  pso->y -= pso->linespace;
  fprintf (pso->f, "\n");
}

static void
pso_blank_line (PSOContext *pso)
{
  pso->y -= pso->linespace;
  fprintf (pso->f, "\n");
}

/* This includes kerning! */
static void
pso_show_word (PSOContext *pso, const char *word, bool space)
{
  char new_word[256];
  int i, j;
  char c;
  int kern;

  j = 0;
  new_word[j++] = '(';
  for (i = 0; word[i]; i++)
    {
      c = word[i];
      if (c == '(' || c == ')' || c == '\\')
	new_word[j++] = '\\';
      new_word[j++] = c;
      kern = get_kern_pair (pso, c, word[i + 1]);
      if (kern)
	  j += sprintf (new_word + j, ")%d %c(",
			kern > 0 ? kern : -kern,
			kern > 0 ? '+' : '-');
    }
  new_word[j++] = ')';
  new_word[j++] = space ? '_' : '|';
  new_word[j++] = 0;
  fputs (new_word, pso->f);
}

static void
pso_hmoveto (PSOContext *pso, double dx)
{
  fprintf (pso->f, " %g 0 rmoveto", dx);
}

static char *
strdup_from_buf (const char *buf, int size)
{
  char *new;

  new = malloc (size + 1);
  memcpy (new, buf, size);
  new[size] = '\0';
  return new;
}

static void
hnj (char **words, int n_words, HyphenDict *dict, HnjParams *params,
     PSOContext *pso)
{
  char hbuf[256];
  HnjBreak breaks[16384];
  int result[16384];
  int is[16384], js[16384];
  int n_breaks;
  int i, j;
  int x;
  int l;
  int n_actual_breaks;
  int line_num;
  int break_num;
  int hyphwidth;
  int spacewidth;
  char new_word[256];
  int word_offset;
  int n_space;
  int width;
  double space;

  hyphwidth = get_width (pso, '-');
  spacewidth = get_width (pso, ' ');

  n_breaks = 0;
  x = 0;

  for (i = 0; i < n_words; i++)
    {
      l = strlen (words[i]);
      if (dict)
        {
          char **rep = NULL;
          int *pos = NULL;
          int *cut = NULL;
          hnj_hyphen_hyphenate2 (dict, words[i], l, hbuf, NULL, &rep, &pos, &cut);
        }
      for (j = 0; j < l; j++)
	{
	  x += get_width (pso, words[i][j]);
	  if (dict && hbuf[j] & 1)
	    {
	      breaks[n_breaks].x0 = x + hyphwidth;
	      breaks[n_breaks].x1 = x;
	      breaks[n_breaks].penalty = 1000000;
	      breaks[n_breaks].flags = HNJ_JUST_FLAG_ISHYPHEN;
	      is[n_breaks] = i;
	      js[n_breaks] = j + 1;
	      n_breaks++;
	    }
	  if (words[i][j + 1])
	    {
	      x += get_kern_pair (pso, words[i][j], words[i][j + 1]);
	    }
	}
      breaks[n_breaks].x0 = x;
      x += spacewidth;
      breaks[n_breaks].x1 = x;
      breaks[n_breaks].penalty = 0;
      breaks[n_breaks].flags = HNJ_JUST_FLAG_ISSPACE;
      is[n_breaks] = i;
      js[n_breaks] = l;
      n_breaks++;
    }
  breaks[n_breaks - 1].flags = 0;
  n_actual_breaks = hnj_hq_just (breaks, n_breaks,
				 params, result);

  word_offset = 0;
  x = 0;
  i = 0;
  /* Now print the paragraph with the breaks present. */
  for (line_num = 0; line_num < n_actual_breaks; line_num++)
    {
      break_num = result[line_num];

      /* Calculate the width of the non-space section of the line */

      n_space = is[break_num] - i;

      width = (breaks[break_num].x0 - x) - n_space * spacewidth;
#ifdef VERBOSE
      fprintf (stderr, "%% x0 = %d, x1 = %d%s\n", breaks[break_num].x0, breaks[break_num].x1, breaks[break_num].flags & HNJ_JUST_FLAG_ISHYPHEN ? " -" : "");
#endif
      if (n_space && ((breaks[break_num].flags & (HNJ_JUST_FLAG_ISHYPHEN |
						 HNJ_JUST_FLAG_ISSPACE)) ||
		      width + n_space * spacewidth > params->set_width))
	space = ((1.0 / SCALE) * (params->set_width - width)) / n_space;
      else
	space = spacewidth * (1.0 / SCALE);

#ifdef VERBOSE
      fprintf (stderr, "%% width=%d, n_space = %d\n", width, n_space);
#endif
      pso_begin_line (pso, space);

      for (; i < is[break_num]; i++)
	{
	  pso_show_word (pso, words[i] + word_offset, true);
	  word_offset = 0;
	}
      if (breaks[break_num].flags & HNJ_JUST_FLAG_ISSPACE)
	{
	  pso_show_word (pso, words[i] + word_offset, false);
	  i++;
	  pso_end_line (pso);
	  word_offset = 0;
	}
      else if (breaks[break_num].flags & HNJ_JUST_FLAG_ISHYPHEN)
	{
	  j = js[break_num];
	  memcpy (new_word, words[i] + word_offset, j - word_offset);
	  new_word[j - word_offset] = '-';
	  new_word[j + 1 - word_offset] = 0;
	  pso_show_word (pso, new_word, true);
	  pso_end_line (pso);
	  word_offset = j;
	}
      else
	{
	  pso_show_word (pso, words[i] + word_offset, false);
	  pso_end_line (pso);
	  pso_blank_line (pso);
	}
      x = breaks[break_num].x1;
   }
}

int
main (int argc, char **argv)
{
#if 1
  char *font_fn = "NimbusRoman-Regular.t1";
  char *font_name = "Times-Roman";
  char *afm_fn = "NimbusRoman-Regular.afm";
#else
  char *font_name = "Helvetica";
  char *afm_fn = "phvr8a.afm";
#endif
  PSOContext pso;

  HyphenDict *dict;
  char buf[256];
  HnjParams params;
  char *words[2048];
  int i;
  int beg_word;
  int word_idx;

  pso.f = stdout;
  pso.fontsize = 12;
  pso.linespace = 14;
  pso.left = 72;
  pso.right = 540;
  pso.right = 72 + 216;
  pso.top = 720;
  pso.bot = 72;
  pso.y = floor (pso.top - .66 * pso.fontsize);

  FT_Library library;
  if (FT_Init_FreeType (&library))
    return 1;
  if (FT_New_Face (library, font_fn, 0, &pso.face))
    return 1;
  if (FT_Set_Char_Size (pso.face, pso.fontsize * SCALE, 0, 0, 0))
    return 1;
  if (FT_Attach_File (pso.face, afm_fn))
    return 1;

  pso.page_num = 1;

  fprintf (pso.f, "%%!PS-Adobe-3.0\n");
  fprintf (pso.f, "%% Here's my prolog\n"
	   "/| {show} bind def\n"
	   "/* {/sa exch def %g exch moveto} bind def\n"
	   "/_ {show sa 0 rmoveto} bind def\n"
	   "/+ {exch show %g mul 0 rmoveto} bind def\n"
	   "/- {exch show -%g mul 0 rmoveto} bind def\n"
	   "\n", pso.left, 1.0 / SCALE, 1.0 / SCALE);

  fprintf (pso.f, "/%s findfont %g scalefont setfont\n"
	   "\n",
	   font_name, pso.fontsize);

  params.set_width = floor ((pso.right - pso.left) * SCALE + 0.5);
  params.max_neg_space = 128;
  dict = hnj_hyphen_load ("hyphen.mashed");

  pso_begin_page (&pso);
  word_idx = 0;
  /* Parse a paragraph into the words data structures. */
  while (fgets (buf, sizeof(buf), stdin))
    {
      if (buf[0] == '\n' && word_idx > 0)
	{
	  hnj (words, word_idx, dict, &params, &pso);
	  for (i = 0; i < word_idx; i++)
	    free (words[i]);
	  word_idx = 0;
	}
      beg_word = 0;
      for (i = 0; i < sizeof(buf) && buf[i] != '\n'; i++)
	{
	  if (isspace (buf[i]))
	    {
	      if (i != beg_word)
		words[word_idx++] = strdup_from_buf (buf + beg_word,
						     i - beg_word);
	      beg_word = i + 1;
	    }
	}
      if (i < sizeof(buf) && i != beg_word)
	words[word_idx++] = strdup_from_buf (buf + beg_word,
					     i - beg_word);
    }
  if (word_idx > 0)
    hnj (words, word_idx, dict, &params, &pso);

  pso_end_page (&pso);
  fprintf (pso.f, "%%%%EOF\n");

  FT_Done_Face (pso.face);
  
  return 0;
}
