/* Lukas Joeressen (c) 2016. */

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void io_error(void)
{
  fprintf(stderr, "There was an I/O error. This is pretty bad!\n");
  exit(1);
}

static int32_t ld_i32_le(const void *x)
{
  int32_t r = ((int8_t *) x)[3];
  r <<= 8; r += ((uint8_t *) x)[2];
  r <<= 8; r += ((uint8_t *) x)[1];
  r <<= 8; r += ((uint8_t *) x)[0];
  return r;
}

static const char *because(int e)
{
  switch (e) {
    case ENOENT: return " because it does not exist";
    case EACCES: return " because the permission was denied";
    default:     return "";
  }
}

static char *find(char *big, const char *little, size_t len)
{
  size_t i, l;
  if (!little) return big;
  l = strlen(little);
  for (i = 0; i < len - l; ++i) {
    if (memcmp(big + i, little, l) == 0) {
      return big + i;
    }
  }
  return 0;
}

int64_t find_mbs_sys(FILE *f)
{
  int64_t r;
  int s = 0, c;
  if (fseek(f, 0, SEEK_SET) == -1) io_error();
  while ((c = getc(f)) != EOF) {
    switch (c) {
      case '\r':
        if (s == 2 || s == 7 || s == 9) {
          s += 1;
        } else if (s == 4) {
          s = 3;
        } else {
          s = 1;
        }
        break;
      case '\n':
        if (s == 10) {
          r = ftell(f) - 11;
          if (r == -1) io_error();
          return r;
        } else if (s == 1 || s == 3 || s == 8) {
          s += 1;
        } else {
          s = 0;
        }
        break;
      case '\\':
        if (s == 4 || s == 5 || s == 6) {
          s += 1;
        } else {
          s = 0;
        }
        break;
      default:
        s = 0;
    }
  }
  return -1;
}

int main(int argc, char **argv)
{
  FILE *card = 0, *mbs_sys = 0, *mbs_data = 0;
  char str[512];
  int c, i, e;
  int64_t pos, size, l;
  char buffer[0x8000];
  char *this_disk, *file_name, *x;

  if (argc < 2) {
    fprintf(stderr,
      "mbscopy 1.1.0\n"
      "Lukas Joeressen (c) 2016\n"
      "This software is released under the GPLv3.\n"
      "\n"
      "Usage: %s /dev/sdX\n",
      argv[0]);
    return 1;
  }

  /* Open the card. */
  card = fopen(argv[1], "rb");
  if (!card) {
    e = errno;
    snprintf(str, sizeof(str), "/dev/%s", argv[1]);
    card = fopen(str, "rb");
  }
  if (!card) {
    fprintf(stderr, "Could not open »%s«%s. Sorry!\n", argv[1], because(e));
    return 1;
  }

  /* Search for magic pattern f0 00 00 20 00 01. */
  i = 0;
  pos = 0x10000;
  if (fseek(card, 0x10000, SEEK_SET) == -1) io_error();
  while ((c = getc(card)) != EOF) {
    if (pos++ > 0x100000) break;
    switch (c) {
    case 0xf0:
      i = 1;
      break;
    case 0x00:
      i = (i == 1 || i == 2 || i == 4) ? i + 1 : 0;
      break;
    case 0x20:
      i = (i == 3) ? 4 : 0;
      break;
    case 0x01:
      if (i == 5) {
        pos = ftell(card) - 6;
        if (pos == -1) io_error();
        goto found;
      }
    default:
      i = 0;
      break;
    }
  }
  fprintf(stderr, "Could not find any data. Sorry!\n");
  return 1;

found:
  /* Inspect header. */
  if (fseek(card, pos - 0x8000, SEEK_SET) == -1) io_error();
  if (fread(buffer, 0x8000, 1, card) != 1) io_error();
  size = ld_i32_le(buffer + 4);
  if (size <= 0) {
    fprintf(stderr, "The data seems pretty corrupted. That is quite problematic!\n");
    return 1;
  }

  /* Load MBS.SYS. */
  if (fseek(card, pos - 0x10000, SEEK_SET) == -1) io_error();
  if (fread(buffer, 0x8000, 1, card) != 1) io_error();
  if (memcmp(buffer, "\r\n\r\n\\\\\\\r\n\r\n", 11) == 0) {
    fprintf(stderr, "Found »MBS.SYS«.\n");
  } else if ((l = find_mbs_sys(card)) > 0) {
    if (fseek(card, l, SEEK_SET) == -1) io_error();
    if (fread(buffer, 0x8000, 1, card) != 1) io_error();
    fprintf(stderr, "Found »MBS.SYS«.\n");
  } else {
    fprintf(stderr, "It seems there is no »MBS.SYS«. Not good!\n");
    return 1;
  }

  /* Parse MBS.SYS. */
  this_disk = find(buffer, "[this_disk]\r\n", 0x8000);
  file_name = find(buffer, "[file_name]\r\n", 0x8000);
  if (!this_disk || this_disk + 13 >= buffer + 0x8000) goto corrupted;
  this_disk += 13;
  if (!file_name || file_name + 13 >= buffer + 0x8000) goto corrupted;
  file_name += 13;

  x = memchr(this_disk, '\r', buffer + 0x8000 - this_disk);
  if (!x || x == this_disk || x - this_disk > 100) goto corrupted;
  l = x - this_disk;
  memcpy(str, this_disk, l);
  strcpy(str + l, ": ");

  file_name = find(file_name, str, buffer + 0x8000 - file_name);
  if (!file_name) goto corrupted;
  file_name += l + 2;
  x = memchr(file_name, '\r', buffer + 0x8000 - file_name);
  if (!x || x == file_name || x - file_name > 100) goto corrupted;
  l = x - file_name;
  memcpy(str, file_name, l);
  str[l] = 0;

  /* Write MBS.SYS to disk. */
  mbs_sys = fopen("MBS.SYS", "wb");
  if (!mbs_sys) {
    fprintf(stderr, "Could not create »MBS.SYS«%s. Sorry!\n", because(errno));
    return 1;
  }
  x = memchr(buffer, 0, 0x8000);
  l = x ? x - buffer : 0x8000;
  if (fwrite(buffer, 0x8000, 1, mbs_sys) != 1) io_error();
  fclose(mbs_sys);

  /* Write data to disk. */
  fprintf(stderr, "Found »%s«\n", str);
  mbs_data = fopen(str, "wb");
  if (!mbs_data) {
    fprintf(stderr, "Could not create »%s«%s. Sorry!\n", str, because(errno));
    return 1;
  }
  if (fseek(card, pos - 0x8000, SEEK_SET) == -1) io_error();
  for (pos = 0; pos < size; ++pos) {
    if (fread(buffer, 512, 1, card) != 1) io_error();
    if (fwrite(buffer, 512, 1, mbs_data) != 1) io_error();
    if (pos % 1024 == 0) {
      fprintf(stderr, "%3d%% %6.1fMB     \r", (int) (pos * 100 / size), (double) pos * 512 / 1000000);
      fflush(stderr);
    }
  }
  fprintf(stderr, "%3d%% %6.1fMB     \n", 100, (double) size * 512 / 1000000);
  fclose(mbs_data);

  fclose(card);
  return 0;

corrupted:
  fprintf(stderr, "Seems corrupted. Dangit!\n");
  return 1;
}
