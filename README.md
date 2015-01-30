pxargs
======

pxargs is a simple work queue with rudimentary load balancing using the message passing interface (mpi) facility.
It builds and executes command lines from a newline delimited list contained in FILE or from standard input. pxargs will
run the commandlines in parallel using mpi. It is losely based on the venerable xargs. See the manpage for more details.

Notes
==

This utility is a proof of concept so the construction is a bit messy. pxargs has been used for fairly large (> 11,000 cores)
and diverse processing pipelines. It has been found to be a convenient mechanism of brining complex serial code to large scale
parallel setups in minutes, with no added software development.

If the user's compute environment involves a pbs scheduler (pro or torque) then they may want to take a peek at the -n 
and -t options. pxargs will attempt to take an inventory of non-completed commandlines (-t secs before the scheduler 
is due to kill the job). This facility is useful for chaining jobs together where a single job cannot complete within 
the pbs queue walltime limit(s).

