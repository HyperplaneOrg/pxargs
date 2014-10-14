/*$**************************************************************************
*
* FILE: 
*    pxargs.c
*
* AUTHOR:
*    Andrew Michaelis, amac@hyperplane.org
* 
* FILE VERSION:
*    0.0.3
* 
# LICENSE:
#  This file is part of pxargs.
#
#  pxargs is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
# 
#  pxargs is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with pxargs. If not, see http://www.gnu.org/licenses/.
#
#  Copyright (C) 2012 Andrew Michaelis
* 
***************************************************************************$*/
#ifdef HAVE_CONFIG_H
 #include <config.h>
#endif

#ifdef HAVE_UNISTD_H 
 #include <unistd.h> 
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_LIMITS_H 
 #include <limits.h>
#endif
#ifdef HAVE_SIGNAL_H 
 #include <signal.h>
#endif

#ifdef HAVE_SYS_WAIT_H 
 #include <sys/wait.h>
#endif

#include <mpi.h>
#include <pxargs.h>

#define MAX_PRINT_MINUTES 10080 /* 1 week in hours (this is only used for pretty printing...) */

#if defined(HAVE_SIGNAL_H) && defined(HAVE_SIGACTION) && defined(HAVE_ALARM)
/* subprocess (child) that will be used in xpopen */
static pid_t MODchldpid;
static volatile int MODTexceed;

/*--------------------------------------------------------------------------------------
* Local module routine that will kill unruly, w.r.t. maxutime, child processes 
*/
static void child_killpg(int sig) 
{
   MODTexceed = PX_YES;
   killpg(MODchldpid, SIGKILL); /* this will kill the whole group under the worker child */
}/* child_killpg */

/*--------------------------------------------------------------------------------------
* Local module routine (run command), more complicated version. This routine does 
* the same thing as below but allows for interrupts. signals, etc... must be available 
* for this to build, else the simple version is built.
*/
static int xpopen(char* cmd, unsigned int maxutime, int verbose, FILE* verbout)
{
   struct sigaction sigst;
   pid_t wtpid;
   int pstat; 
   /* clear these */
   MODchldpid = 0;
   MODTexceed = PX_NO;

   memset(&sigst, 0, sizeof(struct sigaction));
   sigemptyset(&sigst.sa_mask); 
   sigst.sa_handler = child_killpg;
   sigst.sa_flags = 0;
   if( sigaction(SIGALRM, &sigst, NULL) < 0)
   {
      fprintf(stderr, "\"%s\" @L %d : sigaction failed to set SIGALRM : %s\n", __FILE__, __LINE__, strerror(errno));
      return -1;
   }
   /* set max time if needed */
   if(maxutime > 0) alarm(maxutime);

   if( (MODchldpid = fork()) < 0)
   {
      fprintf(stderr, "\"%s\" @L %d : fork failed : %s\n", __FILE__, __LINE__, strerror(errno));
      /* cancel alarm if needed */
      if(maxutime > 0) alarm(0);
      return -1;
   }
   else if(MODchldpid == 0)
   {
      /* set a new group leader here... */
      if(setpgid(0, 0) < 0) 
      {
         fprintf(stderr, "\"%s\" @L %d : setpgid failed : %s\n", __FILE__, __LINE__, strerror(errno));
         exit(1);
      } 

      if( execl("/bin/sh", "sh", "-c", cmd, NULL) < 0)
      {
         fprintf(stderr, "\"%s\" @L %d : execl failed : %s\n", __FILE__, __LINE__, strerror(errno));
         _exit(127);
      }
   }
   else
   {
      while( (wtpid = waitpid(MODchldpid, &pstat, 0)) < 0)
      {
         if(errno != EINTR) 
            return -1;
      }
      if(maxutime > 0) alarm(0);/* clear if needed */
   }
   return 0;
}/* xpopen */
#else
/*--------------------------------------------------------------------
* Local module routine (run pipe). Simple version...
*/
static int xpopen(char* cmd, unsigned int maxutime, int verbose, FILE* verbout)
{
   /* maxutime isn't used here since platform/build doesn't support alarms, signals, etc... */
   FILE* pip = NULL;
   MODTexceed = PX_NO;
   if( (pip = popen(cmd, "w")) == NULL)
   {
      fprintf(stderr, "%s @L %d : popen failed : %s\n", __FILE__, __LINE__, strerror(errno));
      return -1;
   }
   if( pclose(pip) < 0)
      fprintf(stderr, "%s @L %d : pclose failed : %s\n", __FILE__, __LINE__, strerror(errno));
   return 0;
}/* xpopen */
#endif

/*--------------------------------------------------------------------
* Local module routine, ez-now-time
*/
static long now_tm_secs()
{
   struct timeval tvl;
   memset(&tvl, 0 , sizeof(struct timeval));
   gettimeofday(&tvl, NULL);
   return ((long)tvl.tv_sec);
}/* now_tm_secs */

/*--------------------------------------------------------------------
* Local module routine
*/
static void set_work_tm_secs(WORK_UNIT* worku) 
{ 
   worku->proc_secs = now_tm_secs();
}/* set_work_tm_secs */

/*--------------------------------------------------------------------
* See pxargs.h for details
*/
int mpi_sizeof_worku()
{
   int bufsize;
   int packsize = 0;

   /* args */
   MPI_Pack_size(PXARGLENMAX, MPI_CHAR, MPI_COMM_WORLD, &packsize);
   bufsize = packsize; 
   
   /* 1 for long for timing... */ 
   packsize = 0;
   MPI_Pack_size(1, MPI_LONG, MPI_COMM_WORLD, &packsize);
   bufsize += packsize; 

   packsize = 0;
   MPI_Pack_size(1, MPI_INT, MPI_COMM_WORLD, &packsize);
   /* X 3 for all ints */ 
   bufsize += packsize * 3; 

   /* one file path */
   packsize = 0;
   MPI_Pack_size(PATH_MAX, MPI_CHAR, MPI_COMM_WORLD, &packsize);
   bufsize += packsize; 

   return bufsize;
}/* mpi_sizeof_worku */

/*--------------------------------------------------------------------
* See pxargs.h for details
*/
void mpi_worku_serialize(WORK_UNIT* worku, void* buf, int bufsize)
{
   int boffset = 0;
   memset(buf, 0, bufsize);
   MPI_Pack(worku->pargs, PXARGLENMAX, MPI_CHAR, buf, bufsize, &boffset, MPI_COMM_WORLD); 
   MPI_Pack(&(worku->proc_secs), 1, MPI_LONG, buf, bufsize, &boffset, MPI_COMM_WORLD); 
   MPI_Pack(&(worku->resrank), 1, MPI_INT, buf, bufsize, &boffset, MPI_COMM_WORLD); 
   MPI_Pack(&(worku->id_tag), 1, MPI_INT, buf, bufsize, &boffset, MPI_COMM_WORLD); 
   MPI_Pack(&(worku->was_killed), 1, MPI_INT, buf, bufsize, &boffset, MPI_COMM_WORLD); 
   MPI_Pack(worku->procpath, PATH_MAX, MPI_CHAR, buf, bufsize, &boffset, MPI_COMM_WORLD); 
   return;
}/* mpi_worku_serialize */

/*--------------------------------------------------------------------
* See pxargs.h for details
*/
void mpi_worku_unserialize(void* buf, int bufsize, WORK_UNIT* work)
{
   int boffset = 0;
   memset(work, 0, sizeof(WORK_UNIT));
   MPI_Unpack(buf, bufsize, &boffset, work->pargs, PXARGLENMAX, MPI_CHAR, MPI_COMM_WORLD);
   MPI_Unpack(buf, bufsize, &boffset, &(work->proc_secs), 1, MPI_LONG, MPI_COMM_WORLD);
   MPI_Unpack(buf, bufsize, &boffset, &(work->resrank), 1, MPI_INT, MPI_COMM_WORLD);
   MPI_Unpack(buf, bufsize, &boffset, &(work->id_tag), 1, MPI_INT, MPI_COMM_WORLD);
   MPI_Unpack(buf, bufsize, &boffset, &(work->was_killed), 1, MPI_INT, MPI_COMM_WORLD);
   MPI_Unpack(buf, bufsize, &boffset, work->procpath, PATH_MAX, MPI_CHAR, MPI_COMM_WORLD);
   return;
}/* mpi_worku_unserialize */

/*--------------------------------------------------------------------
* See pxargs.h for details
*/
WORK_UNIT* load_work_list(const char* fname, int* n)
{
   WORK_UNIT* worku = NULL;
   FILE* fin;
   char lnbuf[PXARGLENMAX+8];
   char *sep = "\r\n";
   char* next;
   char* tok;
   int i;

   memset(lnbuf, 0, PXARGLENMAX+8);
   *n = 0;
   if( (fin = fopen(fname, "r")) == NULL)
   {
      fprintf(stderr, "%s @L %d : \"%s\" : %s\n", __FILE__, __LINE__, fname, strerror(errno));
      return NULL;
   }
   /* just count here */
   while((fgets(lnbuf, PXARGLENMAX, fin) != NULL)) 
   {   
      /* skip comments, etc... */ 
      if( (lnbuf[0] == '\r') || (lnbuf[0] == '\n') || (lnbuf[0] == '#') ) 
         continue; 
      (*n) += 1;
   }
   rewind(fin);

   if( (worku = (WORK_UNIT*)calloc(*n, sizeof(WORK_UNIT))) == NULL)
   {
      fprintf(stderr, "%s @L %d : calloc error for work unit list : %s\n", __FILE__, __LINE__, strerror(errno));
      fclose(fin);
   }
      
   /* load here */
   i = 0;
   while((fgets(lnbuf, PXARGLENMAX, fin) != NULL) && (i < (*n)) )
   {   
      /* skip comments, etc... */ 
      if( (lnbuf[0] == '\r') || (lnbuf[0] == '\n') || (lnbuf[0] == '#') ) 
         continue; 

      if( (tok=STRTOK(lnbuf, sep, &next)) != NULL) 
         strcpy(worku[i].pargs, tok);
            
      /* basic inits, see pxargs.h */
      worku[i].proc_secs = 0;
      worku[i].resrank = RANK_UNASSIGNED;
      worku[i].id_tag = i;
      worku[i].was_killed = PX_NO;
      i++;
   }
   fclose(fin);
   return worku;
}/* load_work_list */

/*-----------------------------------------------------------------------------------------------------
* See pxargs.h for details
*/
void fprint_worku(FILE* fout, WORK_UNIT* worku)
{
   if(worku == NULL)
   {
      fprintf(fout, "WORKU NULL\n");
      return;
   }
   fprintf(fout, "PROC PATH = \"%s\", ", ((worku->procpath[0] != '\0') ? worku->procpath : "UNASSIGNED"));
   fprintf(fout, "PARGS  = \"%s\", ", worku->pargs);

   if(worku->resrank == RANK_UNASSIGNED)
      fprintf(fout, "RESPONSIBLE RANK = RANK_UNASSIGNED, ");
   else
      fprintf(fout, "RESPONSIBLE RANK = %d, ", worku->resrank);

   fprintf(fout, "ID TAG = %d, ", worku->id_tag);
   if( worku->proc_secs < (60 * MAX_PRINT_MINUTES) )
      fprintf(fout, "PROCESS TIME = %.3f minutes, ", ((double)(worku->proc_secs)) / 60.0);
   else
      fprintf(fout, "PROCESS TIME = Undef minutes, ");
   fprintf(fout, "TIME EXCEEDED = %s\n", ((worku->was_killed == PX_YES) ? "Yes" : "No"));
}/* fprint_worku */

/*-----------------------------------------------------------------------------------------------------
* See pxargs.h for details
*/
void fprint_worklist(FILE* fout, WORK_UNIT* wlist, int n)
{
   int i;
   if(wlist == NULL)
      return;
   for(i = 0; i < n; i++)
      fprint_worku(fout, &(wlist[i]));
}/* fprint_worklist */

/*-----------------------------------------------------------------------------------------------------
* See pxargs.h for details
*/
int coordinate_proc( WORK_UNIT* wunits, int n, const char* proc, 
                     int groupsz, int randstart, int randend,
                     int verbose, FILE* verbout
                   )
{
   unsigned char* mastmsg;
   int i, nxt, bfsize;
   int recvd = 0;
   MPI_Status status;
   char* SRC_FILE = __FILE__;
   const int mrank = 0; /* This will be master for now */
   WORK_UNIT oneu; 
   int waitfor = 0;
   int nworkers;

   nworkers = groupsz-1;
   bfsize = mpi_sizeof_worku();
   if( (mastmsg = (unsigned char*) malloc(bfsize)) == NULL)
   {
      fprintf(stderr, "\"%s\" @L %d RANK %d : malloc failed : %s\n", SRC_FILE, __LINE__, mrank, strerror(errno));
      return -1;
   }
   srand( (unsigned int) now_tm_secs() );
   /* Do initial divvy */
   for(i = 1; (i <= nworkers) && (i <= n) ; i++)
   {
      wunits[i-1].resrank = i;
      strcpy(wunits[i-1].procpath, proc);
      set_work_tm_secs(&(wunits[i-1]));
      mpi_worku_serialize(&(wunits[i-1]), mastmsg, bfsize);
         
      if(verbose >= 2)
         fprintf(verbout, "Sending \"%s\" to rank[%d]\n", wunits[i-1].procpath, wunits[i-1].resrank);
                     
      if( (randstart >= 0) && (randend >= 0) )
      {
         waitfor = rand() % (randend - randstart + 1) + randstart;
         if(verbose >= 3)
            fprintf(verbout, "Wait for %d secs on sending \"%s\" to rank[%d]\n", waitfor, wunits[i-1].procpath, wunits[i-1].resrank);
         sleep(waitfor);
      }
   
      if( MPI_Send(mastmsg, bfsize, MPI_PACKED, i, DOWORK, MPI_COMM_WORLD) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Send Failed! :(\n", SRC_FILE, __LINE__, mrank);
         return -1;
      }
   }
   /* go until we're done */
   for(nxt = i, recvd = 0; nxt <= n; nxt++)
   {
      /* anybody finished? */
      if( MPI_Recv(mastmsg, bfsize, MPI_PACKED, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Recv Failed! :(\n", SRC_FILE, __LINE__, mrank);
         return -1;
      }
      else
         recvd += 1; 

      mpi_worku_unserialize(mastmsg, bfsize, &oneu);
      wunits[oneu.id_tag].proc_secs = now_tm_secs() - oneu.proc_secs; 
      wunits[oneu.id_tag].was_killed = oneu.was_killed;
      /* future TODO: reset anything else that matters here */
   
      if(verbose >= 2)
         fprintf(verbout, "received completed work from rank[%d]\n", status.MPI_SOURCE);
   
      /* send out next piece */
      wunits[nxt-1].resrank = status.MPI_SOURCE;
      strcpy(wunits[nxt-1].procpath, proc);
      set_work_tm_secs(&(wunits[nxt-1]));
      mpi_worku_serialize(&(wunits[nxt-1]), mastmsg, bfsize);

      if(verbose >= 2)
         fprintf(verbout, "Sending \"%s\" to rank[%d]\n", wunits[nxt-1].procpath, status.MPI_SOURCE);
         
      if( MPI_Send(mastmsg, bfsize, MPI_PACKED, status.MPI_SOURCE, DOWORK, MPI_COMM_WORLD) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Send Failed! :(\n", SRC_FILE, __LINE__, mrank);
         return -1;
      }
   }
   /* get stragglers */
   for(i = 0; i < (n-recvd); i++)
   {
      if( MPI_Recv(mastmsg, bfsize, MPI_PACKED, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Recv Failed! (stragglers)\n", SRC_FILE, __LINE__, mrank);
         return -1;
      }
      if(verbose >= 2)
         fprintf(verbout, "received completed work from straggler rank[%d]\n", status.MPI_SOURCE);
         
      mpi_worku_unserialize(mastmsg, bfsize, &oneu);
      wunits[oneu.id_tag].proc_secs = now_tm_secs() - oneu.proc_secs; 
      wunits[oneu.id_tag].was_killed = oneu.was_killed;
      /* future TODO: set anything else that matters here */
   }
   /* tell everyone we're done */
   for(i = 1; i <= nworkers; ++i)
   {
      if( MPI_Send(0, 0, MPI_INT, i, ENDWORK, MPI_COMM_WORLD) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Send Failed! :(\n", SRC_FILE, __LINE__, mrank);
         return -1;
      }
   }
   free(mastmsg);
   return 0;
}/* coordinate_proc */


/*--------------------------------------------------------------------
* See pxargs.h for details
*/
int work_proc(int rank, unsigned int maxutime, int verbose, FILE* verbout)
{
   MPI_Status status;
   unsigned char* workmsg;
   char* cmdbuf;
   const int maxwork = PXMAXARGS; 
   int i = 0;
   int bfsize, cmdsize;
   WORK_UNIT oneu; 
   
   cmdsize = PATH_MAX+PXARGLENMAX+64;
   /* setup on maximum size */
   bfsize = mpi_sizeof_worku();
   if( (workmsg = (unsigned char*) malloc(bfsize)) == NULL)
   {
      fprintf(stderr, "\"%s\" @L %d RANK %d : malloc failed : %s\n", __FILE__, __LINE__, rank, strerror(errno));
      return -1;
   }
   if( (cmdbuf = (char*) malloc(cmdsize)) == NULL)
   {
      fprintf(stderr, "\"%s\" @L %d RANK %d : malloc failed : %s\n", __FILE__, __LINE__, rank, strerror(errno));
      free(workmsg);
      return -1;
   }

   while(i <= maxwork) 
   {
      memset(workmsg, 0, bfsize);
      memset(cmdbuf, 0, cmdsize);

      /* Receive a message from the master */
      MPI_Recv(workmsg, bfsize, MPI_PACKED, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

      /* Check the tag of the received message. */
      if (status.MPI_TAG == ENDWORK) 
      {
         if(verbose >= 2)
            fprintf(verbout, "Rank %d receive exit signal\n", rank);
         free(cmdbuf);
         free(workmsg);
         return 0;
      }
      mpi_worku_unserialize(workmsg, bfsize, &oneu);
      
      snprintf(cmdbuf, cmdsize, "%s %s", oneu.procpath, oneu.pargs); 
      if(verbose == 2)
         fprintf(verbout, "Rank %d receive \"%s\" \"%s\"\n", rank, oneu.procpath, oneu.pargs);
      else if(verbose >= 3)
      {
         fprintf(verbout, "Rank %d receive WORK UNIT: ", rank);
         fprint_worku(verbout, &oneu);
         if(maxutime > 0)
            fprintf(verbout, "Rank %d running \"%s\" (max runtime %u)\n", rank, cmdbuf, maxutime);
         else
            fprintf(verbout, "Rank %d running \"%s\"\n", rank, cmdbuf);
      }

      /*-------------------------------*/
      /* send to program/script        */
      /*-------------------------------*/
      if( xpopen(cmdbuf, maxutime, verbose, verbout) < 0)
          fprintf(stderr, "%s @L %d RANK %d : Command \"%s\" failed with xpopen\n", __FILE__, __LINE__, rank, cmdbuf);

      oneu.was_killed = MODTexceed;
      /* future TODO: set anything else that matters here that may need to be sent back */

      /* now we're finished */
      mpi_worku_serialize(&oneu, workmsg, bfsize);
      MPI_Send(workmsg, bfsize, MPI_PACKED, 0, 0, MPI_COMM_WORLD);
      i++;
   }

   free(workmsg);
   free(cmdbuf);
   return 0;
}/* work_proc */


