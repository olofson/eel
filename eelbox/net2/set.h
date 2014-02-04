#ifndef _SET_H_
#define _SET_H_
/*
    NET2 is a threaded, event based, network IO library for SDL.
    Copyright (C) 2002 Bob Pendleton

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public License
    as published by the Free Software Foundation; either version 2.1
    of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA

    If you do not wish to comply with the terms of the LGPL please
    contact the author as other terms are available for a fee.

    Bob Pendleton
    Bob@Pendleton.com
*/

//
// simple set data type
//

#define SETTYPE(NAME, TYPE)  \
typedef struct  \
{  \
  int itr;  \
  int last;  \
  int size;  \
  TYPE *set;  \
} NAME##Set;

#define SETFORWARD(MODIFIER, NAME, TYPE)  \
  \
MODIFIER int init##NAME##Set(NAME##Set *s, int size);  \
MODIFIER void finit##NAME##Set(NAME##Set *s);  \
MODIFIER int member##NAME##Set(NAME##Set *s, TYPE *v);  \
MODIFIER TYPE *item##NAME##Set(NAME##Set *s, TYPE *v);  \
MODIFIER int add##NAME##Set(NAME##Set *s, TYPE *v);  \
MODIFIER int del##NAME##Set(NAME##Set *s, TYPE *v);  \
MODIFIER int first##NAME##Set(NAME##Set *s, TYPE *v);  \
MODIFIER int next##NAME##Set(NAME##Set *s, TYPE *v);  \

#define SETCODE(MODIFIER, NAME, TYPE, INCREMENT, TESTEQ)  \
  \
MODIFIER int init##NAME##Set(NAME##Set *s, int size)  \
{  \
  s->itr = 0;  \
  s->last = 0;  \
  s->size = size;  \
  s->set = malloc(size * sizeof(TYPE));  \
  if (NULL == s->set)  \
  {  \
    return -1;  \
  }  \
  \
  return 0;  \
}  \
  \
MODIFIER void finit##NAME##Set(NAME##Set *s)  \
{  \
  s->itr = 0;  \
  s->last = 0;  \
  s->size = 0;  \
  if (NULL != s->set)  \
  {  \
    free(s->set);  \
  }  \
  s->set = NULL;  \
}  \
  \
MODIFIER int member##NAME##Set(NAME##Set *s, TYPE *v)  \
{  \
  int i;  \
  for (i = 0; i < s->last; i++)  \
  {  \
    TYPE *a = v;  \
    TYPE *b = &(s->set[i]);  \
    if (TESTEQ)  \
    {  \
      return i;  \
    }  \
  }  \
  return -1;  \
}  \
  \
  \
MODIFIER TYPE *item##NAME##Set(NAME##Set *s, TYPE *v)  \
{  \
  int ind = member##NAME##Set(s, v);  \
  if (-1 != ind)  \
  {  \
    return &(s->set[ind]);  \
  }  \
  \
  return NULL;  \
}  \
  \
MODIFIER int add##NAME##Set(NAME##Set *s, TYPE *v)  \
{  \
  TYPE *nset = NULL;  \
  int nsize = 0;  \
  int i;  \
  \
  if (-1 != member##NAME##Set(s, v))  \
  {  \
    return 0;  \
  }  \
  \
  if (s->last < s->size)  \
  {  \
    s->set[s->last] = *v;  \
    s->last++;  \
  \
    return 0;  \
  }  \
  \
  nsize = s->size + INCREMENT;  \
  nset = malloc(nsize * sizeof(TYPE));  \
  if (NULL == nset)  \
  {  \
    return -1;  \
  }  \
  \
  for (i = 0; i < s->last; i++)  \
  {  \
    nset[i] = s->set[i];  \
  }  \
  \
  free(s->set);  \
  s->set = nset;  \
  s->size = nsize;  \
  \
  return add##NAME##Set(s, v);  \
}  \
  \
MODIFIER int del##NAME##Set(NAME##Set *s, TYPE *v)  \
{  \
  int ind = member##NAME##Set(s, v);  \
  if (-1 != ind)  \
  {  \
    (s->last)--;  \
    s->set[ind] = s->set[s->last];  \
    return 0;  \
  }  \
  \
  return -1;  \
}  \
  \
MODIFIER int first##NAME##Set(NAME##Set *s, TYPE *v)  \
{  \
  s->itr = 0;  \
  if (s->itr < s->last)  \
  {  \
    *v = s->set[s->itr];  \
    return 0;  \
  }  \
  \
  return -1;  \
}  \
  \
MODIFIER int next##NAME##Set(NAME##Set *s, TYPE *v)  \
{  \
  (s->itr)++;  \
  if (s->itr < s->last)  \
  {  \
    *v = s->set[s->itr];  \
    return 0;  \
  }  \
  \
  return -1;  \
}

#endif
