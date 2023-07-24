/*
 * share.c
 *
 * CLI utility to manipulate share DB
 *
 * ShareDB data format (BTREE):
 *   key  = mountpoint (without trailing NUL characters)
 *   data = mount options (per /etc/exports), multi-mountpoint options separated by NUL
 *
 * Copyright (c) 2023, Peter Eriksson <pen@lysator.liu.se>
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <db.h>
#include <fcntl.h>
#include <limits.h>


char *version = "1.1 (2023-07-24)";
char *path_sharedb = "/etc/zfs/exports.db";
char *path_pidfile = "/var/run/mountd.pid";

int f_debug = 0;
int f_print = 0;
int f_signal = 0;
int f_add = 0;
int f_create = 0;
int f_truncate = 0;
int f_remove = 0;
int f_locked = 1;
int f_verbose = 0;


void
signal_mountd(void) {
	FILE *fp;
	int pid;

	if (!path_pidfile)
		return;
  
	fp = fopen(path_pidfile, "r");
	if (!fp)
		return;
  
	if (fscanf(fp, "%u", &pid) == 1) 
		kill(pid, SIGHUP);
  
	fclose(fp);
}


ssize_t 
scan_dbt(DBT *d,
	 size_t size,
	 char *opts) {
	char *buf = d->data;
	size_t p = 0;

	while (p < d->size) {
		int len = strnlen(buf+p, d->size-p);

		if (len == size && memcmp(opts, buf+p, len) == 0)
			return p;

		p += len;

		if (p < d->size && buf[p] == '\0')
			++p;
	}

	return -1;
}

void
print_dbt(DBT *d) {
	char *buf = d->data;
	size_t p = 0;
	int i = 0;

	while (p < d->size) {
		int len = strnlen(buf+p, d->size-p);

		printf("%2d : %.*s\n", i++, len, buf+p);
		p += len; 
   
		if (p < d->size && buf[p] == '\0')
			++p;
	}
}


int
argv2dbt(int argc,
	 char **argv,
	 DBT *d) {
	char *buf;
	size_t len, newsize;
	int i, n;


	n = 0;
	len = 0;
	for (i = 0; i < argc; i++) {
		char *ap = argv[i];
		int alen;

		/* Drop leading space */
		while (isspace(*ap))
			++ap;

		/* Drop trailing space */
		alen = strlen(ap);
		while (len > 0 && isspace(ap[alen-1]))
			--alen;

		if (!alen)
			continue;

		if (scan_dbt(d, alen, ap) >= 0) {
			printf("%s: Already in DBT\n", ap);
			continue;
		}
    
		if (d->data == NULL) {
			newsize = alen;
			d->data = malloc(newsize);
			buf = (char *) d->data;
		} else {
			newsize = d->size+1+alen;
			d->data = realloc(d->data, newsize);
			buf = (char *) d->data + d->size;
			*buf++ = '\0';
		}
    
		d->size = newsize;
		strncpy(buf, ap, alen);
		buf += alen;
	}
  
	return n;
}

void
print_data(size_t size,
	   void *data) {
	unsigned char *buf = (unsigned char *) data;
	size_t pos = 0;
	int i;

	while (pos < size) {
		for (i = 0; i < 16 && pos+i < size; i++) {
			if (i > 0)
				putchar(' ');
			putchar(isprint(buf[pos+i]) ? buf[pos+i] : '?');
		}
		for (; i < 16; i++) {
			putchar(' ');
			putchar(' ');
		}
		putchar(' ');
		putchar(' ');
		putchar(' ');
		putchar(' ');
		for (i = 0; i < 16 && pos+i < size; i++) {
			if (i > 0)
				putchar(' ');
			printf("%02x", buf[pos+i]);
		}
		pos += i;
		for (; i < 16; i++) {
			printf("   ");
		}
		putchar('\n');
	}
}

int
main(int argc,
     char *argv[]) {
	DB *db;
	DBT k, d;
	int i, j, rc;
	char *buf;
	int f_mode = O_RDONLY;
	int nm = 0;
  

	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		for (j = 1; argv[i][j]; j++) {
			switch (argv[i][j]) {
			case 'h':
				printf("Usage:\n\t%s [<options>] [<mountpoint> <flags> [... <flags>]]\n\n",
				       argv[0]);
				puts("Options:");
				puts("\t-h             Display this usage information");
				puts("\t-v             Increase verbosity level");
				puts("\t-d             Increase debugging level");
				puts("\t-p             Print DB entries");
				puts("\t-c             Create ShareDB file if not existing");
				puts("\t-z             Truncate ShareDB file to zero size");
				puts("\t-a             Append share options to share");
				puts("\t-r             Remove share / share options");
				puts("\t-u             Unlocked database access (unsafe!)");
				puts("\t-s             Send signal to mountd");
				printf("\t-D <path>      ShareDB path [%s]\n", path_sharedb);
				printf("\t-P <path>      Mountd pidfile path [%s]\n", path_pidfile);
				exit(0);

			case 'd':
				f_debug++;
				break;

			case 'u':
				f_locked = 0;
				break;

			case 'v':
				f_verbose++;
				break;
				
			case 'p':
				f_print++;
				break;

			case 'c':
				f_create++;
				break;
	
			case 'z':
				f_truncate++;
				break;
	
			case 'a':
				f_add++;
				break;
	
			case 'r':
				f_remove++;
				break;
	
			case 's':
				f_signal++;
				break;
	
			case 'D':
				if (argv[i][j+1])
					path_sharedb = strdup(argv[i]+j+1);
				else if (i+1 < argc && argv[i+1][0])
					path_sharedb = strdup(argv[++i]);
				else {
					fprintf(stderr, "%s: Error: Missing argument to -D\n",
						argv[0]);
					exit(1);
				}
				goto NextArg;
	
			case 'P':
				if (argv[i][j+1])
					path_pidfile = strdup(argv[i]+j+1);
				else if (i+1 < argc && argv[i+1][0])
					path_pidfile = strdup(argv[++i]);
				else {
					fprintf(stderr, "%s: Error: Missing argument to -P\n",
						argv[0]);
					exit(1);
				}
				goto NextArg;
	
			default:
				fprintf(stderr, "%s: Error: -%c: Invalid switch\n",
					argv[0], argv[i][j]);
				exit(1);
			}
		}
	NextArg:;
	}

	if (f_verbose)
		fprintf(stderr, "[share, version %s]\n", version);
	
	if (f_create || f_add || f_remove || i+1 < argc) {
		f_mode = O_RDWR;
		if (f_locked)
			f_mode |= O_EXLOCK;
	} else if (f_locked)
		f_mode |= O_SHLOCK;
	
	if (f_create)
		f_mode |= O_CREAT;
	if (f_truncate)
		f_mode |= O_TRUNC;
		
	while ((db = dbopen(path_sharedb, f_mode|O_NONBLOCK, 0600, DB_BTREE, NULL)) == NULL &&
	       errno == EWOULDBLOCK) {
		fprintf(stderr, "%s: Notice: %s: Database locked, retrying in 1s\n",
			argv[0], path_sharedb);
		sleep(1);
	}
	if (!db) {
		fprintf(stderr, "%s: Error: %s: Unable to open: %s\n",
			argv[0], path_sharedb, strerror(errno));
		exit(1);
	}
  
	if (i < argc) {
		k.size = strlen(argv[i]);
		k.data = argv[i];
    
		d.size = 0;
		d.data = NULL;
    
		if (f_remove) {
			if (i+1 == argc) {
				/* Delete the path */
				rc = db->del(db, &k, 0);
				if (rc != 0) {
					fprintf(stderr, "%s: Error: %s: %s: DB delete failed: %s\n",
						argv[0], path_sharedb, argv[i], strerror(errno));
					exit(1);
				}
			} else {
				/* Delete matching options */
				fprintf(stderr, "%s: Error: Deleting matching options not yet implemented\n",
					argv[0]);
				exit(1);
			}
		} else {
			if (f_add) {
				/* Try to get existing entry, ignore if it doesn't exist */
				if (db->get(db, &k, &d, 0) < 0) {
					fprintf(stderr, "%s: Error: %s: DB lookup failed: %s\n",
						argv[0], path_sharedb, strerror(errno));
					exit(1);
				}
				if (d.size > 0) {
					buf = malloc(d.size);
					if (!buf) {
						fprintf(stderr, "%s: Error: %lu: malloc: %s\n",
							argv[0], d.size, strerror(errno));
						exit(1);
					}
					memcpy(buf, d.data, d.size);
					d.data = buf;
				}
	
				if (argv2dbt(argc-i-1, argv+i+1, &d) < 0) {
					fprintf(stderr, "%s: Error: %s [%s]: Unable to extract options: %s\n",
						argv[0], argv[i], argv[i+1], strerror(errno));
					exit(1);
				}
	
				rc = db->put(db, &k, &d, 0);
				if (rc != 0) {
					fprintf(stderr, "%s: Error: %s: %s: DB update failed: %s\n",
						argv[0], path_sharedb, argv[i], strerror(errno));
					exit(1);
				}
			} else {
				/* Replace */
				if (argv2dbt(argc-i-1, argv+i+1, &d) < 0) {
					fprintf(stderr, "%s: Error: %s [%s]: Unable to extract options: %s\n",
						argv[0], argv[i], argv[i+1], strerror(errno));
					exit(1);
				}
	
				rc = db->put(db, &k, &d, 0);
				if (rc != 0) {
					fprintf(stderr, "%s: Error: %s: %s: DB update failed: %s\n",
						argv[0], path_sharedb, argv[i], strerror(errno));
					exit(1);
				}
			}
		}
    
		if (d.data)
			free(d.data);
	}
    
	if (f_signal)
		signal_mountd();
  
	if (f_print || f_verbose) {
		rc = db->seq(db, &k, &d, R_FIRST);
		nm = 0;
		while (rc == 0) {
			unsigned int p = 0;
			++nm;

			if (f_print) {
				for (p = 0; p < d.size; p++) {
					char *buf = (char *) d.data;
					int len = strnlen(buf+p, d.size-p);
					
					printf("%.*s\t%.*s\n", (int) k.size, (char *) k.data, len, buf+p);
					p += len;
				}
				if (f_debug) {
					print_data(k.size, k.data);
					putchar('\n');
					print_data(d.size, d.data);
					putchar('\n');
				}
			}
      
			rc = db->seq(db, &k, &d, R_NEXT);
		}

	}
	
	if (f_verbose)
		fprintf(stderr, "[%s: %d exported mountpoints]\n", path_sharedb, nm);

	db->close(db);
	exit(0);
}
