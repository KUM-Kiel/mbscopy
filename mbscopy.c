/* Suche "\xf0\x00\x00\x20\x00\x01"
 * Adresse - 0x8000
 * unpack("L>L>")[1] * 512 Bytes lesen */

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char **argv)
{
  FILE *card = 0, *mbs_sys = 0, *mbs_data = 0;
  char str[512];
  int c, i, l;
  int64_t pos, size;
  char buffer[0x8000];
  char *this_disk, *file_name, *x;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s /dev/sdX\n", argv[0]);
    return 1;
  }

  /* Open the card. */
  card = fopen(argv[1], "rb");
  if (!card) {
    snprintf(str, sizeof(str), "/dev/%s", argv[1]);
    card = fopen(str, "rb");
  }
  if (!card) {
    fprintf(stderr, "Could not open »%s«. Sorry!\n", argv[1]);
    return 1;
  }

  /* Search for magic pattern. */
  i = 0;
  if (fseek(card, 0x10000, SEEK_SET) == -1) goto io_error;
  while ((c = getc(card)) != EOF) {
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
        if (pos == -1) goto io_error;
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
  if (fseek(card, pos - 0x8000, SEEK_SET) == -1) goto io_error;
  if (fread(buffer, 0x8000, 1, card) != 1) goto io_error;
  size = 0;
  for (i = 0; i < 4; ++i) {
    size <<= 8;
    size += buffer[7 - i];
  }
  if (size == 0) {
    fprintf(stderr, "The data seems pretty corrupted. That is quite problematic!\n");
    return 1;
  }

  /* Load MBS.SYS. */
  if (fseek(card, pos - 0x10000, SEEK_SET) == -1) goto io_error;
  if (fread(buffer, 0x8000, 1, card) != 1) goto io_error;
  if (memcmp(buffer, "\r\n\r\n\\\\\\\r\n\r\n", 11) == 0) {
    fprintf(stderr, "Found »MBS.SYS«.\n");
  } else {
    fprintf(stderr, "It seems there is no »MBS.SYS«. Not good!\n");
    return 1;
  }

  /* Parse MBS.SYS. */
  this_disk = strnstr(buffer, "[this_disk]\r\n", 0x8000);
  file_name = strnstr(buffer, "[file_name]\r\n", 0x8000);
  if (!this_disk || this_disk + 13 >= buffer + 0x8000) goto corrupted;
  this_disk += 13;
  if (!file_name || file_name + 13 >= buffer + 0x8000) goto corrupted;
  file_name += 13;

  x = memchr(this_disk, '\r', buffer + 0x8000 - this_disk);
  if (!x || x == this_disk || x - this_disk > 100) goto corrupted;
  l = x - this_disk;
  memcpy(str, this_disk, l);
  strcpy(str + l, ": ");

  file_name = strnstr(file_name, str, buffer + 0x8000 - file_name);
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
    fprintf(stderr, "Could not create »MBS.SYS«. Sorry!\n");
    return 1;
  }
  if (fwrite(buffer, 0x8000, 1, mbs_sys) != 1) goto io_error;
  fclose(mbs_sys);

  /* Write data to disk. */
  fprintf(stderr, "Found »%s«\n", str);
  mbs_data = fopen(str, "wb");
  if (!mbs_data) {
    fprintf(stderr, "Could not create »%s«. Sorry!\n", str);
    return 1;
  }
  if (fseek(card, pos - 0x8000, SEEK_SET) == -1) goto io_error;
  for (pos = 0; pos < size; ++pos) {
    if (fread(buffer, 512, 1, card) != 1) goto io_error;
    if (fwrite(buffer, 512, 1, mbs_data) != 1) goto io_error;
    if (pos % 1024 == 0) {
      fprintf(stderr, "\033[k%3d%% %4dMB\r", (int) (pos * 100 / size), (int) (pos * 512 / 1000000));
      fflush(stderr);
    }
  }
  fprintf(stderr, "\033[k%3d%% %4dMB\n", 100, (int) (size * 512 / 1000000));
  fclose(mbs_data);

  fclose(card);
  return 0;

io_error:
  fprintf(stderr, "\033[kThere was an I/O error. This is pretty bad!\n");
  return 1;

corrupted:
  fprintf(stderr, "Seems corrupted. Dangit!\n");
  return 1;
}
