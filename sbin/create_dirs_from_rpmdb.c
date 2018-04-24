/* create_dirs_from_rpmdb - Create missing directories in /srv,/var during boot

   Copyright (C) 2018 SUSE Linux GmbH
   Author: Thorsten Kukuk <kukuk@suse.de>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>

static int debug_flag = 0;
static int verbose_flag = 0;

/* Print the version information.  */
static void
print_version (void)
{
  fprintf (stdout, "create_dirs_from_rpmdb (%s) %s\n", PACKAGE, VERSION);
}

static void
print_usage (FILE *stream)
{
  fprintf (stream, "Usage: create_dirs_from_rpmdb [-V|--version] [--debug] [-v|--verbose]\n");
}

static void
print_error (void)
{
  fprintf (stderr,
           "Try `create_dirs_from_rpmdb --help' or `create_dirs_from_rpmdb --usage' for more information.\n");
}


static char *
fmode2str (int mode)
{
  char *str = strdup("----------");

  if (str == NULL)
    {
      fprintf (stderr, "Out of memory\n");
      exit (1);
    }

  if (S_ISREG(mode))
    str[0] = '-';
  else if (S_ISDIR(mode))
    str[0] = 'd';
  else if (S_ISCHR(mode))
    str[0] = 'c';
  else if (S_ISBLK(mode))
    str[0] = 'b';
  else if (S_ISFIFO(mode))
    str[0] = 'p';
  else if (S_ISLNK(mode))
    str[0] = 'l';
  else if (S_ISSOCK(mode))
    str[0] = 's';
  else
    str[0] = '?';

  if (mode & S_IRUSR) str[1] = 'r';
  if (mode & S_IWUSR) str[2] = 'w';
  if (mode & S_IXUSR) str[3] = 'x';

  if (mode & S_IRGRP) str[4] = 'r';
  if (mode & S_IWGRP) str[5] = 'w';
  if (mode & S_IXGRP) str[6] = 'x';

  if (mode & S_IROTH) str[7] = 'r';
  if (mode & S_IWOTH) str[8] = 'w';
  if (mode & S_IXOTH) str[9] = 'x';

  if (mode & S_ISUID)
    str[3] = ((mode & S_IXUSR) ? 's' : 'S');

  if (mode & S_ISGID)
    str[6] = ((mode & S_IXGRP) ? 's' : 'S');

  if (mode & S_ISVTX)
    str[9] = ((mode & S_IXOTH) ? 't' : 'T');

  return str;
}

int
check_package (rpmts ts, Header h)
{
  int ec = 0;
  rpmfi fi = NULL;
  rpmfiFlags fiflags =  (RPMFI_NOHEADER | RPMFI_FLAGS_QUERY);

  fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, fiflags);
  if (rpmfiFC(fi) <= 0)
    goto exit;

  fi = rpmfiInit (fi, 0);
  while (rpmfiNext (fi) >= 0)
    {
      rpm_mode_t fmode = rpmfiFMode(fi);

      if (S_ISDIR(fmode))
	{
	  const char *prefixes[] = {"/var/", "/srv/"};
	  const char *fn = rpmfiFN(fi);
	  rpmfileAttrs fflags = rpmfiFFlags(fi);
	  int i;

	  for (i = 0; i < sizeof (prefixes)/sizeof(char *); i++)
	    {
	      if (!(fflags & RPMFILE_GHOST) &&
		  strncmp (prefixes[i], fn, strlen (prefixes[i]))== 0 &&
		  access (fn, F_OK) == -1)
		{
		  int rc = 0;
		  struct tm * tm;
		  char timefield[100];
		  rpm_time_t fmtime = rpmfiFMtime(fi);
		  time_t mtime = fmtime;  /* important if sizeof(int32_t) ! sizeof(time_t) */
		  const char *fuser = rpmfiFUser(fi);
		  const char *fgroup = rpmfiFGroup(fi);
		  uid_t user_id;
		  gid_t group_id;
		  struct passwd *pwd;
		  struct group *grp;
		  struct timeval stamps[2] = {
		    { .tv_sec = mtime, .tv_usec = 0 },
		    { .tv_sec = mtime, .tv_usec = 0 }};


		  if (debug_flag)
		    {
		      char *perms =  fmode2str (fmode);

		      /* Convert file mtime to display format */
		      tm = localtime(&mtime);
		      timefield[0] = '\0';
		      if (tm != NULL)
			{
			  const char *fmt = "%F,%H:%M";
			  (void)strftime(timefield, sizeof(timefield) - 1, fmt, tm);
			}

		      printf ("Create %s (%s,%s,%s,%s)\n",  fn, perms, fuser,
			      fgroup, timefield);
		      free (perms);
		    }
		  else if (verbose_flag)
		    printf ("Create %s\n", fn);

		  rc = mkdir (fn, fmode);
		  if (rc < 0)
		    {
                      fprintf (stderr, "Failed to create directory '%s': %m\n", fn);
		      ec = 1;
		      goto exit;
		    }

		  pwd = getpwnam (fuser);
		  grp = getgrnam (fgroup);

		  if (pwd == NULL || grp == NULL)
		    {
                      fprintf (stderr, "Failed to resolve %s/%s\n", fuser, fgroup);
		      rmdir (fn);
		      ec = 1;
		      goto exit;
		    }

		  user_id = pwd->pw_uid;
		  group_id = grp->gr_gid;

		  rc = chown (fn, user_id, group_id);
		  if (rc < 0)
		    {
                      fprintf (stderr, "Failed to set owner/group for '%s': %m\n", fn);
		      /* wrong permissions are bad, remove dir and continue */
		      rmdir (fn);
		      ec = 1;
		      goto exit;
		    }
		  /* ignore errors here, time stamps are not critical */
		  utimes (fn, stamps);
		}
	    }
	}
    }

 exit:
  rpmfiFree(fi);

  return ec;
}

int
main (int argc, char *argv[])
{
  Header h;
  rpmts ts = NULL;
  int ec = 0;


  while (1)
    {
      int c;
      int option_index = 0;
      static struct option long_options[] = {
        {"version",                   no_argument,       NULL,  'V' },
        {"usage",                     no_argument,       NULL,  'u' },
        {"debug",                     no_argument,       NULL,  254 },
	{"verbose",                  no_argument,       NULL, 'v' },
        {"help",                      no_argument,       NULL,  255 },
        {NULL,                    0,                 NULL,    0 }
      };

      /* Don't let getopt print error messages, we do it ourself. */
      opterr = 0;

      c = getopt_long (argc, argv, "uVv",
		       long_options, &option_index);

      if (c == (-1))
        break;

      switch (c)
        {
        case 'V':
          print_version ();
          return 0;
	case 255:
        case 'u':
          print_usage (stdout);
          return 0;
	case 'v':
	  verbose_flag = 1;
	  break;
	case 254:
	  debug_flag = 1;
	  break;
        default:
	  break;
	}
    }

  argc -= optind;
  argv += optind;

  if (argc > 0)
    {
      fprintf (stderr, "create_dirs_from_rpmdb: Too many arguments.\n");
      print_error ();
      return 1;
    }

  rpmReadConfigFiles (NULL, NULL);

  ts = rpmtsCreate ();
  rpmtsSetRootDir (ts, rpmcliRootDir);

  rpmdbMatchIterator mi = rpmtsInitIterator (ts, RPMDBI_PACKAGES, NULL, 0);
  if (mi == NULL)
    return 1;

  while ((h = rpmdbNextIterator (mi)) != NULL)
    {
      int rc;
      /* rpmsqPoll (); */
      if ((rc = check_package (ts, h)) != 0)
	ec = rc;
    }

  rpmdbFreeIterator (mi);
  rpmtsFree (ts);

  return ec;
}
