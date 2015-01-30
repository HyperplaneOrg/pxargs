/*$**************************************************************************
*
* FILE: 
*    ptmpf.c
*
* AUTHOR:
*    Andrew Michaelis, amac@hyperplane.org
* 
* FILE VERSION:
*    0.0.1
* 
* LICENSE:
*  This file is part of pxargs.
*
*  pxargs is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
* 
*  pxargs is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
* 
*  You should have received a copy of the GNU General Public License
*  along with pxargs. If not, see http://www.gnu.org/licenses/.
*
*  Copyright (C) 2012 Andrew Michaelis
* 
***************************************************************************$*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H 
 /* for close */
 #include <unistd.h> 
#endif

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <errno.h>

#include <ptmpf.h>

/*--------------------------------------------------------------------------------------------
*/
int fstream2tempfile(FILE* stream, char* fnamebuf, size_t fnamebuflen)
{
   int fd;
   char* tmpdirnm = NULL;
   size_t slen;
   FILE* fout;
   unsigned char rbuf[4096];
   size_t rdi = 0;

   memset(rbuf, 0, 4096);
   memset(fnamebuf, 0, fnamebuflen);
   if( (tmpdirnm = getenv("TMPDIR")) != NULL)
   {
      strncpy(fnamebuf, tmpdirnm, fnamebuflen-1);
      slen = strlen(fnamebuf);
      if( (slen + 16) > fnamebuflen)
      {
         fprintf(stderr, "\"%s\" @L %d : TMPDIR environ seems a bit long (%zu)\n", __FILE__, __LINE__, slen);
         return -1;
      }
      if(fnamebuf[slen-1] == '/')
         strcat(fnamebuf, "pxargs.XXXXXXXX");
      else
         strcat(fnamebuf, "/pxargs.XXXXXXXX");
   }
   else
      strcpy(fnamebuf, "pxargs.XXXXXXXX");

   if( (fd = mkstemp(fnamebuf)) < 0 )
   {
      fprintf(stderr, "\"%s\" @L %d : mkstemp : %s\n", __FILE__, __LINE__, strerror(errno));
      return -1;
   }
   if( (fout = fdopen(fd, "w")) == NULL)
   {
      fprintf(stderr, "\"%s\" @L %d : fdopen : %s\n", __FILE__, __LINE__, strerror(errno));
      close(fd);
      return -1;
   }
   /* copy */
   while( (rdi = fread(rbuf, sizeof(unsigned char), 4096, stream)) > 0)
      fwrite(rbuf, sizeof(unsigned char), rdi, fout);

   fclose(fout);
   return 0;
}/* fstream2tempfile */


