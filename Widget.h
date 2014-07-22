/*
Copyright (c) 2014 Joe Davisson.

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

#ifndef WIDGET_H
#define WIDGET_H

#include "rendera.h"

class Widget : public Fl_Widget
{
public:
  Widget(Fl_Group *, int, int, int, int, const char *, const char *, int, int);
  Widget(Fl_Group *, int, int, int, int, const char *, int, int);
  virtual ~Widget();

  virtual int handle(int);
  int var;
  int stepx;
  int stepy;
  Fl_Group *group;
  Fl_RGB_Image *image;
  Bitmap *bitmap;
protected:
  virtual void draw();
};

#endif

