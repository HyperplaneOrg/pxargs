/*$**************************************************************************
*
* FILE: 
*    pbsu.c
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
*  Copyright (C) 2014, 2015 Andrew Michaelis
* 
***************************************************************************$*/
#ifdef HAVE_CONFIG_H
 #include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>

#ifdef HAVE_LIMITS_H 
 #include <limits.h>
#endif

#ifdef HAVE_PBS_IFL_H 
 #include <pbs_ifl.h> 
#endif

#ifdef HAVE_PBS_ERROR_H 
 #include <pbs_error.h>
#endif
 
#include <pbsu.h> 

#ifdef HAVE_STRTOK_R 
 #define STRTOK(s, d, sp) (strtok_r(s, d, sp))
#else
 #define STRTOK(s, d, sp) (strtok(s, d))
#endif

#ifndef UINT_MAX	
 #define UINT_MAX	4294967295U  
#endif
 
#if defined(HAVE_LIBPBS) && defined(HAVE_LIBPTHREAD) 

/* module error note */
static const char* SRC_FILE = __FILE__;

/*------------------------------------------------------------------------------
* Local module routine will parse a time string of the format HH:MM:SS
*/
static unsigned long pbsu_time_str_to_sec(char* tstr)
{
   long hours = 0, minutes = 0, seconds = 0;
   char *sep = ":";
   char *next, *tok;

   if(tstr == NULL)
      return 0;

   if( (tok = STRTOK(tstr, sep, &next)) != NULL) 
      hours = atol(tok);
   if( (tok = STRTOK(NULL, sep, &next)) != NULL) 
      minutes = atol(tok);
   if( (tok = STRTOK(NULL, sep, &next)) != NULL) 
      seconds = atol(tok);
   return (unsigned long) ((hours * 60 * 60) + (minutes * 60) + seconds);
}/* pbsu_time_str_to_sec */

/*------------------------------------------------------------------------------
*/
int pbsu_this_job_info(char* pbsHostBuff, size_t phlen, char* jobIdBuff, size_t jlen)
{
   char *ev, tbuf[512];
   size_t l;
   int status;
   regex_t re;
   regmatch_t pmatch[4];

   memset(tbuf, 0, 512);
   memset(&pmatch, 0, sizeof(regmatch_t)*4);
   memset(pbsHostBuff, 0, phlen);
   memset(jobIdBuff, 0, jlen);

   if( (ev = getenv("PBS_JOBID")) == NULL)
   {
      fprintf(stderr, "%s @L %d : \"PBS_JOBID\" not found, are we running via pbs?\n", SRC_FILE, __LINE__);
      return -1;
   }
   strncpy(tbuf, ev, 511);
   if( (status = regcomp(&re, PBS_JOBID_REGX, REG_EXTENDED)) != 0) 
   {
      regerror(status, &re, tbuf, 511);
      fprintf(stderr, "%s @L %d : regcomp for PBS_JOBID failed : %s\n", SRC_FILE, __LINE__, tbuf);
      return -1;
   }
   if( (status = regexec(&re, tbuf, 4, pmatch, 0)) != 0) 
   {
      regerror(status, &re, tbuf, 511);
      fprintf(stderr, "%s @L %d : regexec for PBS_JOBID failed : %s\n", SRC_FILE, __LINE__, tbuf);
      return -1;
   }
   l = pmatch[0].rm_eo - pmatch[0].rm_so;
   if(l >= (jlen-1)) l = jlen-1;
   strncpy(jobIdBuff, &tbuf[pmatch[0].rm_so], l); 
   
   l = pmatch[0].rm_eo - pmatch[1].rm_so;
   if(l >= (phlen-1)) l = phlen-1;
   strncpy(pbsHostBuff, &tbuf[pmatch[2].rm_so], l); 

   regfree(&re);
   return 0;
}/* pbsu_this_job_info */


/*------------------------------------------------------------------------------
*/
int pbsu_server_connect(char* pbsServer)
{
   int s;
   if( (s = pbs_connect(pbsServer)) < 0)
      fprintf(stderr, "%s @L %d : pbs_connect failed [%d] : %s\n", 
                                       SRC_FILE, __LINE__, s, pbsServer);
   return s;
}/* pbsu_server_connect */


/*------------------------------------------------------------------------------
*/
void pbsu_server_close(int conn)
{
   int er;
   char* ers;
   if( (er = pbs_disconnect(conn)) != 0)
      fprintf(stderr, "%s @L %d : pbs_disconnect failed [%d] : %s\n", SRC_FILE, 
               __LINE__, er, (((ers = pbs_geterrmsg(conn)) != NULL ? ers : "NA")));
}/* pbsu_server_close */


/*------------------------------------------------------------------------------
*/
unsigned int pbsu_job_time_left(int conn, char* jobID)
{
   char *ers;
   unsigned long rt = 0, wt = 0; 
   char *wallTmStr = "walltime";
   char tstr[256];
   struct batch_status* bs = NULL;
   struct attrl *aptr; 

   if( (bs = pbs_statjob(conn, jobID, NULL, NULL)) == NULL)
   {
      fprintf(stderr, "%s @L %d : pbs_statjob failed : %s\n", SRC_FILE, __LINE__, 
                              (((ers = pbs_geterrmsg(conn)) != NULL ? ers : "NA")));
      /* The job info probe can fail for various reasons, return a large number so we don't disrupt this job */
      return UINT_MAX - 1; 
   }

	for(aptr = bs->attribs; aptr != NULL; aptr = aptr->next)
   {
      if( (aptr->name != NULL) && (aptr->resource != NULL) ) 
      {
         if( (!strcmp(ATTR_used, aptr->name)) && (!strcmp(wallTmStr, aptr->resource)) )
         {
            memset(tstr, 0, 256);
            rt = pbsu_time_str_to_sec( strncpy(tstr, aptr->value, 255) );
         }
         else if( (!strcmp(ATTR_l, aptr->name)) && (!strcmp(wallTmStr, aptr->resource)) )
         {
            memset(tstr, 0, 256);
            wt = pbsu_time_str_to_sec( strncpy(tstr, aptr->value, 255) );
         }
      }
   }
   if(bs != NULL)
      pbs_statfree(bs);

   return (unsigned int) (wt - rt);
}/* pbsu_job_time_left */

#endif

