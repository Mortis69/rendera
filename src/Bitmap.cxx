/*
Copyright (c) 2015 Joe Davisson.

This file is part of Rendera.

Rendera is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Rendera is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Rendera; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "Bitmap.H"
#include "Blend.H"
#include "Brush.H"
#include "Clone.H"
#include "Gamma.H"
#include "Inline.H"
#include "Map.H"
#include "Palette.H"
#include "Project.H"
#include "ExtraMath.H"
#include "Stroke.H"
#include "Tool.H"
#include "View.H"

namespace
{
  // XOR checkerboard pattern (for brushstroke previews)
  inline int xorValue(const int &x, const int &y)
  {
    static const int c[2] = { 0x00FFFFFF, 0x00808080 };
    return c[(x & 1) ^ (y & 1)];
  }

  // flood-fill related stack routines
  const int stack_size = 4096;
  std::vector<int> stack_x(stack_size);
  std::vector<int> stack_y(stack_size);
  int sp = 0;

  inline bool pop(int *x, int *y)
  {
    if(sp > 0)
    {
      *x = stack_x[sp];
      *y = stack_y[sp];
      sp--;
      return true;
    }

    return false;
  }

  inline bool push(const int &x, const int &y)
  {
    if(sp < stack_size - 1)
    {
      sp++;
      stack_x[sp] = x;
      stack_y[sp] = y;
      return true;
    }

    return false;
  }

  inline void emptyStack()
  {
    int x, y;

    while(pop(&x, &y))
    {
      // loop until pop returns false
    }
  }

  // determines if flood-fill is within range and sets transparency level
  inline bool inRange(const int &c1, const int &c2,
                      const int &range, int *trans)
  {
    int diff = std::sqrt(diff32(c1, c2)) / 2;

    *trans = diff * (256.0f / (range + 1));

    if(diff <= range)
      return true;
    else
      return false;
  }
}

// creates bitmap
Bitmap::Bitmap(int width, int height)
{
  if(width < 1)
    width = 1;
  if(height < 1)
    height = 1;

  data = new int [width * height];
  row = new int *[height];

  x = 0;
  y = 0;
  w = width;
  h = height;
  overscroll = 0;

  setClip(0, 0, w - 1, h - 1);

  for(int i = 0; i < height; i++)
    row[i] = &data[width * i];
}

// creates bitmap with a border
Bitmap::Bitmap(int width, int height, int overscroll)
{
  width += overscroll * 2;
  height += overscroll * 2;

  if(width < 1)
    width = 1;
  if(height < 1)
    height = 1;

  data = new int [width * height];
  row = new int *[height];

  x = 0;
  y = 0;
  w = width;
  h = height;
  this->overscroll = overscroll;

  for(int i = 0; i < height; i++)
    row[i] = &data[width * i];

  setClip(overscroll, overscroll, w - overscroll - 1, h - overscroll - 1);
  clear(getFltkColor(FL_BACKGROUND2_COLOR));
  rectfill(cl, ct, cr, cb, makeRgb(0, 0, 0), 0);
  setClip(0, 0, w - 1, h - 1);

  for(int i = 0; i < 4; i++)
  {
    rect(overscroll - 1 - i, overscroll - 1 - i,
         w - overscroll + i, h - overscroll + i,
         getFltkColor(FL_BACKGROUND_COLOR), 0);
  }

  setClip(overscroll, overscroll, w - overscroll - 1, h - overscroll - 1);
}

// creates bitmap from existing pixel data
Bitmap::Bitmap(int width, int height, int *image_data)
{
  if(width < 1)
    width = 1;
  if(height < 1)
    height = 1;

  data = image_data;
  row = new int *[height];

  x = 0;
  y = 0;
  w = width;
  h = height;
  overscroll = 0;

  setClip(0, 0, w - 1, h - 1);

  for(int i = 0; i < height; i++)
    row[i] = &data[width * i];
}

Bitmap::~Bitmap()
{
  delete[] row;
  delete[] data;
}

void Bitmap::clear(int c)
{
  for(int i = 0; i < w * h; i++)
    data[i] = c;
}

void Bitmap::hline(int x1, int y, int x2, int c, int t)
{
  if(x1 > x2)
    std::swap(x1, x2);

  if(y < ct || y > cb)
    return;
  if(x1 > cr)
    return;
  if(x2 < cl)
    return;

  clip(&x1, &y, &x2, &y);

  int *p = row[y] + x1;

  for(int x = x1; x <= x2; x++)
  {
    *p = Blend::current(*p, c, t);
    p++;
  }
}

void Bitmap::vline(int y1, int x, int y2, int c, int t)
{
  if(y1 < 0)
    y1 = 0;
  if(y2 > h - 1)
    y2 = h - 1;

  int *y = row[y2] + x;

  do
  {
    *y = Blend::current(*y, c, t);
    y -= w;
    y2--;
  }
  while(y2 >= y1);
}

void Bitmap::line(int x1, int y1, int x2, int y2, int c, int t)
{
  int dx = x2 - x1;
  int dy = y2 - y1;
  int inx = dx > 0 ? 1 : -1;
  int iny = dy > 0 ? 1 : -1;

  dx = ExtraMath::abs(dx);
  dy = ExtraMath::abs(dy);

  if(dx >= dy)
  {
    dy <<= 1;
    int e = dy - dx;
    dx <<= 1;

    while(x1 != x2)
    {
      setpixelSolid(x1, y1, c, t);

      if(e >= 0)
      {
        y1 += iny;
        e -= dx;
      }

      e += dy;
      x1 += inx;
    }
  }
  else
  {
    dx <<= 1;
    int e = dx - dy;
    dy <<= 1;

    while(y1 != y2)
    {
      setpixelSolid(x1, y1, c, t);

      if(e >= 0)
      {
        x1 += inx;
        e -= dy;
      }

      e += dx;
      y1 += iny;
    }
  }

  setpixelSolid(x1, y1, c, t);
}

void Bitmap::rect(int x1, int y1, int x2, int y2, int c, int t)
{
  if(x1 > x2)
    std::swap(x1, x2);
  if(y1 > y2)
    std::swap(y1, y2);

  if(x1 > cr)
    return;
  if(x2 < cl)
    return;
  if(y1 > cb)
    return;
  if(y2 < ct)
    return;

  clip(&x1, &y1, &x2, &y2);

  hline(x1, y1, x2, c, t);
  hline(x1, y2, x2, c, t);

  if(y1 == y2)
    return;

  for(int y = y1 + 1; y < y2; y++)
  {
    *(row[y] + x1) = Blend::current(*(row[y] + x1), c, t);
    *(row[y] + x2) = Blend::current(*(row[y] + x2), c, t);
  }
}

void Bitmap::rectfill(int x1, int y1, int x2, int y2, int c, int t)
{
  if(x1 > x2)
    std::swap(x1, x2);
  if(y1 > y2)
    std::swap(y1, y2);

  if(x1 > cr)
    return;
  if(x2 < cl)
    return;
  if(y1 > cb)
    return;
  if(y2 < ct)
    return;

  for(; y1 <= y2; y1++)
    hline(x1, y1, x2, c, t);
}

void Bitmap::xorLine(int x1, int y1, int x2, int y2)
{
  int dx = x2 - x1;
  int dy = y2 - y1;
  int inx = dx > 0 ? 1 : -1;
  int iny = dy > 0 ? 1 : -1;

  dx = ExtraMath::abs(dx);
  dy = ExtraMath::abs(dy);

  if(dx >= dy)
  {
    dy <<= 1;
    int e = dy - dx;
    dx <<= 1;

    while(x1 != x2)
    {
      *(row[y1] + x1) ^= xorValue(x1, y1);

      if(e >= 0)
      {
        y1 += iny;
        e -= dx;
      }

      e += dy;
      x1 += inx;
    }
  }
  else
  {
    dx <<= 1;
    int e = dx - dy;
    dy <<= 1;

    while(y1 != y2)
    {
      *(row[y1] + x1) ^= xorValue(x1, y1);

      if(e >= 0)
      {
        x1 += inx;
        e -= dy;
      }

      e += dx;
      y1 += iny;
    }
  }

  *(row[y1] + x1) ^= xorValue(x1, y1);
}

void Bitmap::xorHline(int x1, int y, int x2)
{
  if(x1 > x2)
    std::swap(x1, x2);

  if(y < ct || y > cb)
    return;
  if(x1 > cr)
    return;
  if(x2 < cl)
    return;

  clip(&x1, &y, &x2, &y);

  int *p = row[y] + x1;

  for(; x1 <= x2; x1++)
    *p++ ^= xorValue(x1, y);
}

void Bitmap::xorRect(int x1, int y1, int x2, int y2)
{
  if(x1 > x2)
    std::swap(x1, x2);
  if(y1 > y2)
    std::swap(y1, y2);

  if(x1 > cr)
    return;
  if(x2 < cl)
    return;
  if(y1 > cb)
    return;
  if(y2 < ct)
    return;

  clip(&x1, &y1, &x2, &y2);

  xorHline(x1, y1, x2);
  xorHline(x1, y2, x2);
  if(y1 == y2)
    return;

  y1++;

  for(; y1 < y2; y1++)
  {
    *(row[y1] + x1) ^= xorValue(x1, y1);
    *(row[y1] + x2) ^= xorValue(x1, y1);
  }
}

void Bitmap::xorRectfill(int x1, int y1, int x2, int y2)
{
  if(x1 > x2)
    std::swap(x1, x2);
  if(y1 > y2)
    std::swap(y1, y2);

  if(x1 > cr)
    return;
  if(x2 < cl)
    return;
  if(y1 > cb)
    return;
  if(y2 < ct)
    return;

  clip(&x1, &y1, &x2, &y2);

  for(; y1 <= y2; y1++)
    xorHline(x1, y1, x2);
}

// faster non-blending version
void Bitmap::setpixel(const int &x, const int &y, const int &c)
{
  *(row[y] + x) = c;
}

void Bitmap::setpixel(const int &x, const int &y, const int &c2, int t)
{
  Blend::target(this, Project::palette.get(), x, y);

  if(Project::brush->alpha_mask == 1)
    t = scaleVal(t, geta(getpixel(x, y)));
  else if(Project::brush->alpha_mask == 2)
    t = scaleVal(t, 255 - geta(getpixel(x, y)));

  switch(Clone::wrap | (Clone::active << 1))
  {
    case 0:
      setpixelSolid(x, y, c2, t);
      break;
    case 1:
      setpixelWrap(x, y, c2, t);
      break;
    case 2:
      setpixelClone(x, y, c2, t);
      break;
    case 3:
      setpixelWrapClone(x, y, c2, t);
      break;
    default:
      setpixelSolid(x, y, c2, t);
      break;
  }
}

void Bitmap::setpixelSolid(const int &x, const int &y,
                           const int &c2, const int &t)
{
  if(x < cl || x > cr || y < ct || y > cb)
    return;

  int *c1 = row[y] + x;

  *c1 = Blend::current(*c1, c2, t);
}

void Bitmap::setpixelWrap(const int &x, const int &y,
                          const int &c2, const int &t)
{
  int x1 = x; 
  int y1 = y; 

  while(x1 < cl)
    x1 += cw;
  while(x1 > cr)
    x1 -= cw;
  while(y1 < ct)
    y1 += ch;
  while(y1 > cb)
    y1 -= ch;

  int *c1 = row[y1] + x1;

  *c1 = Blend::current(*c1, c2, t);
}

void Bitmap::setpixelClone(const int &x, const int &y,
                           const int &, const int &t)
{
  if(x < cl || x > cr || y < ct || y > cb)
    return;

  int *c1 = row[y] + x;

  int x1 = x - Clone::dx;
  int y1 = y - Clone::dy;

  const int w1 = w - 1;
  const int h1 = h - 1;

  switch(Clone::mirror)
  {
    case 0:
      break;
    case 1:
      x1 = (w1 - x1) - (w1 - Clone::x * 2);
      break;
    case 2:
      y1 = (h1 - y1) - (h1 - Clone::y * 2);
      break;
    case 3:
      x1 = (w1 - x1) - (w1 - Clone::x * 2);
      y1 = (h1 - y1) - (h1 - Clone::y * 2);
      break;
  }

  Stroke *stroke = Project::stroke.get();

  int c2;

  if(x1 > stroke->x1 && x1 < stroke->x2 &&
     y1 > stroke->y1 && y1 < stroke->y2)
  {
    c2 = Clone::bmp->getpixel(x1 - stroke->x1 - 1, y1 - stroke->y1 - 1);
  }
  else
  {
    c2 = getpixel(x1, y1);
  }

  *c1 = Blend::current(*c1, c2, t);
}

void Bitmap::setpixelWrapClone(const int &x, const int &y,
                               const int &, const int &t)
{
  int x1 = x; 
  int y1 = y;

  while(x1 < cl)
    x1 += cw;
  while(x1 > cr)
    x1 -= cw;
  while(y1 < ct)
    y1 += ch;
  while(y1 > cb)
    y1 -= ch;

  int *c1 = row[y1] + x1;

  x1 -= Clone::dx;
  y1 -= Clone::dy;

  const int w1 = w - 1;
  const int h1 = h - 1;

  switch(Clone::mirror)
  {
    case 0:
      break;
    case 1:
      x1 = (w1 - x1) - (w1 - Clone::x * 2);
      break;
    case 2:
      y1 = (h1 - y1) - (h1 - Clone::y * 2);
      break;
    case 3:
      x1 = (w1 - x1) - (w1 - Clone::x * 2);
      y1 = (h1 - y1) - (h1 - Clone::y * 2);
      break;
  }

  while(x1 < cl)
    x1 += cw;
  while(x1 > cr)
    x1 -= cw;
  while(y1 < ct)
    y1 += ch;
  while(y1 > cb)
    y1 -= ch;

  Stroke *stroke = Project::stroke.get();

  int c2;

  if(x1 > stroke->x1 && x1 < stroke->x2 &&
     y1 > stroke->y1 && y1 < stroke->y2)
  {
    c2 = Clone::bmp->getpixel(x1 - stroke->x1 - 1, y1 - stroke->y1 - 1);
  }
  else
  {
    c2 = getpixel(x1, y1);
  }

  *c1 = Blend::current(*c1, c2, t);
}

int Bitmap::getpixel(int x, int y)
{
  if(x < cl)
    x = cl;
  if(x > cr)
    x = cr;
  if(y < ct)
    y = ct;
  if(y > cb)
    y = cb;

  return *(row[y] + x);
}

// clips coordinates to the writable image area
void Bitmap::clip(int *x1, int *y1, int *x2, int *y2)
{
  if(*x1 < cl)
    *x1 = cl;
  if(*y1 < ct)
    *y1 = ct;
  if(*x2 > cr)
    *x2 = cr;
  if(*y2 > cb)
    *y2 = cb;
}

// sets the image's writeable area
void Bitmap::setClip(int x1, int y1, int x2, int y2)
{
  cl = x1;
  ct = y1;
  cr = x2;
  cb = y2;
  cw = (cr - cl) + 1;
  ch = (cb - ct) + 1;
}

// copies part of one image to another, performs clipping
void Bitmap::blit(Bitmap *dest, int sx, int sy, int dx, int dy, int ww, int hh)
{
  if((sx >= w) || (sy >= h) || (dx > dest->cr) || (dy > dest->cb))
    return;

  if(sx < 0)
  {
    ww += sx;
    dx -= sx;
    sx = 0;
  }

  if(sy < 0)
  {
    hh += sy;
    dy -= sy;
    sy = 0;
  }

  if((sx + ww) > w)
    ww = w - sx;

  if((sy + hh) > h)
    hh = h - sy;

  if(dx < dest->cl)
  {
    dx -= dest->cl;
    ww += dx;
    sx -= dx;
    dx = dest->cl;
  }

  if(dy < dest->ct)
  {
    dy -= dest->ct;
    hh += dy;
    sy -= dy;
    dy = dest->ct;
  }

  if((dx + ww - 1) > dest->cr)
    ww = dest->cr - dx + 1;

  if((dy + hh - 1) > dest->cb)
    hh = dest->cb - dy + 1;

  if(ww < 1 || hh < 1)
    return;

  int sy1 = sy;
  int dy1 = dy;

  for(int y = 0; y < hh; y++)
  {
    int *sx1 = sx + row[sy1];
    int *dx1 = dx + dest->row[dy1];

    for(int x = 0; x < ww; x++, sx1++, dx1++)
      *dx1 = *sx1;

    sy1++;
    dy1++;
  }
}

// same as blit, but draws using the current blending mode and transparency
void Bitmap::drawBrush(Bitmap *dest,
                       int sx, int sy, int dx, int dy, int ww, int hh)
{
  if((sx >= w) || (sy >= h) || (dx > dest->cr) || (dy > dest->cb))
    return;

  if(sx < 0)
  {
    ww += sx;
    dx -= sx;
    sx = 0;
  }

  if(sy < 0)
  {
    hh += sy;
    dy -= sy;
    sy = 0;
  }

  if((sx + ww) > w)
    ww = w - sx;

  if((sy + hh) > h)
    hh = h - sy;

  if(dx < dest->cl)
  {
    dx -= dest->cl;
    ww += dx;
    sx -= dx;
    dx = dest->cl;
  }

  if(dy < dest->ct)
  {
    dy -= dest->ct;
    hh += dy;
    sy -= dy;
    dy = dest->ct;
  }

  if((dx + ww - 1) > dest->cr)
    ww = dest->cr - dx + 1;

  if((dy + hh - 1) > dest->cb)
    hh = dest->cb - dy + 1;

  if(ww < 1 || hh < 1)
    return;

  Blend::set(Project::brush->blend);

  for(int y = 0; y < hh; y++)
  {
    for(int x = 0; x < ww; x++)
    {
      const int c = getpixel(sx + x, sy + y);
      dest->setpixelSolid(dx + x, dy + y, c | 0xFF000000,
                     scaleVal(255 - geta(c), Project::brush->trans));
    }
  }

  Blend::set(Blend::TRANS);
}

// draws the main viewport
void Bitmap::pointStretch(Bitmap *dest,
                          int sx, int sy, int sw, int sh,
                          int dx, int dy, int dw, int dh,
                          int overx, int overy, bool bgr_order)
{
  Palette *pal = Project::palette.get();

  const int ax = ((float)dw / sw) * 256;
  const int ay = ((float)dh / sh) * 256;
  const int bx = ((float)sw / dw) * 256;
  const int by = ((float)sh / dh) * 256;

  dw -= overx;
  dh -= overy;

  if(dx < dest->cl)
  {
    const int d = dest->cl - dx;
    dx = dest->cl;
    dw -= d;
    sx += (d * ax) >> 8;
    sw -= (d * ax) >> 8;
  }

  if(dx + dw > dest->cr)
  {
    const int d = dx + dw - dest->cr;
    dw -= d;
    sw -= (d * ax) >> 8;
  }

  if(dy < dest->ct)
  {
    const int d = dest->ct - dy;
    dy = dest->ct;
    dh -= d;
    sy += (d * ay) >> 8;
    sh -= (d * ay) >> 8;
  }

  if(dy + dh > dest->cb)
  {
    const int d = dy + dh - dest->cb;
    dh -= d;
    sh -= (d * ay) >> 8;
  }

  dw -= (dw - ((sw * ax) >> 8));
  dh -= (dh - ((sh * ay) >> 8));

  if(sw < 1 || sh < 1)
    return;

  if(dw < 1 || dh < 1)
    return;

  for(int y = 0; y < dh; y++)
  {
    const int y1 = sy + ((y * by) >> 8);
    int *p = dest->row[dy + y] + dx;

    for(int x = 0; x < dw; x++)
    {
      const int x1 = sx + ((x * bx) >> 8);
      const int c = *(row[y1] + x1);
      // generate checkboard pattern for transparent areas
      const int checker = ((x >> 4) & 1) ^ ((y >> 4) & 1) ? 0xA0A0A0 : 0x606060;

      *p++ = convertFormat(blendFast(checker, c, 255 - geta(c)), bgr_order);
    }
  }
}

void Bitmap::flipHorizontal()
{
  for(int y = 0; y < h; y++)
  {
    for(int x = 0; x < w / 2; x++)
    {
      const int temp = *(row[y] + x);
      *(row[y] + x) = *(row[y] + w - 1 - x);
      *(row[y] + w - 1 - x) = temp;
    }
  }
}

void Bitmap::flipVertical()
{
  for(int y = 0; y < h / 2; y++)
  {
    for(int x = 0; x < w; x++)
    {
      const int temp = *(row[y] + x);
      *(row[y] + x) = *(row[h - 1 - y] + x);
      *(row[h - 1 - y] + x) = temp;
    }
  }
}

void Bitmap::rotate90()
{
  const int tw = w;
  const int th = h;

  // make copy
  Bitmap temp(tw, th);
  blit(&temp, 0, 0, 0, 0, tw, th);

  // re-allocate a rotated bitmap
  free(this);
  new Bitmap(th, tw);

  for(int yy = 0; yy < tw; yy++)
  {
    for(int xx = 0; xx < th; xx++)
    {
      *(row[yy] + xx) = *(temp.row[xx] + yy);
    }
  }

  setClip(overscroll, overscroll, w - overscroll - 1, h - overscroll - 1);
}

void Bitmap::rotate180()
{
  const int size = w * h;

  for(int i = 0; i < size / 2; i++)
  {
    const int temp = data[i];
    data[i] = data[size - 1 - i];
    data[size - 1 - i] = temp;
  }
}

// bresenham stretching, used for the navigator preview image
void Bitmap::fastStretch(Bitmap *dest,
                         int xs1, int ys1, int xs2, int ys2,
                         int xd1, int yd1, int xd2, int yd2, bool bgr_order)
{
  Palette *pal = Project::palette.get();

  xs2 += xs1;
  xs2--;
  ys2 += ys1;
  ys2--;
  xd2 += xd1;
  xd2--;
  yd2 += yd1;
  yd2--;

  const int dx = ExtraMath::abs(yd2 - yd1);
  const int dy = ExtraMath::abs(ys2 - ys1) << 1;
  const int sx = ExtraMath::sign(yd2 - yd1);
  const int sy = ExtraMath::sign(ys2 - ys1);
  const int dx2 = dx << 1;

  int e = dy - dx;

  for(int d = 0; d <= dx; d++)
  {
    const int dx_1 = ExtraMath::abs(xd2 - xd1);
    const int dy_1 = ExtraMath::abs(xs2 - xs1) << 1;
    const int sx_1 = ExtraMath::sign(xd2 - xd1);
    const int sy_1 = ExtraMath::sign(xs2 - xs1);
    const int dx2_1 = dx_1 << 1;

    int e_1 = dy_1 - dx_1;
    int *p = dest->row[yd1] + xd1;
    int *q = row[ys1] + xs1;

    for(int d_1 = 0; d_1 <= dx_1; d_1++)
    {
      // generate checkboard pattern for transparent areas
      const int checker = ((d_1 >> 4) & 1) ^ ((yd1 >> 4) & 1)
                            ? 0xA0A0A0 : 0x606060;

      *p = convertFormat(blendFast(checker, *q, 255 - geta(*q)), bgr_order);

      while(e_1 >= 0)
      {
        q += sy_1;
        e_1 -= dx2_1;
      }

      p += sx_1;
      e_1 += dy_1;
    }

    while(e >= 0)
    {
      ys1 += sy;
      e -= dx2;
    }

    yd1 += sx;
    e += dy;
  }
}

// scales an image with bilinear filtering and gamma-correction
void Bitmap::scale(Bitmap *dest)
{
  const int sx = cl;
  const int sy = ct;
  const int sw = cw;
  const int sh = ch;
  const int dx = dest->cl;
  const int dy = dest->ct;
  const int dw = dest->cw;
  const int dh = dest->ch;

  const float ax = ((float)sw / dw);
  const float ay = ((float)sh / dh);

  if(sw < 1 || sh < 1)
    return;

  if(dw < 1 || dh < 1)
    return;

  for(int y = 0; y < dh; y++) 
  {
    int *d = dest->row[dy + y] + dx;
    const float vv = (y * ay);
    const int v1 = vv;
    const float v = vv - v1;

    if(sy + v1 >= h - 1)
      break;

    int v2 = v1 + 1;

    if(v2 >= sh)
    {
      v2--;
    }

    int *c[4];
    c[0] = c[1] = row[sy + v1] + sx;
    c[2] = c[3] = row[sy + v2] + sx;

    for(int x = 0; x < dw; x++) 
    {
      const float uu = (x * ax);
      const int u1 = uu;
      const float u = uu - u1;

      if(sx + u1 >= w - 1)
        break;

      int u2 = u1 + 1;

      if(u2 >= sw)
      {
        u2--;
      }

      c[0] += u1;
      c[1] += u2;
      c[2] += u1;
      c[3] += u2;

      float f[4];

      f[0] = (1.0f - u) * (1.0f - v);
      f[1] = u * (1.0f - v);
      f[2] = (1.0f - u) * v;
      f[3] = u * v;

      float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;

      for(int i = 0; i < 4; i++)
      {
        rgba_type rgba = getRgba(*c[i]);
        r += (float)Gamma::fix(rgba.r) * f[i];
        g += (float)Gamma::fix(rgba.g) * f[i];
        b += (float)Gamma::fix(rgba.b) * f[i];
        a += rgba.a * f[i];
      }

      r = Gamma::unfix((int)r);
      g = Gamma::unfix((int)g);
      b = Gamma::unfix((int)b);

      *d++ = makeRgba((int)r, (int)g, (int)b, (int)a);

      c[0] -= u1;
      c[1] -= u2;
      c[2] -= u1;
      c[3] -= u2;
    }
  }
}

void Bitmap::invert()
{
  for(int i = 0; i < w * h; i++)
  {
    rgba_type rgba = getRgba(data[i]);
    data[i] = makeRgba(255 - rgba.r, 255 - rgba.g, 255 - rgba.b, rgba.a);
  }
}

// flood-fill with range option
void Bitmap::fill(int x, int y, int new_color, int old_color, int range)
{
  if(old_color == new_color)
    return;

  emptyStack();
    
  if(!push(x, y))
    return;

  // make copy
  Bitmap temp(w, h);
  blit(&temp, 0, 0, 0, 0, w, h);

  // blending map
  Map map(w, h);
  map.clear(255);

  int trans;

  while(pop(&x, &y))
  {    
    int x1 = x;

    while(x1 >= cl && inRange(temp.getpixel(x1, y), old_color, range, &trans))
      x1--;

    x1++;

    bool span_t = false;
    bool span_b = false;

    while(x1 <= cr && inRange(temp.getpixel(x1, y), old_color, range, &trans))
    {
      temp.setpixelSolid(x1, y, new_color, 0);
      map.setpixel(x1, y, trans);

      if((!span_t && y > ct) &&
          inRange(temp.getpixel(x1, y - 1), old_color, range, &trans)) 
      {
        if(!push(x1, y - 1))
          return;

        span_t = true;
      }
      else if((span_t && y > ct) &&
               !inRange(temp.getpixel(x1, y - 1), old_color,range, &trans))
      {
        span_t = false;
      }

      if((!span_b && y < cb) &&
          inRange(temp.getpixel(x1, y + 1), old_color,range, &trans)) 
      {
        if(!push(x1, y + 1))
          return;

        span_b = true;
      }
      else if((span_b && y < cb) &&
               !inRange(temp.getpixel(x1, y + 1), old_color,range, &trans))
      {
        span_b = false;
      } 

      x1++;
    }
  }

  for(int y = ct; y <= cb; y++)
  {
    for(int x = cl; x <= cr; x++)
    {
      const int t = map.getpixel(x, y);
      if(t < 255)
        setpixelSolid(x, y, new_color, t);
    }
  }
}

