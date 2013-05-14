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
#define RANK_UNASSINGED -1

/* signal code for no more work */
#define ENDWORK 2
/* signal code for more work */
#define DOWORK 4

/* YES or NO, True or False, etc... */
#define PX_YES 1 
#define PX_NO 0 

/* Maximum arg string length */
#define PXARGLENMAX 8192

#ifdef HAVE_STRTOK_R 
 #define STRTOK(s, d, sp) (strtok_r(s, d, sp))
#else
 #define STRTOK(s, d, sp) (strtok(s, d))
#endif

#ifdef HAVE_STRNCPY 
 #define STRCPY(s, d, l) (strncpy(s, d, l))
#else
 #define STRCPY(s, d, l) (strcpy(s, d))
#endif

/* Maximum number of jobs per worker, 2^31 */
#define PXMAXARGS 2147483647 

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
   int id_tag ;  
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
WORK_UNIT* load_work_list(const char* fname, int* n);

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   Routine prints a work unit list
*
* INPUTS:
*    fout => output file pointer
*    list => the tile list
*    n => number of tile in the list
*
* OUTPUTS:
*    None
*/
void fprint_worklist(FILE* fout, WORK_UNIT* wlist, int n); 


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
*
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
*    groupsz => the number of processors involved
*    randstart => if random release of the initial workload is
*                 desired then set this to a value >= 0 (seconds)
*    randend => see above, the closing interval value

*    e.g. randstart=1,randend=10, causes the program to wait 
*         between 1 and 10 seconds with each release of the initial
*         work distribution.
*    Note if these values are < 0 then the parameters are ignored
*    and work is distributed without waiting.
*         
*    verbose => the verbosity level
*    verbout => the verbosity file stream
*
* OUTPUTS:
*   None
*
* RETURN: A value of 0 = success, < 0 = failed
*/
int coordinate_proc( WORK_UNIT* work, int n, const char* proc, 
                     int groupsz, int randstart, int randend,
                     int verbose, FILE* verbout
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
* OUTPUTS:
*   None
*
*/
int work_proc(int rank, unsigned int maxutime, int verbose, FILE* verbout);

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*  Returns the size of a work unit structure.
*
* INPUTS:
*   None
*
* OUTPUTS:
*   None
*
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
*
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
*
*/
void mpi_worku_unserialize(void* buf, int bufsize, WORK_UNIT* work);

#endif
