/*$**************************************************************************
*
* FILE: 
*    pxargs.h
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
*  Copyright (C) 2012, 2014, 2015 Andrew Michaelis
* 
***************************************************************************$*/
#ifndef PXARGS_H
#define PXARGS_H 1

#ifdef HAVE_CONFIG_H
 #include <config.h>
#endif

#include <stdio.h>
#include <limits.h>
#include <sys/time.h>
#include <string.h>

#include <mpi.h>

#ifndef PATH_MAX
 #define PATH_MAX USER_PATH_MAX 
#endif

/* From rank info */
#define RANK_UNASSIGNED -1

/* signal code for no more work */
#define ENDWORK 2
/* signal code for more work */
#define DOWORK 4

/* signal code work was completed */
#define COMPLETED_WORK 8

/* signal for unit info */
#define UNIT_TAG 16 

/* YES or NO, True or False, etc... */
#define PX_YES 1 
#define PX_NO 0 

/* Maximum arg string length */
#define PXARGLENMAX 8192

/* Default sh path path */
#define SHL_PATH "/bin/sh"
#define SHL_STR "sh"

/* The default time, in seconds, to stop execution 
   before a pbs exit signal and do a "checkpoint" */
#define DEFAULT_PREEXIT_SECONDS 120
#define DEFAULT_PREEXIT_MIN_POLL_SECONDS 90 

#ifdef HAVE_STRTOK_R 
 #define STRTOK(s, d, sp) (strtok_r(s, d, sp))
#else
 #define STRTOK(s, d, sp) (strtok(s, d))
#endif

/* Maximum number of jobs per worker, 2^32 - 1 */
#define PXMAXARGS 4294967295

/* 
* Note, you will need to change the routines:
*  mpi_sizeof_worku
*  mpi_worku_serialize
*  mpi_worku_unserialize
* If you change the struct def below
*/
typedef struct
{
   /* parameters for procpath */ 
   char pargs[PXARGLENMAX]; 
   /* process time */
   long proc_secs;
   /* responsible rank */
   int resrank;
   /* id tag */
   unsigned int id_tag ;  
   /* was the unit killed, 0=No, 1=yes */
   int was_killed;
   /* the last process/operation preformed script/exe */ 
   char procpath[PATH_MAX]; 
   /* TODO: add more... */
} WORK_UNIT;

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   Loads the list of work unit arguments to be processed. The file format is of:
*     <args_1> 
*     <  .   > 
*     <  .   > 
*     <args_n> 
*
* INPUTS:
*    fname => The text file path
*
* OUTPUTS:
*    WORK_UNIT => WORK_UNITs loaded from the file fname.
*    n => the number of units (valid lines in file) in WORK_UNIT*
*
*/
WORK_UNIT* load_work_list(const char* fname, unsigned int* n);

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   This routine will dump our the args in a WORK_UNIT list using an index (see below).
*
* INPUTS:
*   fname => The file path to dump the WORK_UNIT args to
*   wlist => The list to dump
*   nlist => The number of WORK_UNITs in wlist
*   windex => The index that contains the id's of the WORK_UNITs in wlist to write.
*             A 0 at index n implies don't dump, a 1 at index n implies dump the args 
*   nindex => The number of items in windex 
*
* RETURN: 
*   A value of 0 = success, < 0 = failed
*/
int dump_work_list_by_index( const char* fname, WORK_UNIT* wlist, unsigned int nlist, 
                             unsigned char* windex, unsigned int nindex);

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   Routine prints a work unit list
*
* INPUTS:
*    fout => output file pointer
*    list => the tile list
*    n => number of units in the list
*
*/
void fprint_worklist(FILE* fout, WORK_UNIT* wlist, unsigned int n); 


/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   Routine prints a WORK_UNIT structure 
*
* INPUTS:
*    fout => output file pointer
*    work => the work unit list
*
* OUTPUTS:
*    None
*/
void fprint_worku(FILE* fout, WORK_UNIT* worku);

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   Coordinator routine for asynchronous processing.
*
* INPUTS:
*    work => the work units list
*    n => the number of tiles
*    proc => processor script/program path
*    nworkers => the number of worker processors involved
*    rankstart => the starting rank of the workers (assume contiguous 
*                 ranks, rankstart to rankstart+nworkers)
*    randstart => if random release of the initial workload is
*                 desired then set this to a value >= 0 (seconds)
*    randend => see above, the closing interval value

*    e.g. randstart=1,randend=10, causes the program to wait 
*         between 1 and 10 seconds with each release of the initial
*         work distribution.
*    Note if these values are < 0 then the parameters are ignored
*    and work is distributed without waiting.
*    moncomm => if a monitor process exists this is the communicator handle.
*               This can be MPI_COMM_NULL...
*    mnrank => if moncomm exists then this should be set to the rank
*             the monitor has in moncomm group
*    verbose => the verbosity level
*    verbout => the verbosity file stream
*
* RETURN: 
*   A value of 0 = success, < 0 = failed
*/
int coordinate_proc( WORK_UNIT* wunits, unsigned int n, const char* proc, 
                     int nworkers, int rankstart, int randstart, 
                     int randend, MPI_Comm moncomm, int mnrank, 
                     unsigned int verbose, FILE* verbout
                   );

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   worker routine for asynchronous processing.
*
* INPUTS:
*    rank => the worker's rank
*    maxutime => the max run time a unit is allowed to run (signals must be present on the platform) 
*    verbose => the verbosity level
*    verbout => the verbosity file stream
*
* RETURN: 
*   A value of 0 = success, < 0 = failed
*/
int work_proc(int rank, unsigned int maxutime, unsigned int verbose, FILE* verbout);

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   A monitor routine for asynchronous event handling.
* INPUTS:
*    coorcomm => communicator handle, for 'talking' to the coordinator proc.
*    crank => the coordinator's rank for the sub comm coorcomm 
*    wrank => the rank for comm world
*    preexit => Notify of a pre-exit event n seconds before the scheduling 
*               facility is due to terminate this pxargs job. This is a 
*               suggested time, it will not be exact (libpbs required).
*    verbose => the verbosity level
*    verbout => the verbosity file stream
* 
* OUTPUTS:
*    itemidx => An boolean array for which items were completed (1=yes, 0=no).
*               Note, this array runs 'parallel' to the work units list id's 
*               (wunits parameter in coordinate_proc), e.g. index[20] is work unit
*               with id 20.
*    ni => The length of itemidx.
*
* RETURN: 
*   A value of 0 implies a successful run, i.e. nothing out of the ordinary happened
*   A value of < 0 implies something failed badly
*   A value of 1 implies a forced early exit notice by the scheduling.
*
* NOTES:
*   The caller is responsible for freeing itemidx if it is not NULL
*/
int monitor_proc( MPI_Comm coorcomm, int crank, int wrank, 
                  unsigned int preexit, unsigned char** itemidx, 
                  unsigned int* ni, unsigned int verbose, 
                  FILE* verbout );

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*  Returns the size of a WORK_UNIT type.
*
* RETURN: 
*   The number of bytes.
*/
int mpi_sizeof_worku();

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   Packs a work unit structure for sending/passing
*
* INPUTS:
*   work => the work structure to serialize
*   bufsize => the buffer size
*
* OUTPUTS:
*   buf => a buffer, which should be of adequate size, that the work unit is packed into
*/
void mpi_worku_serialize(WORK_UNIT* work, void* buf, int bufsize);

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   Unpacks a work unit structure 
*
* INPUTS:
*   buf => a buffer of adequate size that was packed by mpi_work_serialize
*   bufsize => the buffer size
*
* OUTPUTS:
*   work => the work structure with the data from buf 
*/
void mpi_worku_unserialize(void* buf, int bufsize, WORK_UNIT* work);

#endif
