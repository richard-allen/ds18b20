/* Small program that locates all DS18B20 probes in the system
 * and opens a logfile for each probe and writes the temperature
 * from each probe to it's logfile.
 *
 * Arguments:
 * -v = Verbose.  Print additional info on what the program is doing.
 * -i = Interval. The interval the probes are checked in seconds.
 * -l = Log to text files
 * -r = Maintain RRD files
 * -o = Run once. Print temperatures to screen and exit
 * -f = Foreground.  Do not daemonize and run in the foreground.
 *
 *  Additionally these arguments can also be set in the config file.
 *
 *  RRD support is still pending.
 *
 * Written by Richard Allen <ra@ra.is> and released under the GPLv2 license.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <signal.h>
#include <syslog.h>
#include <time.h>

#define PROBEPATH    "/sys/bus/w1/devices"	// Location of the probes
#define LOGROOT      "/var/probes"		// Location of the logs
#define RRDROOT      "/var/www/rrd"		// Location of the rrd files
#define CFGFILE      "/etc/ds18b20.cfg"		// Default config file
#define MAXPROBES    5				// Max number of probes
#define PROBENAMELEN 80				// Max length of the probenames including directory.
#define BUFSIZE      256			// Typical charbuffer

// Globals.  (Yes, I know)...
int verbose, numprobes, foreground, interval, dolog, dorrd;
char probepath[MAXPROBES][PROBENAMELEN];
char probename[MAXPROBES][PROBENAMELEN];
char logpath[MAXPROBES][PROBENAMELEN];
char alias[MAXPROBES][BUFSIZE];
FILE *probefd[MAXPROBES];
FILE *logfd[MAXPROBES];

/* An extreemly simple config file parser */
void ParseCfg(char *cfgfilename) {
	FILE *cfgfile;
	char *key, *value1, *value2;
	char cfgline[BUFSIZE];
	int i, line, numalias;

	memset(cfgline, 0, sizeof(cfgline));
	cfgfile = NULL;
	key = value1 = value2 = NULL;
	line = numalias = 0;

	if ((cfgfile = fopen (cfgfilename, "r")) == NULL) {
		printf("Could not open config file: '%s'\n", cfgfilename);
		exit(EXIT_FAILURE);
	}

	/* loop thru the file */
	while (fgets(cfgline, sizeof(cfgline)-1, cfgfile) != NULL) {
		line++;
		if ((cfgline[0] == '#') || (cfgline[0] == '\n') || (cfgline[0] == ';')) {
			memset(cfgline, 0, sizeof(cfgline));
			continue;
		}
		for (i=0; cfgline[i] != 0; i++) {
			if (cfgline[i] == '#' || cfgline[i] == ';') {
				cfgline[i] = '\0';
			}
		}

		key = strtok(cfgline, " \t");
		if (!key) {
			printf("Syntax error in line %d in file %s\n", line, cfgfilename);
			exit(EXIT_FAILURE);
		}

		if ((strncasecmp(key, "VERBOSE", sizeof(cfgline)-1)) == 0) {
			verbose = 1;
		} else if ((strncasecmp(key, "LOG", sizeof(cfgline)-1)) == 0) {
			dolog = 1;
		} else if ((strncasecmp(key, "RRD", sizeof(cfgline)-1)) == 0) {
			dorrd = 1;
		} else if ((strncasecmp(key, "FOREGROUND", sizeof(cfgline)-1)) == 0) {
			foreground = 1;
		} else if ((strncasecmp(key, "INTERVAL", sizeof(cfgline)-1)) == 0) {
			value1 = strtok(NULL, "\n");
			if (!value1) {
				printf("Syntax error in line %d in file %s\n", line, cfgfilename);
				exit(EXIT_FAILURE);
			}
			interval = atoi(value1);
			if (interval == 0) {
				// atoi is crap. Need to make sure interval is real
				fprintf(stderr, "Interval can not be zero\n");
				exit(EXIT_FAILURE);
			}
		} else if ((strncasecmp(key, "ALIAS", sizeof(cfgline)-1)) == 0) {
			value1 = strtok(NULL, " ");
			value2 = strtok(NULL, "\n");
			if (!value1 || !value2) {
				printf("Syntax error in line %d in file %s\n", line, cfgfilename);
				exit(EXIT_FAILURE);
			}
			snprintf(alias[numalias], BUFSIZE-1, "%s %s", value1, value2);
			numalias++;
		} else {
			printf("Unknown key '%s' on line %d in file %s\n", key, line, cfgfilename);
			exit(EXIT_FAILURE);
		}
		memset(cfgline, 0, sizeof(cfgline));
		key = value1 = value2 = NULL;
	}
	fclose(cfgfile);
}

// Locate all probes
int findprobes(void) {
	struct dirent *pDirent;
	DIR *pDir;
	char *realname, *aliasname;
	int i, count, foundit;
	char aliascopy[BUFSIZE];

	count = foundit = 0;
	pDir = opendir(PROBEPATH);
	if (pDir == NULL) {
		fprintf(stderr, "Cannot open directory '%s'\n", PROBEPATH);
		return 0;
	}
	while ((pDirent = readdir(pDir)) != NULL) {
		// All DS18B20 have filenames starting with 28-
		if (pDirent->d_name[0] == '2' && pDirent->d_name[1] == '8' && pDirent->d_name[2] == '-') {
			snprintf(probepath[count], PROBENAMELEN-1, "%s/%s/w1_slave", PROBEPATH, pDirent->d_name);
			snprintf(probename[count], PROBENAMELEN-1, "%s", pDirent->d_name);
			for (i=0; alias[i][0] != '\0'; i++) {
				strncpy (aliascopy, alias[i], BUFSIZE-1);
				realname = strtok(aliascopy, " ");
				aliasname = strtok(NULL, "\n");
				if (strncasecmp(probename[count], realname, PROBENAMELEN-1) == 0) {
					snprintf(logpath[count], PROBENAMELEN-1, "%s/%s.log", LOGROOT, aliasname);
					snprintf(probename[count], PROBENAMELEN-1, "%s", aliasname);
					foundit++;
				}
			}
			if (foundit == 0) {
				snprintf(logpath[count], PROBENAMELEN-1, "%s/%s.log", LOGROOT, pDirent->d_name);
			}
			if (verbose) printf ("Found DS18B20 compatible probe named '%s':\n\tDevice file '%s'\n\tLogfile '%s'\n",
				probename[count], probepath[count], logpath[count]);
			count++;
		}
		foundit = 0;
	}
	closedir(pDir);
	return count;
}

// Open the probe device files and logs
int openlogs() {
	int c, count;

	count = 0;
	for (c = 0; c < numprobes; c++) {
		logfd[c] = fopen (logpath[c], "a");
		if (logfd[c] == NULL) {
			fprintf(stderr, "Error: Unable to open '%s': %s\n", logpath[c], strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (verbose) fprintf(stderr, "Successfully opened '%s' for writing\n", logpath[c]);
		count++;
	}
	return count;
}

// Close the logs.
int closelogs() {
	int c, count;

	count = 0;
	for (c = 0; c < numprobes; c++) {
		if ((fclose (logfd[c])) != 0) {
			fprintf(stderr, "Error: Unable to close '%s': %s\n", logpath[c], strerror(errno));
			exit(EXIT_FAILURE);
		}
		count++;
	}
	return count;
}

void handle_sighup(int signum) {
	closelogs(numprobes);
	openlogs(numprobes);
}

int update_rrd(int probe, float temp) {
	// No dragons here
	return 0;
}

int main(int argc, char **argv) {
	int c, once;
	double temperature;
	char *myname, *bmyname, *temp;
	pid_t mypid, sid;
	time_t now;
	struct stat fileStat;
	FILE *pidfile;
	char buf[BUFSIZE];

	foreground = verbose = numprobes = dolog = dorrd = once = 0;
	interval = 300;		// Default interval is 300 seconds (5 minutes)

	ParseCfg(CFGFILE);	// Parse CFG now, allowing cmdline to override

	while ((c = getopt (argc, argv, "fvlroi:")) != -1) {
		switch (c) {
			case 'f':
				foreground = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'l':
				dolog = 1;
				break;
			case 'r':
				dorrd = 1;
				break;
			case 'o':
				once = 1;
				break;
			case 'i':
				interval = atoi(optarg);
				if (interval == 0) {
					// atoi is crap. Need to make sure interval is real
					fprintf(stderr, "Interval can not be zero\n");
					exit(EXIT_FAILURE);
				}
				break;
			default:
				printf("usage: %s [ -v ] [ -f ] [ -o ] [ -i interval ] [ -l | -r ]\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	if ((!dolog && !dorrd) && !once) {
		fprintf(stderr, "Please use either logging (-l) or rrd (-r) output\n");
		exit(EXIT_FAILURE);
	}

	if (once) foreground = 1;

// Locate the probes.
	numprobes = findprobes();
	if (numprobes == 0) {
		fprintf(stderr, "Error: No DS18B20 compatible probes located.\n");
		fprintf(stderr, "Please make sure the w1-gpio and w1-therm drivers.\n");
		fprintf(stderr, "are loaded into the kernel.\n");
		exit(EXIT_FAILURE);
	}

// Lets see if our logdir exists and is a directory
	if (!foreground) {
		if (stat(LOGROOT, &fileStat) < 0) {
			fprintf(stderr, "Error: %s does not exist.\n", LOGROOT);
			exit(EXIT_FAILURE);
		}
		if (!S_ISDIR(fileStat.st_mode)) {
			fprintf(stderr, "Error: %s is not a directory.\n", LOGROOT);
			exit(EXIT_FAILURE);
		}
		if (verbose) fprintf(stderr, "Log folder '%s' is OK.\n", LOGROOT);

// Now to open all the logfiles
		if ((openlogs()) != numprobes) {
			fprintf(stderr, "Something went wrong opening files.\n");
			exit(EXIT_FAILURE);
		}
	}
// All looks good. Time to prep the main loop.
	if (foreground) {
		fprintf(stderr, "Running in the foreground, not daemonizing.\n");
	} else {
		mypid = fork();
		if (mypid < 0)
			exit(EXIT_FAILURE); // Could not fork.
		if (mypid > 0)
			exit(EXIT_SUCCESS); // Parent can exit here
		// New child process from here.
		sid = setsid();
		if (sid < 0)
			exit(EXIT_FAILURE);
		if ((chdir(LOGROOT)) < 0)
			exit(EXIT_FAILURE);
		// Close the standard file descriptors
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}

	if (!foreground) {
		mypid = getpid();	// Get my pid
		myname = strdup(argv[0]);
		bmyname = basename(myname);
		snprintf(buf, sizeof(buf)-1, "/var/run/%s.pid", bmyname);
		if ((pidfile = fopen(buf, "w")) == NULL) {
			fprintf(stderr, "Unable to open pidfile '%s'. '%s'\n", buf, strerror(errno));
			exit(EXIT_FAILURE);
		}
		fprintf(pidfile, "%d\n", mypid);
		fflush(pidfile);
		fclose(pidfile);
		openlog(bmyname, LOG_PID|LOG_CONS, LOG_USER);
	}

// SIGHUP makes us close and reopen logs. This is probably a FIXME as signal() is really
// not portable at all and can even behave differently on some Linux systems.
	signal(SIGHUP, handle_sighup);

// Main loop
	while(1) {
		// rewind the logs and read them
		for (c = 0; c < numprobes; c++) {
			probefd[c] = fopen(probepath[c], "r");
			if (probefd[c] == NULL) {
				if (foreground) {
					fprintf(stderr, "Error: Unable to open '%s': %s\n",
						probepath[c], strerror(errno));
				} else {
					syslog(LOG_ERR, "Error: Unable to open '%s': %s\n",
						probepath[c], strerror(errno));
				}
				exit(EXIT_FAILURE);
			}
/*
 * Example output from a probe. It's two lines and we want the t= part:
 * 5c 01 4b 46 7f ff 04 10 a1 : crc=a1 YES
 * 5c 01 4b 46 7f ff 04 10 a1 t=21750
 */
			fgets(buf, sizeof(buf)-1, probefd[c]);	// we always want line 2
			memset(buf, 0, sizeof(buf));		// so we ignore the first read
			fgets(buf, sizeof(buf)-1, probefd[c]);
			temp = strtok(buf, "t=");		// C stringparsing rules! :)
			temp = strtok(NULL, "t=");		// See?  Easy as pie.
			temperature = atof(temp)/1000;		// change "21750" into "21.750"
			if (dolog && !once) {
				now = time(NULL);
				fprintf(logfd[c], "%u %2.3f\n", (unsigned int) now, temperature);
				fflush(logfd[c]);			// Probably unnecessary to flush.
			}
			if (once) {
				fprintf(stdout, "%s:\t%2.3f\n", probename[c], temperature);
			}
			fclose(probefd[c]);
		}
		if (once) exit(EXIT_SUCCESS);
		sleep(interval);
	}
	exit(EXIT_SUCCESS);	// Never reached. Makes gcc -Wall shut up.
}
