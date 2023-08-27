/*
*************************************************************************

        ╔═══════════════════════════════════════════════════╗
        ║                                                   ║
        ║                  正  大  光  明                    ║
        ║                                                   ║
        ╚═══════════════════════════════════════════════════╝

*************************************************************************
VN is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any
later version.

VN is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along
with VN. If not, see <https://www.gnu.org/licenses/>.
*************************************************************************
** VN: eVe's general iNfrastructure
*************************************************************************
*/
#include <errno.h> // EAGAIN EWOULDBLOCK
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h> // nanosleep

#ifdef MOCK_READ
typedef ssize_t (*ReadFunction)(int fd, void *buf, size_t count);
extern ReadFunction v_read;
#define read v_read
#endif

struct vn_gets_buf
{
  short ln_so; // init: /
  short ln_eo; // init: 0, 0 for incomplete
  short read_eo; // init: 0
  char dat[32762];
};

// TODO nework line ending not handled correctly
void vn_gets(int fd, struct vn_gets_buf *buf)
{
  if (buf->ln_eo > 16384)
    {
      memcpy(buf->dat, &buf->dat[buf->ln_eo], buf->read_eo - buf->ln_eo);
      buf->read_eo -= buf->ln_eo;
      buf->ln_eo = 0;
    }

  buf->ln_so = buf->ln_eo;

  if (buf->ln_eo < buf->read_eo)
    {
#define VN_GETS_SCAN_LINE() \
      for (int i = 0; buf->ln_so+i < buf->read_eo - 1; i++) \
	if (buf->dat[buf->ln_so+i] == '\r' && buf->dat[buf->ln_so+i+1] == '\n') \
	  { \
	    buf->ln_eo += i+2; \
	    return; \
	  } \
\
      buf->ln_eo = buf->read_eo; \
      return;

      VN_GETS_SCAN_LINE();
    }

  size_t bytes_read = 0;
  while (buf->read_eo < sizeof(buf->dat))
    {
      int ret = read(fd, buf->dat + buf->read_eo, sizeof(buf->dat) - buf->read_eo);
      if (ret > 0)
	{
	  buf->read_eo += ret;
	  VN_GETS_SCAN_LINE();
	}
      // TODO sleep and retry; for now it simply quits
      else if (ret == 0)
	{
	  // connection closed by client
	  break;
	}
      else
	{
	  if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
	      struct timespec sleep_time;
	      sleep_time.tv_sec = 0;
	      sleep_time.tv_nsec = 10000000; // 10 ms
	      nanosleep(&sleep_time, NULL);
            }
	  else
            {
	      // error occurred while reading
	      perror("read");
	      break;
           } 
	}
    }
}

char *vn_gets_to_string(struct vn_gets_buf *buf)
{
  char *str = malloc(buf->ln_eo - buf->ln_so + 1);
  if (str == NULL)
    {
      return NULL;
    }

  strncpy(str, buf->dat + buf->ln_so, buf->ln_eo - buf->ln_so);
  str[buf->ln_eo - buf->ln_so] = '\0';

  return str;
}

ssize_t vn_read(int fd, char *buf, ssize_t buffer_size)
{
  ssize_t bytes_read, n;
  bytes_read = 0;
  while ((n = read(fd, buf + bytes_read, buffer_size - bytes_read)) > 0)
    {
      if (n > 0)
	{
	  bytes_read += n;
	  if (bytes_read == buffer_size)
	    {
	      printf("File is larger than 1MB.\n");
	    }
	  // TODO sleep and retry; for now it simply quits
	}
      else
	{
	  perror("read");
	  break;
	}
    }

  return bytes_read;
}

void vn_puts(int fd, const void *buf, size_t len)
{
  ssize_t bytes_written = 0;
  while (bytes_written < len)
    {
      ssize_t n = write(fd, buf + bytes_written, len - bytes_written);
      if (n == -1)
        {
	  if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
	      struct timespec sleep_time;
	      sleep_time.tv_sec = 0;
	      sleep_time.tv_nsec = 10000000; // 10 ms
	      nanosleep(&sleep_time, NULL);
            }
	  else
            {
	      perror("write");
	      break;
            }
        }
      else
        {
	  bytes_written += n;
        }
    }
}

char *vn_string_map(char *kvs[], char *s)
{
  // TODO more accurate return string length
  char *r = malloc(2 * strlen(s) + 20);

  size_t len = strlen(s);
  size_t i, j, k;
  size_t z;
  while (kvs[z++]);

  i = 0;
  while (i < len)
    {
      for (k = 0; k < z; k += 2)
	{
	  if (strcmp(kvs[k], s+i) == 0)
	    {
	      strcpy(r+j, kvs[k+1]);
	      i += strlen(kvs[k]);
	      j += strlen(kvs[k+1]);
	      goto next_match;
	    }
	}
      i++, j++;
    next_match:
    }
}
