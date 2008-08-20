/*
 schedtool
 Copyright (C) 2002-2006 by Freek
 Please contact me via freshmeat.net
 Release under GPL, version 2
 Use at your own risk.
 Inspired by setbatch (C) 2002 Ingo Molnar

 01/2006:
 included support for SCHED_IDLEPRIO for ck kernels

 01/2004:
 included SCHED_ISO patch by Con Kolivas

 11/2004:
 add probing for some features (priority)

 Born in the need of querying and setting SCHED_* policies.
 All output, even errors, go to STDOUT to ease piping.

 - Setting CPU-affinity is now supported, too.

 - schedtool can now be used to exec a new process with specific parameters.

 - nice functionality is incorporated.


 Content:

 main code
 cmd-line parsing
 the engine
 set_/print_process
 usage

 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#include <unistd.h>
#include <stdint.h>

/* this gets us the list of syscalls */
#include <linux/unistd.h>

#include "error.h"
#include "util.h"

/*
 CPU-affinity stuff
 have an hack to include support for a newer kernel with older headers
 check if support is really there

 It's time for autoconf, baby.
 */

/* provide support for the syscalls even if the libc doesn't know about it */
#ifdef HAVE_AFFINITY

#ifdef AFFINITY_HACK
#include "syscall_magic.h"
#endif

#ifndef __NR_sched_getaffinity
#error You tried to build with support for affinity, but your system/headers are not ready.
#error Please see the file INSTALL for more information.
#endif

#endif

/* various operation modes: print/set/affinity/fork */
#define MODE_NOTHING	0x0
#define MODE_PRINT	0x1
#define MODE_SETPOLICY	0x2
#define MODE_AFFINITY	0x4
#define MODE_EXEC	0x8
#define MODE_NICE       0x10
#define VERSION "1.3.0"

/*
 constants are from the O(1)-sched kernel's include/sched.h
 I don't want to include kernel-headers.
 Included those defines for improved readability.
 */
#undef SCHED_NORMAL
#undef SCHED_FIFO
#undef SCHED_RR
#undef SCHED_BATCH
#define SCHED_NORMAL	0
#define SCHED_FIFO	1
#define SCHED_RR	2
#define SCHED_BATCH	3
#define SCHED_ISO	4
#define SCHED_IDLEPRIO	5

/* for loops */
#define SCHED_MIN SCHED_NORMAL
#define SCHED_MAX SCHED_IDLEPRIO

#define CHECK_RANGE_POLICY(p) (p <= SCHED_MAX && p >= SCHED_MIN)
#define CHECK_RANGE_NICE(n) (n <= 20 && n >= -20)

char *TAB[] = {
	"N: SCHED_NORMAL",
	"F: SCHED_FIFO",
	"R: SCHED_RR",
	"B: SCHED_BATCH",
	"I: SCHED_ISO",
	"D: SCHED_IDLEPRIO",
	0
};

/* call it engine_s in lack of a better name */
struct engine_s {

	int mode;
	int policy;
	int prio;
        int nice;
	cpu_set_t aff_mask;

	/* # of args when going in PID-mode */
	int n;
	char **args;
};


int engine(struct engine_s *e);
int set_process(pid_t pid, int policy, int prio);
int parse_affinity(cpu_set_t *, char *arg);
int set_affinity(pid_t pid, cpu_set_t *mask);
int set_niceness(pid_t pid, int nice);
void probe_sched_features();
void get_prio_min_max(int policy, int *min, int *max);
void print_prio_min_max(int policy);
void print_process(pid_t pid);
void usage(void);


extern char *optarg;
extern int optind, opterr, optopt;
extern int errno;

int main(int ac, char **dc)
{
	/*
	 policy: -1, to indicate it was not set;
	 nice: 10, as nice/renice have as default;
	 prio: 0 per default
	 mode: MODE_NOTHING, no options set
	 */
	int policy=-1, nice=10, prio=0, mode=MODE_NOTHING;

	/*
	 aff_mask: zero it out
	 */
	cpu_set_t aff_mask;
        CPU_ZERO(&aff_mask);

        /* for getopt() */
	int c;

	if (ac < 2) {
		usage();
		return(0);
	}

	while((c=getopt(ac, dc, "+NFRBID012345M:a:p:n:ervh")) != -1) {

		switch(c) {
		case '0':
		case 'N':
			policy=SCHED_NORMAL;
                        mode |= MODE_SETPOLICY;
			break;
		case '1':
		case 'F':
			policy=SCHED_FIFO;
                        mode |= MODE_SETPOLICY;
			break;
		case '2':
		case 'R':
			policy=SCHED_RR;
                        mode |= MODE_SETPOLICY;
			break;
		case '3':
		case 'B':
			policy=SCHED_BATCH;
                        mode |= MODE_SETPOLICY;
			break;
		case '4':
		case 'I':
			policy=SCHED_ISO;
                        mode |= MODE_SETPOLICY;
			break;
		case '5':
		case 'D':
			policy=SCHED_IDLEPRIO;
                        mode |= MODE_SETPOLICY;
			break;
		case 'M':
			/* manual setting */
			policy=atoi(optarg);
			mode |= MODE_SETPOLICY;
			break;
		case 'a':
#ifdef HAVE_AFFINITY
			mode |= MODE_AFFINITY;
			parse_affinity(&aff_mask, optarg);
                        break;
#else
			printf("ERROR: compile-time option CPU-affinity is not supported\n");
                        return(-1);
#endif
		case 'n':
                        mode |= MODE_NICE;
			nice=atoi(optarg);
                        break;
		case 'e':
			mode |= MODE_EXEC;
			break;
		case 'p':
			prio=atoi(optarg);
			break;
		case 'r':
                        probe_sched_features();
			break;
		case 'v':
                        /* the user wants feedback for each process */
			mode |= MODE_PRINT;
                        break;
		case 'V':
		case 'h':
			usage();
                        return(0);
		default:
			//printf("ERROR: unknown switch\n");
			usage();
			return(1);
		}
	}

	/*
         DAMN FUCKING
	 parameter checking
         ARGH!
	 */
	/* for _BATCH and _NORMAL, prio is ignored and must be 0*/
	if((policy==SCHED_NORMAL || policy==SCHED_BATCH) && prio) {
		decode_error("%s call may fail as static PRIO must be 0 or omitted",
			     TAB[policy]
			    );

	/* _FIFO and _RR MUST have prio set */
	} else if((policy==SCHED_FIFO || policy==SCHED_RR || policy==SCHED_ISO)) {

#define CHECK_RANGE_PRIO(p, p_low, p_high) (p <= (p_high) && p >= (p_low))
		/* FIFO and RR - check min/max priority */
		int prio_min, prio_max;

		get_prio_min_max(policy, &prio_min, &prio_max);
		//int prio_max=sched_get_priority_max(policy);
		//int prio_min=sched_get_priority_min(policy);

		if(! CHECK_RANGE_PRIO(prio, prio_min, prio_max)) {
			// this could be problematic on very special custom kernels
			if(prio == 0) {
				decode_error("missing priority; specify static priority via -p");
			} else {
				decode_error("PRIO %d is out of range %d-%d for %s",
					     prio,
					     prio_min,
					     prio_max,
					     TAB[policy]
					    );
			}
			/* treat as all calls have failed */
			return(ac-optind);
		}
#undef CHECK_RANGE_PRIO
	}

	/* no mode -> do querying */
	if(! mode) {
		mode |= MODE_PRINT;
	}

	if( mode_set(mode, MODE_EXEC) && ! ( mode_set(mode, MODE_SETPOLICY)
					     || mode_set(mode, MODE_AFFINITY)
                                             || mode_set(mode, MODE_NICE) )

	  ) {
		/* we have nothing to do */
		decode_error("Option -e needs scheduling-parameters, not given - exiting");
                return(-1);
	}

	if(! CHECK_RANGE_NICE(nice)) {
		decode_error("NICE %d is out of range -20 to 20", nice);
                return(-1);
	}
#undef CHECK_RANGE_NICE

	/* and: ignition */
	{
                struct engine_s stuff;
		/* now fill struct */
		stuff.mode=mode;
		stuff.policy=policy;
		stuff.prio=prio;
		stuff.aff_mask=aff_mask;
		stuff.nice=nice;

                /* we have this much real args/PIDs to process */
		stuff.n=ac-optind;

                /* this is the first real arg */
		stuff.args=dc+optind;

		/* now go on and do what we were told */
		return(engine(&stuff));
	}
}


int engine(struct engine_s *e)
{
	int ret=0;
	int i;

#ifdef DEBUG
	printf("Dumping mode: 0x%x\n", e->mode);
	printf("Dumping affinity: 0x%x\n", e->aff_mask);
	printf("We have %d args to do\n", e->n);
	for(i=0;i < e->n; i++) {
		printf("Dump arg %d: %s\n", i, e->args[i]);
	}
#endif

	/*
	 handle normal query/set operation:
	 set/query all given PIDs
	 */
	for(i=0; i < e->n; i++) {

		int pid, tmpret=0;

                /* if in MODE_EXEC skip check for PIDs */
		if(mode_set(e->mode, MODE_EXEC)) {
			pid=getpid();
			goto exec_mode_special;
		}

		if(! (isdigit( *(e->args[i])) ) ) {
			decode_error("Ignoring arg %s: is not a PID", e->args[i]);
			continue;
		}

		pid=atoi(e->args[i]);

	exec_mode_special:
		if(mode_set(e->mode, MODE_SETPOLICY)) {
			/*
			 accumulate possible errors
			 the return value of main will indicate
			 how much set-calls went wrong
                         set_process returns -1 upon failure
			 */
			tmpret=set_process(pid,e->policy,e->prio);
			ret += tmpret;

                        /* don't proceed as something went wrong already */
			if(tmpret) {
				continue;
			}

		}

		if(mode_set(e->mode, MODE_NICE)) {
			tmpret=set_niceness(pid, e->nice);
                        ret += tmpret;

			if(tmpret) {
				continue;
			}

		}

#ifdef HAVE_AFFINITY
		if(mode_set(e->mode, MODE_AFFINITY)) {
			tmpret=set_affinity(pid, &(e->aff_mask));
			ret += tmpret;

			if(tmpret) {
				continue;
			}

		}
#endif

		/* and print process info when set, too */
		if(mode_set(e->mode, MODE_PRINT)) {
			print_process(pid);
		}


		/* EXECUTE: at the end */
		if(mode_set(e->mode, MODE_EXEC)) {

			char **new_argv=e->args;

			ret=execvp(*new_argv, new_argv);

			/* only reached on error */
			decode_error("schedtool: Could not exec %s", *new_argv);
			return(ret);
		}
	}
	/*
	 indicate how many errors we got; as ret is accumulated negative,
	 convert to positive
	 */
	return(abs(ret));
}


int set_process(pid_t pid, int policy, int prio)
{
	struct sched_param p;
	int ret;

	char *msg1="could not set PID %d to %s";
	char *msg2="could not set PID %d to raw policy #%d";

	p.sched_priority=prio;

	/* anything other than 0 indicates error */
	if((ret=sched_setscheduler(pid, policy, &p))) {

                /* la la pointer mismatch .. lala */
		decode_error((CHECK_RANGE_POLICY(policy) ? msg1 : msg2),
			     pid,
			     (CHECK_RANGE_POLICY(policy) ? TAB[policy] : policy)
			    );
		return(ret);
	}
	return(0);
}


#ifdef HAVE_AFFINITY
/* mhm - we need something clever for all that CPU_SET() and CPU_ISSET() stuff */
int parse_affinity(cpu_set_t *mask, char *arg)
{
	cpu_set_t tmp_aff;
	char *tmp_arg;
        size_t valid_len;

	if(*arg == '0' && *(arg+1) == 'x') {
		/* we're in standard hex mode */
                /* FIXME TODO taskset code */

	} else if( (valid_len=strspn(arg, "0123456789,.")) ) {
		/* new list mode: schedtool -a 0,2 -> run on CPU0 and CPU2 */

		/* split on ',' and '.', because '.' is near ',' :) */
		while((tmp_arg=strsep(&arg, ",."))) {
                        int tmp_cpu;

			if(isdigit((int)*tmp_arg)) {
				tmp_cpu=atoi(tmp_arg);
                                CPU_SET(tmp_cpu, &tmp_aff);
#ifdef DEBUG
				printf("tmp_arg: %s -> tmp_cpu: %d\n", tmp_arg, tmp_cpu);
#endif
			}
		}

	} else {
		decode_error("affinity %s is not parseable", arg);
		exit(1);
	}

        *mask=tmp_aff;
	return 0;
}


int set_affinity(pid_t pid, cpu_set_t *mask)
{
	int ret;

	if((ret=sched_setaffinity(pid, sizeof(*mask), mask))) {
		decode_error("could not set PID %d to affinity 0x%x",
			     pid,
                             mask
			    );
		return(ret);
	}
        return(0);
}
#endif


int set_niceness(pid_t pid, int nice)
{
	int ret;

	if((ret=setpriority(PRIO_PROCESS, pid, nice))) {
		decode_error("could not set PID %d to nice %d",
			     pid,
			     nice
			    );
                return(ret);
	}
        return(0);
}


/*
 probe some features; just basic right now
 */
void probe_sched_features()
{
	int i;

	for(i=SCHED_MIN; i <= SCHED_MAX; i++) {
		print_prio_min_max(i);
 	}
}


/*
 get the min/max static priorites of a given policy. Max only work
 for SCHED_FIFO / SCHED_RR
 */
void get_prio_min_max(int policy, int *min, int *max)
{
	*min=sched_get_priority_min(policy);
        *max=sched_get_priority_max(policy);
}


/* print the min and max priority of a given policy, like chrt does */
void print_prio_min_max(int policy)
{
	int min, max;

	get_prio_min_max(policy, &min, &max);

	switch(min|max) {

	case -1:
		printf("%-17s: policy not implemented\n", TAB[policy]);
                break;
	default:
		printf("%-17s: prio_min %d, prio_max %d\n", TAB[policy], min, max);
                break;
	}
}


/*
 Be more careful with at least the affinity call; someone may use an
 affinity-compiled version on a non-affinity kernel.
 This is getting more and more fu-gly.
 */
void print_process(pid_t pid)
{
	int policy, nice;
	struct sched_param p;
        unsigned long aff_mask=(0-1);

	/* strict error checking not needed - it works or not. */
	if( ((policy=sched_getscheduler(pid)) < 0)
	    || (sched_getparam(pid, &p) < 0)
	    /* getpriority may successfully return negative values, so errno needs to be checked */
	    || ((nice=getpriority(PRIO_PROCESS, pid)) && errno)
	  ) {
		decode_error("could not get scheduling-information for PID %d", pid);

	} else {

		/* do custom output for unknown policy */
		if(! CHECK_RANGE_POLICY(policy)) {
			printf("PID %5d: PRIO %3d, POLICY %-5d <UNKNOWN>, NICE %3d",
			       pid,
			       p.sched_priority,
			       policy,
			       nice
			      );
		} else {

			printf("PID %5d: PRIO %3d, POLICY %-17s, NICE %3d",
			       pid,
			       p.sched_priority,
			       TAB[policy],
			       nice
			      );
		}

#ifdef HAVE_AFFINITY
		/*
		 sched_getaffinity() seems to also return (int)4 on 2.6.8+ on x86 when successful.
		 this goes against the documentation
                 */
		if(sched_getaffinity(pid, sizeof(aff_mask), &aff_mask) == -1) {
			/*
			 error or -ENOSYS
                         simply ignore and reset errno!
			 */
                        errno=0;
		} else {
			printf(", AFFINITY 0x%lx", aff_mask);
		}
#endif
	printf("\n");
	}
}


void usage(void)
{
	printf(
	       "get/set scheduling policies - v" VERSION ", GPL'd, NO WARRANTY\n" \
	       "USAGE: schedtool PIDS                    - query PIDS\n" \
	       "       schedtool [OPTIONS] PIDS          - set PIDS\n" \
	       "       schedtool [OPTIONS] -e COMMAND    - exec COMMAND\n" \
               "\n" \
	       "set scheduling policies:\n" \
	       "    -N                    for SCHED_NORMAL\n" \
	       "    -F -p PRIO            for SCHED_FIFO       only as root\n" \
	       "    -R -p PRIO            for SCHED_RR         only as root\n" \
	       "    -B                    for SCHED_BATCH\n" \
	       "    -I -p PRIO            for SCHED_ISO\n" \
	       "    -D                    for SCHED_IDLEPRIO\n" \
               "\n" \
               "    -M POLICY             for manual mode; raw number for POLICY\n" \
	       "    -p STATIC_PRIORITY    usually 1-99; only for FIFO or RR\n" \
	       "                          higher numbers means higher priority\n" \
	       "    -n NICE_LEVEL         set niceness to NICE_LEVEL\n" \
	      );
#ifdef HAVE_AFFINITY
	printf("    -a AFFINITY_MASK      set CPU-affinity to bitmask or list\n\n");
#endif
	printf(
	       "    -e COMMAND [ARGS]     start COMMAND with specified policy/priority\n" \
	       "    -r                    display priority min/max for each policy\n" \
	       "    -v                    be verbose\n" \
	       "\n" \
	      );
/*	printf("Parent ");
	print_process(getppid()); */

}
