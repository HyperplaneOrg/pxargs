/*$**************************************************************************
*
* FILE: 
*    pxargs.c
*
* AUTHOR:
*    Andrew Michaelis, amac@hyperplane.org
* 
* FILE VERSION:
*    0.0.4
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
*  Copyright (C) 2012, 2014, 2015 Andrew Michaelis
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
#include <time.h>
#include <math.h>

#ifdef HAVE_LIMITS_H 
 #include <limits.h>
#endif
#ifdef HAVE_SIGNAL_H 
 #include <signal.h>
#endif

#ifdef HAVE_SYS_WAIT_H 
 #include <sys/wait.h>
#endif

#ifdef HAVE_PTHREAD_H 
 #include <pthread.h>
#endif

#include <mpi.h>
#include <pxargs.h>
#include <pbsu.h>

#define MAX_PRINT_MINUTES 10080 /* 1 week in hours (this is only used for pretty printing...) */

static const char* SRC_FILE = __FILE__;

/* subprocess (child) that will be used in xpopen */
static pid_t MODchldpid;
static volatile int MODTexceed;

/* These are for the pthreaded monitor */
static int stopissed = 0; /* init to 0 is important here */
pthread_mutex_t mtexstop;
struct ptparams
{
   unsigned int preexit;
   unsigned int verbose;
   FILE* verbout;
};

#if defined(HAVE_SIGNAL_H) && defined(HAVE_SIGACTION) && defined(HAVE_ALARM) 
/*--------------------------------------------------------------------------------------
* Local module routine that will kill unruly, w.r.t. maxutime, child processes 
*/
static void child_killpg(int sig) 
{
   MODTexceed = PX_YES;
   kill(-MODchldpid, SIGKILL); /* this should kill the whole group under the worker */
}/* child_killpg */

/*------------------------------------------------------------------------------
* DESCRIPTION:
*   A local module routine for running a subprocess. This routine allows for 
*   a timeout interrupt if the subprocess gets stuck and takes too long.
*/
static int xpopen(char* cmd, unsigned int maxutime, unsigned int verbose, FILE* verbout)
{
   struct sigaction sigst, oldsig;
   pid_t wtpid;
   int pstat; 

   /* clear these */
   MODchldpid = 0;
   MODTexceed = PX_NO;

   memset(&sigst, 0, sizeof(struct sigaction));
   memset(&oldsig, 0, sizeof(struct sigaction));

   sigemptyset(&sigst.sa_mask); 
   sigst.sa_handler = child_killpg;
   sigst.sa_flags = 0;

   if( sigaction(SIGALRM, &sigst, &oldsig) < 0)
   {
      fprintf(stderr, "\"%s\" @L %d : sigaction failed to set SIGALRM : %s\n", SRC_FILE, __LINE__, strerror(errno));
      return -1;
   }
   /* set max time if needed */
   if(maxutime > 0) 
      alarm(maxutime);

   if( (MODchldpid = fork()) < 0)
   {
      fprintf(stderr, "\"%s\" @L %d : fork failed : %s\n", SRC_FILE, __LINE__, strerror(errno));
      if(maxutime > 0) 
         alarm(0);
      if( sigaction(SIGALRM, &oldsig, NULL) < 0)
         fprintf(stderr, "\"%s\" @L %d : sigaction failed to restore SIGALRM : %s\n", SRC_FILE, __LINE__, strerror(errno));
      return -1;
   }
   
   if(MODchldpid == 0) /* child */
   {
      /* set a new group leader here... */
      if(setpgid(0, 0) < 0) 
      {
         fprintf(stderr, "\"%s\" @L %d : setpgid failed : %s\n", SRC_FILE, __LINE__, strerror(errno));
         exit(1); /* exit child */
      } 

      if( execl(SHL_PATH, SHL_STR, "-c", cmd, NULL) < 0)
      {
         fprintf(stderr, "\"%s\" @L %d : execl failed : %s\n", SRC_FILE, __LINE__, strerror(errno));
         _exit(127);
      }
   }
   else /* parent */
   {
      while( (wtpid = waitpid(MODchldpid, &pstat, 0)) < 0)
      {
         if(errno != EINTR) 
         {
            if(maxutime > 0) 
               alarm(0);
            if( sigaction(SIGALRM, &oldsig, NULL) < 0)
               fprintf(stderr, "\"%s\" @L %d : sigaction failed to restore SIGALRM : %s\n", SRC_FILE, __LINE__, strerror(errno));
            return -1;
         }
      }
      /* clear and restore */
      if(maxutime > 0) 
         alarm(0);
      if( sigaction(SIGALRM, &oldsig, NULL) < 0)
      {
         fprintf(stderr, "\"%s\" @L %d : sigaction failed to restore SIGALRM : %s\n", SRC_FILE, __LINE__, strerror(errno));
         return -1;
      }
   }
   return 0;
}/* xpopen */
#else
/*--------------------------------------------------------------------
* Local module routine (run pipe). Simple version, see above...
*/
static int xpopen(char* cmd, unsigned int maxutime, unsigned int verbose, FILE* verbout)
{
   /* maxutime isn't used here since platform/build doesn't support alarms, signals, etc... */
   FILE* pip = NULL;
   MODTexceed = PX_NO;
   if( (pip = popen(cmd, "w")) == NULL)
   {
      fprintf(stderr, "%s @L %d : popen failed : %s\n", SRC_FILE, __LINE__, strerror(errno));
      return -1;
   }
   if( pclose(pip) < 0)
      fprintf(stderr, "%s @L %d : pclose failed : %s\n", SRC_FILE, __LINE__, strerror(errno));
   return 0;
}/* xpopen */
#endif

/*------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   Module routine for polling the pbs server
*
* INPUTS:
*   targs => The struct ptparams passed in via the pthread_create routine
*
* NOTES:
*   Rather than pinging the server once, get the walltime then and check
*   the pre-exit time this routine makes requests to the server for the 
*   job time using a "decaying" wait interval in a rough attempt to 
*   catch a qalter reduction in walltime after job submission.
*/
void* pbs_poll(void* targs)
{
#ifdef HAVE_LIBPBS
   struct timespec ts;
   char host[256], job[256];
   unsigned int secs, max, itr;
   int con;
   struct ptparams* fpars;

   memset(&ts, 0, sizeof(struct timespec));
   fpars = (struct ptparams*) targs;
   max = 0;
   itr = 1;

   if( fpars->preexit < DEFAULT_PREEXIT_MIN_POLL_SECONDS)
      fpars->preexit = DEFAULT_PREEXIT_MIN_POLL_SECONDS;

   /* get pbs job info and connection to the server */
   pbsu_this_job_info(host, 256, job, 256);
   con = pbsu_server_connect(host);

   while( PX_YES  )
   {
      secs = pbsu_job_time_left(con, job); /* ping pbs and get time left */
      if(secs == 0)
         fprintf(stderr, "%s @L %d : PBS_POLL WARN : time left routine returned 0 seconds?\n", SRC_FILE, __LINE__);
      else if(max == 0)  /* set once */
         max = secs;

      if(fpars->verbose >= 3)
         fprintf(fpars->verbout, "MONITOR : PBS poll shows %u s time left (max time %u s).\n", secs, max);

      if(secs <= fpars->preexit) 
      {
         pthread_mutex_lock(&mtexstop);
         stopissed = 1;
         pthread_mutex_unlock(&mtexstop);
         break;
      }
      else if(stopissed == 2) /* locking this here not that critical */
         break;
 
      /* compute the 'backup' time for a sleep before next poll. */
      ts.tv_sec = (time_t) ( (double)max / ceil( pow(2, itr)) );
      if(ts.tv_sec < DEFAULT_PREEXIT_MIN_POLL_SECONDS) 
         ts.tv_sec = DEFAULT_PREEXIT_MIN_POLL_SECONDS; /* safety so the users doesn't poll too much */
 #ifdef HAVE_NANOSLEEP 
      nanosleep(&ts, NULL);
 #else
      sleep(ts.tv_sec);
 #endif
      itr += 1; 
   }
   pbsu_server_close(con);
#endif
   pthread_exit(NULL);
}/* pbs_poll */

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
   /* X 2 for all ints */ 
   bufsize += packsize * 2; 
   
   /* 1 unsigned int */
   packsize = 0;
   MPI_Pack_size(1, MPI_UNSIGNED, MPI_COMM_WORLD, &packsize);
   bufsize += packsize;

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
   MPI_Pack(&(worku->id_tag), 1, MPI_UNSIGNED, buf, bufsize, &boffset, MPI_COMM_WORLD); 
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
   MPI_Unpack(buf, bufsize, &boffset, &(work->id_tag), 1, MPI_UNSIGNED, MPI_COMM_WORLD);
   MPI_Unpack(buf, bufsize, &boffset, &(work->was_killed), 1, MPI_INT, MPI_COMM_WORLD);
   MPI_Unpack(buf, bufsize, &boffset, work->procpath, PATH_MAX, MPI_CHAR, MPI_COMM_WORLD);
   return;
}/* mpi_worku_unserialize */

/*--------------------------------------------------------------------
* See pxargs.h for details
*/
WORK_UNIT* load_work_list(const char* fname, unsigned int* n)
{
   WORK_UNIT* worku = NULL;
   FILE* fin;
   char lnbuf[PXARGLENMAX+8];
   char *sep = "\r\n";
   char* next;
   char* tok;
   unsigned int i;

   memset(lnbuf, 0, PXARGLENMAX+8);
   *n = 0;
   if( (fin = fopen(fname, "r")) == NULL)
   {
      fprintf(stderr, "%s @L %d : \"%s\" : %s\n", SRC_FILE, __LINE__, fname, strerror(errno));
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
      fprintf(stderr, "%s @L %d : calloc error for work unit list : %s\n", SRC_FILE, __LINE__, strerror(errno));
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
int dump_work_list_by_index( const char* fname, WORK_UNIT* wlist, unsigned int nlist, 
                             unsigned char* windex, unsigned int nindex )
{
   FILE* fout;
   unsigned int i;

   if( (fout = fopen(fname, "w")) == NULL)
   {
      fprintf(stderr, "%s @L %d : \"%s\" : %s\n", SRC_FILE, __LINE__, fname, strerror(errno));
      return -1;
   }
   for(i = 0; (i < nindex) && (i < nlist); i++)
   {
      if(windex[i] == 0)
      {
         if(wlist[i].pargs[0] != '\0')
            fprintf(fout, "%s\n", wlist[i].pargs);
      }
   }
   fclose(fout);
   return 0;
}/* dump_work_list_by_index */

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
void fprint_worklist(FILE* fout, WORK_UNIT* wlist, unsigned int n)
{
   unsigned int i;
   if(wlist == NULL)
      return;
   for(i = 0; i < n; i++)
      fprint_worku(fout, &(wlist[i]));
}/* fprint_worklist */

/*-----------------------------------------------------------------------------------------------------
* See pxargs.h for details
*/
int coordinate_proc( WORK_UNIT* wunits, unsigned int n, const char* proc, 
                     int nworkers, int rankstart, int randstart, 
                     int randend, MPI_Comm moncomm, int mnrank, 
                     unsigned int verbose, FILE* verbout
                   )
{
   MPI_Status status, mstatus;
   MPI_Request req; 
   WORK_UNIT oneu; 
   unsigned int i, j, recvd;
   int waitfor = 0, bfsize;
   const int mrank = 0; /* This is fixed to 0 for now */
   unsigned char* mastmsg;
#ifdef HAVE_NANOSLEEP 
   struct timespec nanoreq;

   memset(&nanoreq, 0, sizeof(struct timespec));
#endif

   if(moncomm != MPI_COMM_NULL) 
   {
      if(verbose >= 2) 
         fprintf(verbout, "COORDNTR WRLD Rank %d, NOTIFYING MONITOR[Rank %d] of the number of units (%u)\n", mrank, mnrank, n);
      
      if( MPI_Isend(&n, 1, MPI_UNSIGNED, mnrank, UNIT_TAG, moncomm, &req) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_ISend Failed to monitor via sub comm.\n", SRC_FILE, __LINE__, mrank);
         return -1;
      }
   }

   bfsize = mpi_sizeof_worku();
   if( (mastmsg = (unsigned char*) malloc(bfsize)) == NULL)
   {
      fprintf(stderr, "\"%s\" @L %d RANK %d : malloc failed : %s\n", SRC_FILE, __LINE__, mrank, strerror(errno));
      return -1;
   }
   srand( (unsigned int) now_tm_secs() );

   /* Do initial divvy */
   for(i = 0, j = rankstart; (i < nworkers) && (i < n); i++, j++)
   {
      wunits[i].resrank = j;
      strcpy(wunits[i].procpath, proc);
      set_work_tm_secs(&(wunits[i]));
      mpi_worku_serialize(&(wunits[i]), mastmsg, bfsize);
         
      if(verbose >= 2)
         fprintf(verbout, "Sending \"%s\" to rank[%d]\n", wunits[i].procpath, wunits[i].resrank);
                     
      if( (randstart >= 0) && (randend >= 0) )
      {
         waitfor = rand() % (randend - randstart + 1) + randstart;
         if(verbose >= 3)
            fprintf(verbout, "Wait for %d secs on sending \"%s\" to rank[%d]\n", waitfor, wunits[i].procpath, wunits[i].resrank);
#ifdef HAVE_NANOSLEEP 
         nanoreq.tv_sec = (time_t) waitfor; 
         nanosleep(&nanoreq, NULL);
#else
         sleep(waitfor);
#endif
      }
   
      if( MPI_Send(mastmsg, bfsize, MPI_PACKED, j, DOWORK, MPI_COMM_WORLD) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Send Failed! :(\n", SRC_FILE, __LINE__, mrank);
         return -1;
      }
   }

   /* Go until we're done (i starts from above i end) */
   for(recvd = 0; i < n; i++)
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

      /* notify monitor of unit completion */
      if(moncomm != MPI_COMM_NULL) 
      {
         if( MPI_Wait(&req, &mstatus) != MPI_SUCCESS)
            fprintf(stderr, "\"%s\" @L %d RANK %d : WARN MPI_Wait Failed! :(\n", SRC_FILE, __LINE__, mrank);
         if( MPI_Isend(&oneu.id_tag, 1, MPI_UNSIGNED, mnrank, COMPLETED_WORK, moncomm, &req) != MPI_SUCCESS)
         {
            fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_ISend Failed to monitor via sub comm.\n", SRC_FILE, __LINE__, mrank);
            return -1;
         }
      }
   
      if(verbose >= 2)
         fprintf(verbout, "received completed work from rank[%d]\n", status.MPI_SOURCE);
   
      /* send out next piece */
      wunits[i].resrank = status.MPI_SOURCE;
      strcpy(wunits[i].procpath, proc);
      set_work_tm_secs(&(wunits[i]));
      mpi_worku_serialize(&(wunits[i]), mastmsg, bfsize);

      if(verbose >= 2)
         fprintf(verbout, "Sending \"%s\" to rank[%d]\n", wunits[i].procpath, status.MPI_SOURCE);
         
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
         
      mpi_worku_unserialize(mastmsg, bfsize, &oneu);
      wunits[oneu.id_tag].proc_secs = now_tm_secs() - oneu.proc_secs; 
      wunits[oneu.id_tag].was_killed = oneu.was_killed;
      
      if(verbose >= 2)
         fprintf(verbout, "received completed work from straggler rank[%d] (id=%u)\n", status.MPI_SOURCE, oneu.id_tag);

      /* future TODO: set anything else that matters here */

      if(moncomm != MPI_COMM_NULL) 
      {
         if( MPI_Wait(&req, &mstatus) != MPI_SUCCESS)
            fprintf(stderr, "\"%s\" @L %d RANK %d : WARN MPI_Wait Failed! :(\n", SRC_FILE, __LINE__, mrank);
         if( MPI_Isend(&oneu.id_tag, 1, MPI_UNSIGNED, mnrank, COMPLETED_WORK, moncomm, &req) != MPI_SUCCESS)
         {
            fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Send Failed to monitor via sub comm.\n", SRC_FILE, __LINE__, mrank);
            return -1;
         }
      }
   }
   
   /* tell workers we're done */
   for(j = rankstart; j < (rankstart+nworkers); j++)
   {
      if( MPI_Send(0, 0, MPI_INT, j, ENDWORK, MPI_COMM_WORLD) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Send Failed! :(\n", SRC_FILE, __LINE__, mrank);
         return -1;
      }
   }
   
   /* tell the monitor we're done */
   if(moncomm != MPI_COMM_NULL) 
   {
      fprintf(verbout, "Sending exit to monitor...\n");
      if( MPI_Wait(&req, &mstatus) != MPI_SUCCESS)
         fprintf(stderr, "\"%s\" @L %d RANK %d : WARN MPI_Wait Failed! :(\n", SRC_FILE, __LINE__, mrank);
      if( MPI_Isend(0, 0, MPI_UNSIGNED, mnrank, ENDWORK, moncomm, &req) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Send Failed to monitor via sub comm.\n", SRC_FILE, __LINE__, mrank);
         return -1;
      }
   }

   free(mastmsg);
   return 0;
}/* coordinate_proc */

/*-------------------------------------------------------------------------
* See pxargs.h for details
*/
int monitor_proc( MPI_Comm coorcomm, int crank, int wrank, 
                  unsigned int preexit, unsigned char** itemidx, 
                  unsigned int* ni, unsigned int verbose, 
                  FILE* verbout )
{
   MPI_Status status;
   MPI_Request req; 
   unsigned int curiflg = 0;
   int per = 0, stp = 0;
   struct ptparams p;
   pthread_t pthrd;
   pthread_attr_t attr;

   *ni = 0;
   *itemidx = NULL;
   status.MPI_TAG = DOWORK; /* init */
   p.preexit = preexit; 
   p.verbose = verbose; 
   p.verbout = verbout;

   if(verbose >= 2)
      fprintf(verbout, "MONITOR WRLD Rank %d, COORDNTR Sub Rank %d\n", wrank, crank);
   
   if( (per = pthread_mutex_init(&mtexstop, NULL)) != 0)
   {   
      fprintf(stderr, "\"%s\" @L %d RANK %d : pthread_mutex_init Failed (pthrd error %d)\n", SRC_FILE, __LINE__, wrank, per);
      return -1;
   }   
   if( (per = pthread_attr_init(&attr)) != 0)
   {   
      fprintf(stderr, "\"%s\" @L %d RANK %d : pthread_attr_init Failed (pthrd error %d)\n", SRC_FILE, __LINE__, wrank, per);
      return -1;
   }   
   if( (per = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0)
   {   
      fprintf(stderr, "\"%s\" @L %d RANK %d : pthread_attr_setdetachstate Failed (pthrd error %d)\n", SRC_FILE, __LINE__, wrank, per);
      pthread_attr_destroy(&attr);
      return -1;
   }
   if( (per = pthread_create(&pthrd, &attr, pbs_poll, (void*) &p)) != 0)
   {
      fprintf(stderr, "\"%s\" @L %d RANK %d : pthread_create Failed (pthrd error %d)\n", SRC_FILE, __LINE__, wrank, per);
      pthread_attr_destroy(&attr);
      return -1;
   }
   pthread_attr_destroy(&attr);

   /* begin checkpointing and pbs poll loop... */
   while(status.MPI_TAG != ENDWORK) 
   {
      curiflg = 0;
      if( MPI_Irecv(&curiflg, 1, MPI_UNSIGNED, crank, MPI_ANY_TAG, coorcomm, &req) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Recv Failed! :(\n", SRC_FILE, __LINE__, wrank);
         return -1;
      }
      
      pthread_mutex_lock(&mtexstop);
      stp = stopissed; 
      pthread_mutex_unlock(&mtexstop);
      if(stp == 1)
      {
         if(verbose >= 1) 
            fprintf(verbout, "MONITOR, shutdown threat issued, MONITOR exiting...\n");
         return 1;
      }

      /* it's assumed at least one message will come otherwise this will wait, and wait, and wait ... */
      if( MPI_Wait(&req, &status) != MPI_SUCCESS)
         fprintf(stderr, "\"%s\" @L %d RANK %d : WARN MPI_Wait Failed! :(\n", SRC_FILE, __LINE__, wrank);
         
      if(status.MPI_TAG == COMPLETED_WORK) 
      {
         if(verbose >= 3) 
            fprintf(verbout, "MONITOR received id %u for a completed unit from rank %d\n", curiflg, status.MPI_SOURCE);
         /* mark complete */
         if( (*itemidx != NULL) && (curiflg < *ni) )
            (*itemidx)[curiflg] = 1;

      }
      else if(status.MPI_TAG == UNIT_TAG)
      {
         if(verbose >= 3) 
            fprintf(verbout, "MONITOR received n items (%u) notice from rank %d\n", curiflg, status.MPI_SOURCE);
         *ni = curiflg;

         if( ( *itemidx = (unsigned char*) calloc( (size_t)(*ni), sizeof(unsigned char)) ) == NULL)
         {
            fprintf(stderr, "\"%s\" @L %d RANK %d : calloc failed : %s\n", SRC_FILE, __LINE__, wrank, strerror(errno));
            return -1;
         }
      }
      else if(status.MPI_TAG == ENDWORK) 
      {
         if(verbose >= 2) 
            fprintf(verbout, "MONITOR received a normal exit signal\n");
         stopissed = 2; /* locking this here not that critical */
      }
   }
   return 0;
}/* monitor_proc */

/*-------------------------------------------------------------------------
* See pxargs.h for details
*/
int work_proc(int rank, unsigned int maxutime, unsigned int verbose, FILE* verbout)
{
   MPI_Status status;
   unsigned char* workmsg;
   char* cmdbuf;
   const unsigned int maxwork = PXMAXARGS; 
   unsigned int i = 0;
   int bfsize, cmdsize;
   WORK_UNIT oneu; 
   
   cmdsize = PATH_MAX+PXARGLENMAX+64;
   /* setup on maximum size */
   bfsize = mpi_sizeof_worku();
   if( (workmsg = (unsigned char*) malloc(bfsize)) == NULL)
   {
      fprintf(stderr, "\"%s\" @L %d RANK %d : malloc failed : %s\n", SRC_FILE, __LINE__, rank, strerror(errno));
      return -1;
   }
   if( (cmdbuf = (char*) malloc(cmdsize)) == NULL)
   {
      fprintf(stderr, "\"%s\" @L %d RANK %d : malloc failed : %s\n", SRC_FILE, __LINE__, rank, strerror(errno));
      free(workmsg);
      return -1;
   }

   while(i <= maxwork) 
   {
      memset(workmsg, 0, bfsize);
      memset(cmdbuf, 0, cmdsize);

      /* Receive a message from the master */
      if( MPI_Recv(workmsg, bfsize, MPI_PACKED, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Recv Failed! :(\n", SRC_FILE, __LINE__, rank);
         free(workmsg); free(cmdbuf);
         return -1;
      }

      /* Check the tag of the received message. */
      if (status.MPI_TAG == ENDWORK) 
      {
         if(verbose >= 2)
            fprintf(verbout, "Rank %d received exit signal\n", rank);
         free(cmdbuf); free(workmsg);
         return 0;
      }
      mpi_worku_unserialize(workmsg, bfsize, &oneu);
      
      snprintf(cmdbuf, cmdsize, "%s %s", oneu.procpath, oneu.pargs); 
      if(verbose == 2)
         fprintf(verbout, "Rank %d received \"%s\" \"%s\"\n", rank, oneu.procpath, oneu.pargs);
      else if(verbose >= 3)
      {
         fprintf(verbout, "Rank %d received WORK UNIT: ", rank);
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
          fprintf(stderr, "%s @L %d RANK %d : Command \"%s\" failed with xpopen\n", SRC_FILE, __LINE__, rank, cmdbuf);

      oneu.was_killed = MODTexceed;
      /* future TODO: set anything else that matters here that may need to be sent back */

      /* now we're finished */
      mpi_worku_serialize(&oneu, workmsg, bfsize);
      if( MPI_Send(workmsg, bfsize, MPI_PACKED, 0, 0, MPI_COMM_WORLD) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Send Failed! :(\n", SRC_FILE, __LINE__, rank);
         free(workmsg); free(cmdbuf);
         return -1;
      }
      i++;
   }

   free(workmsg);
   free(cmdbuf);
   return 0;
}/* work_proc */


