#include "common.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct memfile_cookie {
   char   *buf;        /* Dynamically sized buffer for data */
   size_t  allocated;  /* Size of buf */
   size_t  endpos;     /* Number of characters in buf */
   off_t   offset;     /* Current file offset in buf */
};

ssize_t
memfile_write(void *c, const char *buf, size_t size)
{
   char *new_buff;
   memfile_cookie *cookie = (memfile_cookie *)c;

   /* Buffer too small? Keep doubling size until big enough */

   while (size + cookie->offset > cookie->allocated) {
       new_buff = (char*)realloc(cookie->buf, cookie->allocated * 2);
       if (new_buff == NULL) {
           return -1;
       } else {
           cookie->allocated *= 2;
           cookie->buf = new_buff;
       }
   }

   memcpy(cookie->buf + cookie->offset, buf, size);

   cookie->offset += size;
   if (cookie->offset > cookie->endpos)
       cookie->endpos = cookie->offset;

   return size;
}

ssize_t
memfile_read(void *c, char *buf, size_t size)
{
   ssize_t xbytes;
   memfile_cookie *cookie = (memfile_cookie *)c;

   /* Fetch minimum of bytes requested and bytes available */

   xbytes = size;
   if (cookie->offset + size > cookie->endpos)
       xbytes = cookie->endpos - cookie->offset;
   if (xbytes < 0)     /* offset may be past endpos */
      xbytes = 0;

   memcpy(buf, cookie->buf + cookie->offset, xbytes);

   printrarread(buf,size,cookie->offset);
   sleep(1);
   cookie->offset += xbytes;
   return xbytes;
}

int
memfile_seek(void *c, off64_t *offset, int whence)
{
   off64_t new_offset;
   memfile_cookie *cookie = (memfile_cookie *)c;

   if (whence == SEEK_SET)
       new_offset = *offset;
   else if (whence == SEEK_END)
       new_offset = cookie->endpos + *offset;
   else if (whence == SEEK_CUR)
       new_offset = cookie->offset + *offset;
   else
       return -1;

   if (new_offset < 0)
       return -1;

   printrarseek(*offset,whence,cookie->offset);

   cookie->offset = new_offset;
   *offset = new_offset;
   return 0;
}

int
memfile_close(void *c)
{
   memfile_cookie *cookie = (memfile_cookie *)c;

   free(cookie->buf);
   cookie->allocated = 0;
   cookie->buf = NULL;

   return 0;
}


FILE *fopenmemory() {
	cookie_io_functions_t  memfile_func = {
       .read  = memfile_read,
       .write = memfile_write,
       .seek  = memfile_seek,
       .close = memfile_close
   };
   memfile_cookie * mycookie = new memfile_cookie();
   mycookie->buf = (char*)malloc(1024);
   mycookie->allocated = 1024;
   mycookie->endpos = 0;
   mycookie->offset = 0;
   return fopencookie(mycookie,"w+", memfile_func);
}