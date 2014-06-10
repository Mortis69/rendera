#include "rendera.h"
#include <jpeglib.h>
#include <setjmp.h>

extern Gui *gui;

int *preview_data = 0;

void load(Fl_Widget *, void *)
{
  Fl_Native_File_Chooser *fc = new Fl_Native_File_Chooser();
  fc->title("Load Image");
  fc->filter("JPEG Image\t*.{jpg,jpeg}\nBitmap Image\t*.bmp");
  fc->options(Fl_Native_File_Chooser::PREVIEW);
  fc->type(Fl_Native_File_Chooser::BROWSE_FILE);
  fc->show();

  const char *fn = fc->filename();
  FILE *in = fl_fopen(fn, "rb");
  if(!in)
    return;

  unsigned char header[2];
  if(fread(&header, 2, 1, in) != 1)
  {
    fclose(in);
    return;
  }

  fclose(in);

  if(memcmp(header, (const unsigned char[2]){ 0xff, 0xd8 }, 2) == 0)
    load_jpg(fn);
  else if(memcmp(header, "BM", 2) == 0)
    load_bmp(fn);
  else
    return;

  delete Map::main;
  Map::main = new Map(Bitmap::main->w, Bitmap::main->h);

  delete fc;

  gui->view->draw_main(1);
}

// jpeg structures
struct my_error_mgr
{
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

typedef struct my_error_mgr *my_error_ptr;

static void jpg_exit(j_common_ptr cinfo)
{
  my_error_ptr myerr = (my_error_ptr)cinfo->err;
  (*cinfo->err->output_message)(cinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

// bmp structures
#pragma pack(1)
typedef struct
{
  uint16_t bfType;
  uint32_t bfSize;
  uint16_t bfReserved1;
  uint16_t bfReserved2;
  uint32_t bfOffBits;
}
BITMAPFILEHEADER;

#pragma pack(1)
typedef struct
{
  uint32_t biSize;
  uint32_t biWidth;
  uint32_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  uint32_t biXPelsPerMeter;
  uint32_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
}
BITMAPINFOHEADER;

Fl_Image *preview_jpg(const char *fn, unsigned char *header, int len)
{
  // check file type
//  unsigned char jpeg_header[2] = { 0xff, 0xd8 };
//  if(memcmp(header, jpeg_header, 2) != 0)
  if(memcmp(header, (const unsigned char[2]){ 0xff, 0xd8 }, 2) != 0)
    return 0;

  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  JSAMPARRAY linebuf;
  int row_stride;

  FILE *in = fl_fopen(fn, "rb");
  if(!in)
    return 0;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpg_exit;

  if(setjmp(jerr.setjmp_buffer))
  {
    jpeg_destroy_decompress(&cinfo);
    fclose(in);
    return 0;
  }

  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, in);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);
  row_stride = cinfo.output_width * cinfo.output_components;
  linebuf = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

  int bytes = cinfo.out_color_components;

  int w = row_stride / bytes;
  int h = cinfo.output_height;

  if(w < 1 || h < 1)
    return 0;

  delete[] preview_data;
  preview_data = new int[w * h];
  Fl_RGB_Image *image = new Fl_RGB_Image((unsigned char *)preview_data, w, h, 4, 0);

  int x;
  int i;
  int *p = &preview_data[0];

  if(bytes == 3)
  {
    while(cinfo.output_scanline < cinfo.output_height)
    {
      jpeg_read_scanlines(&cinfo, linebuf, 1);
      for(x = 0; x < row_stride; x += 3)
      {
        *p++ = makecol((linebuf[0][x] & 0xFF),
                     (linebuf[0][x + 1] & 0xFF), (linebuf[0][x + 2]) & 0xFF);
      }
    }
  }
  else
  {
    while(cinfo.output_scanline < cinfo.output_height)
    {
      for(x = 0; x < row_stride; x += 1)
      {
        *p++ = makecol((linebuf[0][x] & 0xFF),
                     (linebuf[0][x] & 0xFF), (linebuf[0][x]) & 0xFF);
      }
    }
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  fclose(in);

  return image;
}

void load_jpg(const char *fn)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  JSAMPARRAY linebuf;
  int row_stride;

  FILE *in = fl_fopen(fn, "rb");
  if(!in)
    return;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpg_exit;

  if(setjmp(jerr.setjmp_buffer))
  {
    jpeg_destroy_decompress(&cinfo);
    fclose(in);
    return;
  }

  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, in);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);
  row_stride = cinfo.output_width * cinfo.output_components;
  linebuf = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

  int bytes = cinfo.out_color_components;

  int w = row_stride / bytes;
  int h = cinfo.output_height;

  int aw = w + 64;
  int ah = h + 64;

  delete Bitmap::main;
  Bitmap::main = new Bitmap(aw, ah);
  Bitmap::main->clear(makecol(0, 0, 0));
  Bitmap::main->set_clip(32, 32, aw - 32 - 1, ah - 32 - 1);

  int x;
  int *p = Bitmap::main->row[32] + 32;
  if(bytes == 3)
  {
    while(cinfo.output_scanline < cinfo.output_height)
    {
      jpeg_read_scanlines(&cinfo, linebuf, 1);
      for(x = 0; x < row_stride; x += 3)
      {
        *p++ = makecol((linebuf[0][x] & 0xFF),
                     (linebuf[0][x + 1] & 0xFF), (linebuf[0][x + 2]) & 0xFF);
      }

      p += 64;
    }
  }
  else
  {
    while(cinfo.output_scanline < cinfo.output_height)
    {
      for(x = 0; x < row_stride; x += 1)
      {
        *p++ = makecol((linebuf[0][x] & 0xFF),
                     (linebuf[0][x] & 0xFF), (linebuf[0][x]) & 0xFF);
      }

      p += 64;
    }
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  fclose(in);
}

void load_bmp(const char *fn)
{
puts("got here");
  FILE *in = fl_fopen(fn, "rb");
  if(!in)
    return;

  BITMAPFILEHEADER bh;
  BITMAPINFOHEADER bm;
//printf("%d\n", (int)sizeof(BITMAPFILEHEADER));
//printf("%d\n", (int)sizeof(BITMAPINFOHEADER));
//return;

  if(fread(&bh, sizeof(BITMAPFILEHEADER), 1, in) != 1)
  {
puts("header problem");
    fclose(in);
    return;
  }

  if(fread(&bm, sizeof(BITMAPINFOHEADER), 1, in) != 1)
  {
puts("info problem");
    fclose(in);
    return;
  }

  printf("%u\n", bm.biSize);
  printf("%u\n", bm.biWidth);
  printf("%u\n", bm.biHeight);
  printf("%u\n", bm.biPlanes);
  printf("%u\n", bm.biBitCount);
  printf("%u\n", bm.biCompression);
  printf("%u\n", bm.biSizeImage);
  printf("%u\n", bm.biXPelsPerMeter);
  printf("%u\n", bm.biYPelsPerMeter);
  printf("%u\n", bm.biClrUsed);
  printf("%u\n", bm.biClrImportant);

  int w = bm.biWidth;
  int h = bm.biHeight;
  int bits = bm.biBitCount;

  if(bits != 24)
  {
puts("bits problem");
/*
  uint32_t biSize;
  uint32_t biWidth;
  uint32_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  uint32_t biXPelsPerMeter;
  uint32_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
*/
    fclose(in);
    return;
  }

  // skip additional header info if it exists
  if(bm.biSize > 40)
    fseek(in, bm.biSize - 40, SEEK_CUR);

  //dpix = bm.biXPelsPerMeter / 39.370079 + .5;
  //dpiy = bm.biYPelsPerMeter / 39.370079 + .5;

  int mul = 3;
  int negx = 0, negy = 0;
  int pad = w % 4;

  if(w < 0)
    negx = 1;
  if(h >= 0)
    negy = 1;

  w = ABS(w);
  h = ABS(h);

  int aw = w + 64;
  int ah = h + 64;

  delete Bitmap::main;
  Bitmap::main = new Bitmap(aw, ah);
  Bitmap::main->clear(makecol(0, 0, 0));
  Bitmap::main->set_clip(32, 32, aw - 32 - 1, ah - 32 - 1);

  unsigned char *linebuf = new unsigned char[w * mul + pad];

  int x, y;

  for(y = 0; y < h; y++)
  {
    int y1 = negy ? h - 1 - y : y;
    y1 += 32;

    if(fread(linebuf, w * mul + pad, 1, in) != 1)
    {
      fclose(in);
      return;
    }
    else
    {
      int xx = 0;
      for(x = 0; x < w; x++)
      {
        int x1 = negx ? w - 1 - x : x;
        x1 += 32;
        *(Bitmap::main->row[y1] + x1) = makecol((linebuf[xx + 2] & 0xFF),
                     (linebuf[xx + 1] & 0xFF), (linebuf[xx + 0]) & 0xFF);
        xx += mul;
      }
    }
  }

  delete[] linebuf;
  fclose(in);
}

