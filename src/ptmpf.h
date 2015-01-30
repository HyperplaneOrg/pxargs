/*$**************************************************************************
*
* FILE: 
*    ptmpf.h
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
#ifndef PXTMPF_H
#define PXTMPF_H 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

/*--------------------------------------------------------------------------------------------
* 
* DESCRIPTION:
*   The routine creates a temporary file, reads in all data from a stream and 
*   writes the data to the tempfile. An attempt is made to create the temp file 
*   in a secure manner.
*
* INPUTS:
*    stream => FILE* to read the data from.
*    fnamebuflen => The buffer lenght (bytes) of fnamebuf, see below.
*
* OUTPUTS:
*    fnamebuf => The name of the tempfile. Caller is responsible for deletion.
*
* RETURN:
*    < 0 on failure, 0 on sucess.
*/
int fstream2tempfile(FILE* stream, char* fnamebuf, size_t fnamebuflen);

#endif
