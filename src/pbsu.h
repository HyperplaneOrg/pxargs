/*$**************************************************************************
*
* FILE: 
*    pbsu.h
*
* AUTHOR:
*    Andrew Michaelis, amac@hyperplane.org
* 
* FILE VERSION:
*    0.0.1
* 
# LICENSE:
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
#ifndef PBSU_H
#define PBSU_H 1

#ifdef HAVE_CONFIG_H
 #include <config.h>
#endif

#if defined(HAVE_LIBPBS) && defined(HAVE_LIBPTHREAD) 

/* This will work for PBS_JOBID=12345.<hostname>[.<netname>] */
#define PBS_JOBID_REGX "([0-9]+)\\.([a-z0-9]+)(.*)" 

/*------------------------------------------------------------------------------
* DESCRIPTION:
*  This routine attempts to determine the pbs host name and the pbs 
*  job ID of the current process via the environment variable PBS_JOBID
*   
* INPUTS:
*  phlen -> The length of the pbsHostBuff (see below)
*  jlen -> The length of the jobIdBuff (see below)
*  It is assumed the calling process is associated with the 
*  current PBS_JOBID environment variable.
*
* OUTPUTS:
*  pbsHostBuff -> The host name of the pbs server
*  jobIdBuff -> The full pbs job id
*
* RETURN:
*  < 0 on failure, 0 on success.
*/
int pbsu_this_job_info(char* pbsHostBuff, size_t phlen, char* jobIdBuff, size_t jlen);


/*------------------------------------------------------------------------------
* DESCRIPTION:
*  This routine attempts to connect to a pbs server. 
*   
* INPUTS:
*  pbsServer -> The host name of the pbs server
*
* RETURN:
*  < 0 on failure, the pbs server connection ID (libpbs) on success.
*/
int pbsu_server_connect(char* pbsServer);

/*------------------------------------------------------------------------------
* DESCRIPTION:
*  See the pbsu_server_connect routine. 
*/
void pbsu_server_close(int conn);

/*------------------------------------------------------------------------------
* DESCRIPTION:
*  This routine attempts to get the time left (in seconds) for the jobID via the
*  pbs server connection conn. See pbsu_server_connect routine.
*   
* INPUTS:
*  conn -> A valid pbs server connection
*  jobID -> A valid jobID string
*
* RETURN:
*  0 on failure, else the number of seconds remaining for job jobID
*/
unsigned int pbsu_job_time_left(int conn, char* jobID);

#endif

#endif

