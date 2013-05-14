#!/bin/bash
# FILE:
#  package.sh
#
# VERSION:
#  0.0.2, Wed Mar  7 10:22:44 PST 2012
#
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
#

VERSION='0.0.3'
PACKAGE='pxargs-'$VERSION

cdr=`basename $PWD`
if [ $cdr != $PACKAGE ]; then
   echo "Please run package.sh from the $PACKAGE directory"
   exit 1
fi

cd ../

tar -c -z -v -f $PACKAGE'.tgz' $PACKAGE

sha1sum $PACKAGE'.tgz' > $PACKAGE'.tgz.sha1'

