pxargs
======

pxargs is a simple work queue with rudimentary load balancing using the message passing interface (mpi) facility.
It builds and executes command lines from a newline delimited list contained in FILE or from standard input. pxargs will
run the commandlines in parallel using mpi. It is loosely based on the venerable xargs.

This utility is a proof of concept so the construction is a bit messy. pxargs has been used for fairly large (> 11,000 cores)
and diverse processing pipelines. It has been found to be a convenient mechanism of bringing complex serial code to large scale
parallel setups in minutes, with no added software development.

See the manpage for more details.

### PBS Notes
If the user's compute environment involves a pbs scheduler (pro or torque) then they may want to take a peek at the
-n and -t options. pxargs will attempt to take an inventory of non-completed commandlines (-t secs before the
scheduler is due to kill the job). This facility is useful for chaining jobs together where a single job cannot
complete within the pbs queue walltime limit(s).

##### PBS Chunk-level Resources
Because pxargs uses one mpi process for scheduling work, and possibly another to monitor completed and non completed
command lines (see above), the user may want to consider using the PBS chunk-level request capabilities
when possible. You may want to pack ONE node with more processes, as ONE or TWO mpi process may not be doing much
work if the job is small. See the manpage for details.

### Building

You will need gnu autotools to generate a new configure script along with an mpi compiler and or libraries. If you want to use pbs 
integration then you should add the PBS libraries and includes in your environ, e.g. CPPFLAGS and LDFLAGS 

`$ ./autogen.sh`

`$ ./configure CPPFLAGS=-I/PBS.12/include/ LDFLAGS=-L/PBS.12/lib/`

`$ make`

`$ make install`

