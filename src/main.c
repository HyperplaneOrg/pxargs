/*$**************************************************************************
*
* FILE: 
*    main.c
*
* AUTHOR:
*    Andrew Michaelis, amac@hyperplane.org
* 
* FILE VERSION:
*    0.0.1
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
	fprintf(stdout, "   -m | --max-time <n> :: The maximum runtime in seconds allowed for a arg unit.\n");
	fprintf(stdout, "          If the work unit exceeds n seconds it is killed (if the platform has signals)\n");
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
   int verbose = 0;
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
      {0, 0, 0, 0}         /* terminate */
   };
#endif
   
   FILE* verbout = stdout;
   struct timeval tvl1, tvl2;
   time_t secs = 0;
   int rank = 0;
   int nsize = 0;
   int ercode = 1;
   char* flist = NULL;
   char* procpgrm = NULL;
   WORK_UNIT* wlist = NULL;
   int nlist = 0;
   int randstart = -1;
   int randend = -1;
   char randstr[64] = {0};
   char tempfname[PATH_MAX];
   unsigned int maxutime = 0;

   /* init mpi */
   MPI_Init(&argc, &argv);
   /* set rank */
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   /* how many? */
   MPI_Comm_size(MPI_COMM_WORLD, &nsize);

   if(nsize < 2)
   {
     fprintf(stderr, "\"%s\" @L %d RANK %d : Need > 1 processors, bump up num procs.\n", __FILE__, __LINE__, rank);
     MPI_Abort(MPI_COMM_WORLD, ercode);
   }
   
   if(rank == 0)
   {
      tempfname[0] = '\0';
      randstr[63] = '\0';
#ifdef HAVE_GETOPT_LONG 
      while( (c = getopt_long(argc, argv, "hva:p:wr:Vm:", long_options, &option_index)) != -1 )
#else
      while( (c = getopt(argc, argv, "hva:p:wr:Vm:")) != -1 )
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
                  verbose += 1;
               else if(option_index == 2)
                  flist = optarg;
               else if(option_index == 3)
                  wrkinf = 1;
               else if(option_index == 4)
                  procpgrm = optarg;
               else if(option_index == 5)
               {
                  STRCPY(randstr, optarg, 63); 
                  parse_arg_hyph(randstr, &randstart, &randend);
               }
               else if(option_index == 6)
               {
                  printf("Version %s (built %s)\n", PACKAGE_VERSION, __DATE__);
                  exit(0);
               }
               else if(option_index == 7)
                  maxutime = (unsigned int) atoi(optarg);
               break;
#endif
			   case 'a':
               flist = optarg;
				   break;
			   case 'v':
               verbose += 1;
				   break;
			   case 'm':
               maxutime = (unsigned int) atoi(optarg);
				   break;
			   case 'p':
               procpgrm = optarg;
				   break;
			   case 'w':
               wrkinf = 1;
				   break;
			   case 'r':
               STRCPY(randstr, optarg, 63); 
               parse_arg_hyph(randstr, &randstart, &randend);
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
               fprintf(stderr, "Try -h for help.\n");
               MPI_Abort(MPI_COMM_WORLD, ercode);
               exit(0);
         }
      }

      if(procpgrm == NULL)
      {
         fprintf(stderr, "No processing program or script given. Try -h for help.\n");
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }
      
      if(flist == NULL)
      {
         /* Assume stdin and use a temp file for simplicity... Pipes don't work very well 
            in mpi environments so we have this intermediary copy just for user convenience... */
         if( fstream2tempfile(stdin, tempfname, PATH_MAX) < 0 )
         {
            fprintf(stderr, "\"%s\" @L %d RANK %d : tempfile error (for stdin).\n", __FILE__, __LINE__, rank);
            MPI_Abort(MPI_COMM_WORLD, ercode);
         }
         flist = tempfname;
      }
      
      /* for timing */
      memset(&tvl1, 0 , sizeof(struct timeval));
      memset(&tvl2, 0 , sizeof(struct timeval));
      gettimeofday(&tvl1, NULL);
      
      if(verbose >= 1)
      {
         if(randstart >= 0)
            fprintf(verbout, "COORDNTR INIT WITH %d WORKERS. ARG FILE \"%s\". PROC \"%s\". RAND INTERVAL %d-%d. MAX UNIT TIME %u\n", 
                     nsize-1, flist, procpgrm, randstart, randend, maxutime);
         else
            fprintf(verbout, "COORDNTR INIT WITH %d WORKERS. ARG FILE \"%s\". PROC \"%s\". MAX UNIT TIME %u\n", nsize-1, flist, procpgrm, maxutime);
      }
      
      /* pull in tiles of interest */
      if( (wlist = load_work_list(flist, &nlist)) == NULL)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : Failed to load arg list.\n", __FILE__, __LINE__, rank);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }

      if(nlist <= 0)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : No work loaded. Check the arg list file.\n", __FILE__, __LINE__, rank);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }

      if(verbose >= 3)
      {
         fprintf(verbout, "ARG LIST:\n");
         fprint_worklist(verbout, wlist, nlist);
      }
   }/* rank 0 */
      
   /* set verbosity and have everybody catchup */
   if( MPI_Bcast(&verbose , 1, MPI_INT, 0, MPI_COMM_WORLD) != MPI_SUCCESS)
   {
      fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Bcast Failed! :(\n", __FILE__, __LINE__, rank);
      MPI_Abort(MPI_COMM_WORLD, ercode);
   }
   /* set maxutime */
   if( MPI_Bcast(&maxutime, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD) != MPI_SUCCESS)
   {
      fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Bcast Failed! :(\n", __FILE__, __LINE__, rank);
      MPI_Abort(MPI_COMM_WORLD, ercode);
   }
   
   /*-*-*-* begin processing *-*-*-*-*/
   if(rank == 0)
   {
      /* master */
      if(coordinate_proc(wlist, nlist, procpgrm, nsize, randstart, randend, verbose, verbout) < 0)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : coordinate_proc Failed! :(\n", __FILE__, __LINE__, rank);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }
   }
   else
   {
      if(verbose >= 1)
         fprintf(verbout, "CONTACT ESTB %d of %d WORKERS. VERBOSITY LEVEL %d\n", rank, nsize-1, verbose);
      /* slaves */
      if(work_proc(rank, maxutime, verbose, verbout) < 0)
      {
         fprintf(stderr, "\"%s\" @L %d RANK %d : work_proc Failed! :(\n", __FILE__, __LINE__, rank);
         MPI_Abort(MPI_COMM_WORLD, ercode);
      }
   }
   
   /*-*-*-* end processing *-*-*-*-*/
   fflush(verbout);

   /* complete hold */
   if( MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS)
      fprintf(stderr, "\"%s\" @L %d RANK %d : MPI_Barrier Failed! :(\n", __FILE__, __LINE__, rank);
   
   /* for timing */
   if(rank == 0)
   {
      /* process info */
      if( (verbose >= 3) || (wrkinf ==1) )
      {
         if(wrkinf == 0) 
            fprintf(verbout, "ARG LIST ON EXIT:\n");
            
         fprint_worklist(verbout, wlist, nlist);
      }

      if(verbose >= 1)
      {
         gettimeofday(&tvl2, NULL);
         secs = tvl2.tv_sec - tvl1.tv_sec;
         fprintf(verbout, "WALL TM %lu seconds : %.2f minutes : %.2f hours (np %d, units %d)\n", 
                   (unsigned long int)secs, ((double)secs) / 60.0, (((double)secs) / 60.0) / 60.0, nsize, nlist); 
      }
      if(tempfname[0] != '\0')
      {
         if( remove(tempfname) < 0)
            fprintf(stderr, "\"%s\" @L %d : remove : %s\n", __FILE__, __LINE__, strerror(errno));
      }
   
   }

   MPI_Finalize();
   exit(0);
}/* end main */

