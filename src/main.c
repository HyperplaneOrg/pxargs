/*$**************************************************************************
*
* FILE: 
*    main.c
*
* AUTHOR:
*    Andrew Michaelis, amac@hyperplane.org
* 
* FILE VERSION:
*    0.1.1
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_GETOPT_H 
 #include <getopt.h>
#else
 #ifdef HAVE_UNISTD_H 
  #include <unistd.h> 
 #endif
#endif

#include <sys/time.h>

#include <mpi.h>
#include <pxargs.h>
#include <ptmpf.h>
   
/* bcast array offset for runtime params */
#define RT_VERBOSE 0
#define RT_MAXUNIT_TIME 1
#define RT_HAVE_MONITOR 2 
#define RT_PREEXIT_TIME 3
#define RT_CHKPNT_OPTIDX 4
#define RT_ARGS_OPTIDX 5
#define RT_LEN 6

static const char* SRC_FILE = __FILE__;

/*-------------------------------------------------------------------------------------
*/
static void parse_arg_hyph(char* strp, int* v0, int* v1)
{
   char* strngb = NULL;
   char* ssv = NULL;
   int tmp;

   if(strp == NULL) 
      return;

   if((v0 != NULL) && (v1 != NULL)) 
   { 
      *v0 = 0; *v1 = 0; 
      if( (ssv = STRTOK(strp, "- ", &strngb)) == NULL)
         return;
      *v0 = (int)atoi(ssv);

      if( (ssv = STRTOK(NULL, "- ", &strngb)) == NULL)
         return;
      *v1 = (int)atoi(ssv);

      if((*v0) > (*v1))
      {
         tmp = *v0;
         *v0 = *v1;
         *v1 = tmp;
      }
   }
}/* parse_arg_hyph */

/*-------------------------------------------------------------------------------------
*/
static void print_usage()
{
	fprintf(stdout, "\n   Usage: %s [ OPTIONS ] -p <exec>\n\n",  PACKAGE_NAME);
	fprintf(stdout, "   -a | --arg-file <path> :: The path to the arg list.\n");
	fprintf(stdout, "         The file format is:\n");
	fprintf(stdout, "         <args_1>\n");
	fprintf(stdout, "         <  .   >\n");
	fprintf(stdout, "         <args_n>\n");
	fprintf(stdout, "   -p | --proc <exec> :: The exec or script utility.\n");
	fprintf(stdout, "   -w | --work-analyze :: Prints out job statistics\n");
	fprintf(stdout, "   -r | --random-starts <n-m> :: Randomize initial starts on\n");
	fprintf(stdout, "                        interval n to m. Units are seconds.\n");
	fprintf(stdout, "   -m | --max-time <n> :: The maximum runtime in seconds allowed for a arg unit. If\n");
	fprintf(stdout, "        the work unit exceeds n seconds it is killed (if the platform has signals)\n");
	fprintf(stdout, "   -n | --not-complete <path> :: A file that pxargs attempts to write out unfinished work\n");
	fprintf(stdout, "           to (on master) at pre-exit. Option requires libpbs to be useful; see manpage for info.\n");
	fprintf(stdout, "   -t <sec> :: suggested time, in seconds, to react before the job is due to get the exit\n");
   fprintf(stdout, "               signal from pbs. The default is %u s. See manpage.\n", DEFAULT_PREEXIT_SECONDS);  
	fprintf(stdout, "   -v | --verbose :: Run in verbose mode.\n");
	fprintf(stdout, "   -V | --version :: Print the version then exit.\n");
	fprintf(stdout, "   -h | --help :: This help message.\n");
	fprintf(stdout, "\n");
}/* print_usage */

/*-------------------------------------------------------------------------------------
*/
int main(int argc, char** argv)
{
   int c = 0;
   int wrkinf = 0;
#ifdef HAVE_GETOPT_LONG 
   int option_index = -1;
   static struct option long_options[] =
   {
      {"help", 0, 0, 0},   
      {"verbose", 0, 0, 0},   
      {"arg-file", 1, 0, 0},   
      {"work-analyze", 0, 0, 0},   
      {"proc", 1, 0, 0},   
      {"random-starts", 1, 0, 0},   
      {"version", 0, 0, 0},   
	   {"max-time", 1, 0, 0},  
	   {"not-complete", 1, 0, 0}, 
      {0, 0, 0, 0}         /* terminate */
   };
#endif
   extern int optind;
   FILE* verbout = stdout;
   struct timeval tvl1, tvl2;
   time_t secs = 0;
   int rank = 0, nsize = 0;
   int nworkers = 0, wrankstart = 0;
   int ercode = 1;
   char* flist = NULL;
   char* procpgrm = NULL;
   WORK_UNIT* wlist = NULL;
   unsigned int nlist = 0;
   int randstart = -1;
   int randend = -1;
   char randstr[64];
   char tempfname[PATH_MAX+1];
   char* chkpntfname = NULL;
   unsigned char* chkpntidx = NULL;
   unsigned int nchkpntidx = 0;
   unsigned int rtparams[RT_LEN] ; 
   MPI_Group maingrp, crdmntgrp;
   MPI_Comm subcomm = MPI_COMM_NULL;
   int subgrp[2]= {0, 1};
   int subrank = 0;
   int mret;

   /* init mpi */
   MPI_Init(&argc, &argv);
   /* set rank */
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   MPI_Comm_size(MPI_COMM_WORLD, &nsize);

   memset(rtparams, 0, sizeof(unsigned int) * RT_LEN);
   rtparams[RT_PREEXIT_TIME] = DEFAULT_PREEXIT_SECONDS; 
   memset(&maingrp, 0, sizeof(MPI_Group));
   memset(&crdmntgrp, 0, sizeof(MPI_Group));

   if(rank == 0)
   {
      memset(randstr, 0, 64);
      memset(tempfname, 0, PATH_MAX+1);
#ifdef HAVE_GETOPT_LONG 
      while( (c = getopt_long(argc, argv, "hva:p:wr:Vm:t:n:", long_options, &option_index)) != -1 )
#else
      while( (c = getopt(argc, argv, "hva:p:wr:Vm:t:n:")) != -1 )
#endif
      {
         switch (c)
         {
#ifdef HAVE_GETOPT_LONG 
            case 0:
               if(option_index == 0)
               {
				      print_usage();
				      exit(0);
               }
               else if(option_index == 1)
                  rtparams[RT_VERBOSE] += 1;
               else if(option_index == 2)
               {
                  flist = optarg;
                  rtparams[RT_ARGS_OPTIDX] = optind - 1;
               }
               else if(option_index == 3)
                  wrkinf = 1;
               else if(option_index == 4)
                  procpgrm = optarg;
               else if(option_index == 5)
               {
                  strncpy(randstr, optarg, 63); 
                  parse_arg_hyph(randstr, &randstart, &randend);
               }
               else if(option_index == 6)
               {
                  printf("Version %s (built %s)\n", PACKAGE_VERSION, __DATE__);
                  MPI_Abort(MPI_COMM_WORLD, 0);
               }
               else if(option_index == 7)
                  rtparams[RT_MAXUNIT_TIME] = (unsigned int) atol(optarg);
               else if(option_index == 8)
               {
#ifdef HAVE_LIBPBS 
                  chkpntfname = optarg;
                  rtparams[RT_HAVE_MONITOR] = 1;
                  rtparams[RT_CHKPNT_OPTIDX] = optind - 1;
#else
                  fprintf(stderr, "WARN: --not-complete option not enabled.\n");
#endif
               }
               break;
#endif
			   case 'a':
               flist = optarg;
               rtparams[RT_ARGS_OPTIDX] = optind - 1;
				   break;
			   case 'v':
               rtparams[RT_VERBOSE] += 1;
				   break;
			   case 'm':
               rtparams[RT_MAXUNIT_TIME] = (unsigned int) atol(optarg);
				   break;
			   case 'p':
               procpgrm = optarg;
				   break;
			   case 'w':
               wrkinf = 1;
				   break;
			   case 't':
               rtparams[RT_PREEXIT_TIME] = (unsigned int) atol(optarg);
				   break;
			   case 'r':
               strncpy(randstr, optarg, 63); 
               parse_arg_hyph(randstr, &randstart, &randend);
				   break;
			   case 'n':
#ifdef HAVE_LIBPBS 
               chkpntfname = optarg;
               rtparams[RT_HAVE_MONITOR] = 1;
               rtparams[RT_CHKPNT_OPTIDX] = optind - 1;
#else
               fprintf(stderr, "WARN: -n option not enabled.\n");
#endif
				   break;
			   case 'V':
               printf("Version %s (built %s)\n", PACKAGE_VERSION, __DATE__);
               MPI_Abort(MPI_COMM_WORLD, 0);
               exit(0);
			   case 'h':
			      print_usage();
               MPI_Abort(MPI_COMM_WORLD, 0);
               exit(0);
            default:
               fprintf(stderr, "Bad arg given. Try -h for help.\n");
               MPI_Abort(MPI_COMM_WORLD, ercode);
               exit(0);
         }
      }

      /* basic options sanity check */
      if(procpgrm == NULL)
      {
         fprintf(stderr, "No processing program or script given. Try -h for help.\n");
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }
      if(rtparams[RT_HAVE_MONITOR] == 1)
      {
         nworkers = nsize - 2;
         wrankstart = 2;
      }
      else
      {
         nworkers = nsize - 1;
         wrankstart = 1;
      }
      if(nworkers == 0) 
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : Need more mpi procs. Bump up num procs (np %d, workers %d, 1 monitor).\n", 
                                                   SRC_FILE, __LINE__, rank, nsize, nworkers);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }
      
      if(flist == NULL)
      {
         /* Assume stdin and use a temp file for simplicity... Pipes don't work well 
            in mpi environments so we have a intermediary copy for user convenience... */
         if( fstream2tempfile(stdin, tempfname, PATH_MAX) < 0 )
         {
            fprintf(stderr, "\"%s\" @L %d RANK %d : tempfile error (for stdin).\n", SRC_FILE, __LINE__, rank);
            MPI_Abort(MPI_COMM_WORLD, ercode);
         }
         flist = tempfname;
      }
      /* for timing */
      memset(&tvl1, 0 , sizeof(struct timeval));
      memset(&tvl2, 0 , sizeof(struct timeval));
      gettimeofday(&tvl1, NULL);
      
      if(rtparams[RT_VERBOSE] >= 1)
      {
         if(randstart >= 0)
            fprintf(verbout, "COORDNTR INIT WITH %d WORKERS. ARG FILE \"%s\". PROC \"%s\". RAND INTERVAL %d-%d. MAX UNIT TIME %u. CHECK PNT FILE \"%s\"\n", 
                              nworkers, flist, procpgrm, randstart, randend, rtparams[RT_MAXUNIT_TIME], (chkpntfname != NULL ? chkpntfname : "NA"));
         else
            fprintf(verbout, "COORDNTR INIT WITH %d WORKERS. ARG FILE \"%s\". PROC \"%s\". MAX UNIT TIME %u. CHECK PNT FILE \"%s\"\n", 
                              nworkers, flist, procpgrm, rtparams[RT_MAXUNIT_TIME], (chkpntfname != NULL ? chkpntfname : "NA"));
      }
      
      /* pull in tiles of interest */
      if( (wlist = load_work_list(flist, &nlist)) == NULL)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : Failed to load arg list.\n", SRC_FILE, __LINE__, rank);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }

      if(nlist == 0)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : No work loaded. Check the arg list file.\n", SRC_FILE, __LINE__, rank);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }

      if(rtparams[RT_VERBOSE] >= 3)
      {
         fprintf(verbout, "ARG LIST:\n");
         fprint_worklist(verbout, wlist, nlist);
      }
   }/* rank 0 setup */
      
   /* prime comm, set basic rt params and have everybody get ready */
   if( MPI_Bcast(rtparams, RT_LEN, MPI_UNSIGNED, 0, MPI_COMM_WORLD) != MPI_SUCCESS)
   {
      fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Bcast Failed! :(\n", SRC_FILE, __LINE__, rank);
      MPI_Abort(MPI_COMM_WORLD, ercode);
   }

   /* If there is a monitor, create another sub communicator */
   if(rtparams[RT_HAVE_MONITOR] == 1) 
   {
      /* another communicator... */
      if( MPI_Comm_group(MPI_COMM_WORLD, &maingrp) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Comm_group Failed! :(\n", SRC_FILE, __LINE__, rank);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }
      if( MPI_Group_incl(maingrp, 2, subgrp, &crdmntgrp) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Group_incl Failed! :(\n", SRC_FILE, __LINE__, rank);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }
      if( MPI_Comm_create(MPI_COMM_WORLD, crdmntgrp, &subcomm) != MPI_SUCCESS)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Comm_create Failed! :(\n", SRC_FILE, __LINE__, rank);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }
      if(subcomm != MPI_COMM_NULL)
      {
         if( MPI_Comm_rank(subcomm, &subrank) != MPI_SUCCESS)
         {
            fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Comm_rank Failed for sub comm group\n", SRC_FILE, __LINE__, rank);
            MPI_Abort(MPI_COMM_WORLD, ercode);
         }
         /* set to the opposite of "my" sub rank (there is only 2 ranks in this group) */
         if(subrank == 1)
            subrank = 0;
         else if(subrank == 0)
            subrank = 1;
         else
         {
            fprintf(stderr, "\"%s\" @L %d RANK %d : SUB RANK %d is suspicious?\n", SRC_FILE, __LINE__, rank, subrank);
            MPI_Abort(MPI_COMM_WORLD, ercode);
         }
      }
   }
   
   /*-*-*-* begin processing *-*-*-*-*/

   if(rank == 0) /* master | producer */
   {
      if( coordinate_proc( wlist, nlist, procpgrm, nworkers, wrankstart, randstart, randend, 
                           subcomm, subrank, rtparams[RT_VERBOSE], verbout) < 0  )
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : coordinate_proc Failed! :(\n", SRC_FILE, __LINE__, rank);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }
   }
   else if( (rank == 1) && (rtparams[RT_HAVE_MONITOR] == 1) ) /* monitor, if requested */ 
   {
      /* this set by the coordinator rank and the index was broadcast */
      if( rtparams[RT_ARGS_OPTIDX] == 0)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : ** WARN ** : monitor checkpointing does not currently work with unix pipes.\n", SRC_FILE, __LINE__, rank);
         flist = NULL;
      }
      else
      {
         if( ( flist = strchr(argv[ rtparams[RT_ARGS_OPTIDX] ], '=')) != NULL)  
            flist = &flist[1];
         else
            flist = argv[ rtparams[RT_ARGS_OPTIDX] ];  
      }

      if( (chkpntfname = strchr(argv[ rtparams[RT_CHKPNT_OPTIDX] ], '=')) != NULL)  /* maybe long opt */
         chkpntfname = &chkpntfname[1];
      else
         chkpntfname = argv[ rtparams[RT_CHKPNT_OPTIDX] ];  /* standard opt */

      if(rtparams[RT_VERBOSE] >= 1)
         fprintf(verbout, "MONITOR INIT, RANK %d. VERBOSITY LEVEL %u, PRE-EXT CHK PNT TM %u (s), CHECK PNT FILE \"%s\" ARG FILE \"%s\"\n", 
                                     rank, rtparams[RT_VERBOSE], rtparams[RT_PREEXIT_TIME], chkpntfname, (flist != NULL ? flist : "NA"));

      /*-* start the monitor *-*/
      mret = monitor_proc( subcomm, subrank, rank, rtparams[RT_PREEXIT_TIME], &chkpntidx, &nchkpntidx, rtparams[RT_VERBOSE], verbout );

      if( (mret == 1) && (chkpntidx != NULL) && (flist != NULL) )
      {
         /* We need to get the original list here; why don't we just get it from rank 0 ? We don't because it is messy, it can 
            take a long time, dancing around deadlocks, etc.. attempting to interrupt the coordinator (stop it and get the list) 
            while activitiy is going on. We leave the coordinator alone and let it die on it's own but only after we've written the 
            incompleted work as given by the monitor. This can be an issue if one is using many 1000's of procs. */
         if( (wlist = load_work_list(flist, &nlist)) == NULL)
            fprintf(stderr, "\"%s\" @L %d RANK %d : WARN : monitor failed to load arg list on post proc diff.\n", SRC_FILE, __LINE__, rank);
         else
         {
            if(rtparams[RT_VERBOSE] >= 2)
               fprintf(verbout, "MONITOR, RANK %d. DUMPING CHECK PNT, %u items to consider...\n", rank, nchkpntidx);

            if( dump_work_list_by_index(chkpntfname, wlist, nlist, chkpntidx, nchkpntidx) < 0)
               fprintf(stderr, "\"%s\" @L %d RANK %d : WARN : monitor failed to dump work list.\n", SRC_FILE, __LINE__, rank);

            free(wlist);
            free(chkpntidx);
         }
      }
      else if(mret < 0)
         fprintf(stderr, "\"%s\" @L %d RANK %d : WARN : monitor_proc Failed! :(\n", SRC_FILE, __LINE__, rank);
   }
   else /* slaves | consumer */
   {
      if(rtparams[RT_VERBOSE] >= 1)
         fprintf(verbout, "CONTACT ESTB RANK %d. VERBOSITY LEVEL %u\n", rank, rtparams[RT_VERBOSE]);
      
      if(work_proc(rank, rtparams[RT_MAXUNIT_TIME], rtparams[RT_VERBOSE], verbout) < 0)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : work_proc Failed! :(\n", SRC_FILE, __LINE__, rank);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }
   }
   
   /*-*-*-* end processing *-*-*-*-*/

   fflush(verbout);
   /* completion hold */
   if( MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS)
      fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Barrier Failed! :(\n", SRC_FILE, __LINE__, rank); 
   fflush(verbout);
   
   if(rank == 0)
   {
      /* show timing */
      if( (rtparams[RT_VERBOSE] >= 3) || (wrkinf ==1) )
      {
         if(wrkinf == 0) 
            fprintf(verbout, "ARG LIST ON EXIT:\n");
            
         fprint_worklist(verbout, wlist, nlist);
      }
      if(rtparams[RT_VERBOSE] >= 1)
      {
         gettimeofday(&tvl2, NULL);
         secs = tvl2.tv_sec - tvl1.tv_sec;
         fprintf(verbout, "WALL TM %lu seconds : %.2f minutes : %.2f hours (np %d, units %u)\n", 
                   (unsigned long int)secs, ((double)secs) / 60.0, (((double)secs) / 60.0) / 60.0, nsize, nlist); 
      }
      /* clean up if needed */
      if(tempfname[0] != '\0')
      {
         if( remove(tempfname) < 0)
            fprintf(stderr, "\"%s\" @L %d : remove : %s\n", SRC_FILE, __LINE__, strerror(errno));
      }
      free(wlist);
   }

   if(rtparams[RT_HAVE_MONITOR] == 1) 
   {
      if(subcomm != MPI_COMM_NULL)
       MPI_Comm_free(&subcomm);
      MPI_Group_free(&maingrp);
      MPI_Group_free(&crdmntgrp);
   }

   MPI_Finalize();
   exit(0);
}/* end main */
