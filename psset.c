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


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "hyphen.h"
#include "hsjust.h"
#include "hqjust.h"
#include "hnjalloc.h"

#include "z_misc.h"
#include "parseAFM.h"
#include "namecontext.h"

typedef struct _PSContext PSContext;
typedef struct _LoadedFont LoadedFont;
typedef struct _MunchedFontInfo MunchedFontInfo;
typedef struct _KernPair KernPair;
typedef struct _LigList LigList;

typedef struct _PSOContext PSOContext;

#define SCALE 50
#define SAVE_KERN

struct _PSContext {
  NameContext *nc; /* the context for all names */
};

struct _LoadedFont {
  PSContext *psc;
  FontInfo *fi;
  MunchedFontInfo *mfi;
};

struct _LigList {
  LigList *next;
  unsigned char succ, lig;
};

/* The kern pair table is actually a hash table */
struct _MunchedFontInfo {
  int kern_pair_table_size;
  KernPair *kern_pair_table;
  int widths[256];
  int encoding[256];
  LigList *lig[256];
  unsigned char *rev_encoding;
};

struct _KernPair {
  NameId name1; /* or -1 if empty */
  NameId name2;
  int xamt; /* , yamt */
};

/* PostScript output context */
struct _PSOContext {
  FILE *f;
  LoadedFont *font;
  double fontsize;
  double linespace;
  double left;
  double right;
  double top;
  double bot;

  double y;
  int page_num;
};

#define kern_pair_hash(n1,n2) ((n1) * 367 + (n2))

/* some functions for dealing with defined fonts after evaluation of the
   font program is complete. */

void
scale_widths (FontInfo *fi, double fontsize, int widths[256])
{
  int i;
  double scale;
  int c;

  scale = fontsize * 0.001 * SCALE;

  for (i = 0; i < 256; i++)
    widths[i] = 0;

  for (i = 0; i < fi->numOfChars; i++)
    {
      c = fi->cmi[i].code;
      if (c >= 0 && c < 256)
	widths[c] = floor (fi->cmi[i].wx * scale + 0.5);
    }
}

unsigned char *
get_encodings (FontInfo *fi, NameContext *nc, int encoding[256])
{
  int i;
  int c;
  unsigned char *rev_encoding;

  rev_encoding = hnj_malloc (fi->numOfChars);
  for (i = 0; i < 256; i++)
    encoding[i] = -1;

  for (i = 0; i < fi->numOfChars; i++)
    rev_encoding[i] = 0;

  for (i = 0; i < fi->numOfChars; i++)
    {
      c = fi->cmi[i].code;
      if (c >= 0 && c < 256)
	{
	  encoding[c] = name_context_intern (nc, fi->cmi[i].name);
	  rev_encoding[encoding[c]] = c;
	}
    }
  return rev_encoding;
}

void
munch_lig_info (PSContext *psc, MunchedFontInfo *mfi, FontInfo *fi)
{
  int i;
  int c;
  int succ;
  int lig;
  LigList *ll;
  Ligature *ligs;

  for (i = 0; i < 256; i++)
    mfi->lig[i] = NULL;

  for (i = 0; i < fi->numOfChars; i++)
    {
      for (ligs = fi->cmi[i].ligs; ligs; ligs = ligs->next)
	{
	  c = fi->cmi[i].code;
	  succ = mfi->rev_encoding[name_context_intern (psc->nc, ligs->succ)];
	  lig = mfi->rev_encoding[name_context_intern (psc->nc, ligs->lig)];
	  /* predicate says all three characters are encoded in 8 bits */
	  if (!((c | succ | lig) & ~255))
	    {
	      ll = z_new (LigList, 1);
	      ll->succ = succ;
	      ll->lig = lig;
	      ll->next = mfi->lig[c];
#ifdef VERBOSE
	      printf ("%% lig %c%c -> %c\n", c, succ, lig);
#endif
	      mfi->lig[c] = ll;
	    }
	}
    }
}

MunchedFontInfo *
munch_font_info (PSContext *psc, FontInfo *fi, double fontsize)
{
  MunchedFontInfo *mfi;
  KernPair *table;
  int table_size;
  int i, j;
  NameId name1, name2;
  double scale;
  int c1, c2;

  scale = fontsize * 0.001 * SCALE;

  mfi = z_new (MunchedFontInfo, 1);

  scale_widths (fi, fontsize, mfi->widths);

  mfi->rev_encoding = get_encodings (fi, psc->nc, mfi->encoding);

  munch_lig_info (psc, mfi, fi);

  table_size = fi->numOfPairs << 1;
  mfi->kern_pair_table_size = table_size;
  table = z_new (KernPair, table_size);
  mfi->kern_pair_table = table;
  for (i = 0; i < mfi->kern_pair_table_size; i++)
    table[i].name1 = -1;

  /* Transfer afm kern pair information into the hash table,
     taking care to intern the names as we go. */
  for (i = 0; i < fi->numOfPairs; i++)
    {
      name1 = name_context_intern (psc->nc, fi->pkd[i].name1);
      name2 = name_context_intern (psc->nc, fi->pkd[i].name2);
      c1 = mfi->rev_encoding[name1];
      c2 = mfi->rev_encoding[name2];
      for (j = kern_pair_hash (c1, c2);
	   table[j % table_size].name1 != -1;
	   j++);
      j = j % table_size;
      table[j].name1 = c1;
      table[j].name2 = c2;
      table[j].xamt = floor (fi->pkd[i].xamt * scale + 0.5);
#ifdef VERBOSE
      printf ("%% %c%c %d\n", c1, c2, table[j].xamt);
#endif
    }

  return mfi;
}

/* allocate a new filename, same as the old one, but with the extension */
static char *
replace_extension (const char *filename, const char *ext)
{
  int i;
  int size_fn, size_ext;
  char *new_fn;

  size_fn = strlen (filename);
  size_ext = strlen (ext);
  for (i = size_fn - 1; i >= 0; i--)
    if (filename[i] == '.' || filename[i] == '/')
      break;
  if (filename[i] != '.')
    i = size_fn;
  new_fn = z_new (char, i + size_ext + 1);
  memcpy (new_fn, filename, i);
  memcpy (new_fn + i, ext, size_ext);
  new_fn[i + size_ext] = '\0';
  return new_fn;
}

LoadedFont *
load_afm (char *filename, double fontsize)
{
  LoadedFont *loaded_font;
  FILE *afm_f;
  int status;

  PSContext *psc;

  psc = z_new (PSContext, 1);
  psc->nc = name_context_new ();

  loaded_font = z_new (LoadedFont, 1);
  loaded_font->psc = psc;

  loaded_font->fi = NULL;
  loaded_font->mfi = NULL;
  afm_f = fopen (filename, "rb");
  if (afm_f != NULL)
    {
      status = parseFile (afm_f, &loaded_font->fi, P_ALL);
      fclose (afm_f);

      loaded_font->mfi = munch_font_info (psc, loaded_font->fi, fontsize);
    }

  return loaded_font;
}

/* get xamt of kern pair */
int
get_kern_pair (MunchedFontInfo *mfi, int glyph1, int glyph2)
{
  int i, idx;
  KernPair *table;
  int table_size;

  table_size = mfi->kern_pair_table_size;
  table = mfi->kern_pair_table;
  for (i = kern_pair_hash (glyph1, glyph2); idx = i % table_size,
	 table[idx].name1 != -1;
       i++)
    if (table[idx].name1 == glyph1 && table[idx].name2 == glyph2)
      return table[idx].xamt;
  return 0;
}

void
pso_begin_page (PSOContext *pso)
{
  fprintf (pso->f, "%%%%Page: %d %d\n", pso->page_num, pso->page_num);
}

void
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

void
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

void
pso_end_line (PSOContext *pso)
{
  pso->y -= pso->linespace;
  fprintf (pso->f, "\n");
}

void
pso_blank_line (PSOContext *pso)
{
  pso->y -= pso->linespace;
  fprintf (pso->f, "\n");
}

#ifdef SAVE_KERN
/* This includes kerning and ligatures! */
void
pso_show_word (PSOContext *pso, const char *word, int *kerns, z_boolean space)
{
  char new_word[256];
  int i, j;
  char c;
  int kern;
  MunchedFontInfo *mfi;
  LigList *lig;

  mfi = pso->font->mfi;
  j = 0;
  new_word[j++] = '(';
  for (i = 0; word[i]; i++)
    {
      c = word[i];
      lig = mfi->lig[(unsigned char)c];
      while (lig)
	{
	  if (lig->succ == word[i + 1])
	    {
	      c = lig->lig;
	      lig = mfi->lig[(unsigned char)c];
	      /* which is mfi->lig[lig->lig] :) */
	      i++;
	    }
	  else
	    lig = lig->next;
	}
      if (c == '(' || c == ')' || c == '\\')
	new_word[j++] = '\\';
      new_word[j++] = c;
      kern = kerns[i];
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
#else
/* This includes kerning! */
void
pso_show_word (PSOContext *pso, const char *word, z_boolean space)
{
  char new_word[256];
  int i, j;
  char c;
  MunchedFontInfo *mfi;
  int kern;

  mfi = pso->font->mfi;
  j = 0;
  new_word[j++] = '(';
  for (i = 0; word[i]; i++)
    {
      c = word[i];
      if (c == '(' || c == ')' || c == '\\')
	new_word[j++] = '\\';
      new_word[j++] = c;
      kern = get_kern_pair (mfi, c, word[i + 1]);
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
#endif

void
pso_hmoveto (PSOContext *pso, double dx)
{
  fprintf (pso->f, " %g 0 rmoveto", dx);
}

char *
strdup_from_buf (const char *buf, int size)
{
  char *new;

  new = hnj_malloc (size + 1);
  memcpy (new, buf, size);
  new[size] = '\0';
  return new;
}

#define noPRINT_HYPHENS

void
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
  int *widths;
  int word_offset;
  int n_space;
  int width;
  double space;
  MunchedFontInfo *mfi;
#ifdef SAVE_KERN
  int save_kern[65536];
  int word_k[16384];
  int k_idx;
#endif

  mfi = pso->font->mfi;
  widths = mfi->widths;
  hyphwidth = widths['-'];
  spacewidth = widths[' '];

#ifdef SAVE_KERN
  k_idx = 0;
#endif
  n_breaks = 0;
  x = 0;
  for (i = 0; i < n_words; i++)
    {
      l = strlen (words[i]);
      hnj_hyphen_hyphenate (dict, words[i], l, hbuf);
#ifdef SAVE_KERN
      word_k[i] = k_idx;
#endif
      for (j = 0; j < l; j++)
	{
#ifdef PRINT_HYPHENS
	  putchar (words[i][j]);
#endif
	  x += widths[words[i][j]];
	  if (hbuf[j] & 1)
	    {
#ifdef PRINT_HYPHENS
	      putchar ('-');
#endif
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
#ifdef SAVE_KERN
	      x += save_kern[k_idx++] =
		get_kern_pair (mfi, words[i][j], words[i][j + 1]);
#else
	      x += get_kern_pair (mfi, words[i][j], words[i][j + 1]);
#endif
	    }
	}
#ifdef PRINT_HYPHENS
      putchar (' ');
#endif
#ifdef SAVE_KERN
      save_kern[k_idx++] = 0;
#endif
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
      printf ("%% x0 = %d, x1 = %d%s\n", breaks[break_num].x0, breaks[break_num].x1, breaks[break_num].flags & HNJ_JUST_FLAG_ISHYPHEN ? " -" : "");
#endif
      if (n_space && ((breaks[break_num].flags & (HNJ_JUST_FLAG_ISHYPHEN |
						 HNJ_JUST_FLAG_ISSPACE)) ||
		      width + n_space * spacewidth > params->set_width))
	space = ((1.0 / SCALE) * (params->set_width - width)) / n_space;
      else
	space = spacewidth * (1.0 / SCALE);

#ifdef VERBOSE
      printf ("%% width=%d, n_space = %d\n", width, n_space);
#endif
      pso_begin_line (pso, space);

      for (; i < is[break_num]; i++)
	{
#ifdef SAVE_KERN
	  pso_show_word (pso, words[i] + word_offset, save_kern +
			 word_k[i] + word_offset, z_true);
#else
	  pso_show_word (pso, words[i] + word_offset, z_true);
#endif
	  word_offset = 0;
	}
      if (breaks[break_num].flags & HNJ_JUST_FLAG_ISSPACE)
	{
#ifdef SAVE_KERN
	  pso_show_word (pso, words[i] + word_offset, save_kern +
			 word_k[i] + word_offset, z_false);
#else
	  pso_show_word (pso, words[i] + word_offset, z_false);
#endif
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
#ifdef SAVE_KERN
	  save_kern[j] = 0;
	  pso_show_word (pso, new_word, save_kern +
			 word_k[i] + word_offset, z_true);
#else
	  pso_show_word (pso, new_word, z_true);
#endif
	  pso_end_line (pso);
	  word_offset = j;
	}
      else
	{
#ifdef SAVE_KERN
	  pso_show_word (pso, words[i] + word_offset, save_kern +
			 word_k[i] + word_offset, z_false);
#else
	  pso_show_word (pso, words[i] + word_offset, z_false);
#endif
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
  char *font_name = "Times-Roman";
  char *afm_fn = "ptmr8a.afm";
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

  pso.font = load_afm (afm_fn, pso.fontsize);

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
	    hnj_free (words[i]);
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
  return 0;
}
