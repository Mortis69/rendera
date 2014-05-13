#include "rendera.h"

static inline int is_edge(Map *brush, int x, int y)
{
  if((brush->getpixel(x - 1, y) == 0xff) &&
     (brush->getpixel(x + 1, y) == 0xff) &&
     (brush->getpixel(x, y - 1) == 0xff) &&
     (brush->getpixel(x, y + 1) == 0xff))
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

Map::Map(int width, int height)
{
  int i;

  if(width < 1)
    width = 1;
  if(height < 1)
    height = 1;

  data = new unsigned char[width * height];
  row = new unsigned char[height];

  w = width;
  h = height;

  for(i = 0; i < height; i++)
    row[i] = width * i;
}

Map::~Map()
{
  delete row;
  delete data;
}

void Map::clear(int color)
{
  int i;

  for(i = 0; i < w * h; i++)
    data[i] = color & 0xff;
}

void Map::setpixel(int x, int y, int color)
{
  if(x < 0 || x >= w || y < 0 || y >= h)
    return;

  data[row[y] + x] = color & 0xff;
}

int Map::getpixel(int x, int y)
{
  if(x < 0 || x >= w || y < 0 || y >= h)
    return 0;

  return data[row[y] + x];
}

void Map::draw_brush(Map *brush, int color)
{
  int x, y;

  for(y = 0; y < brush->h; y++)
  {
    for(x = 0; x < brush->w; x++)
    {
      if(is_edge(brush, x, y))
        setpixel(x, y, color);
    }
  }
}

