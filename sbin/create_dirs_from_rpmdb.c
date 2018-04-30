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
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <malloc.h>
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


/* Quicksort code to sort the directories before creating them. Else we could run into the case,
   that we have to create sub-directories and the parents don't exist yet. */

struct node
{
  const char *dirname;
  rpm_mode_t fmode;
  uid_t user_id;
  gid_t group_id;
  time_t fmtime;
  struct node *next;
};

struct node *dir_list = NULL;

/* A utility function to insert a node at the beginning of linked list */
void
insert_node (struct node** head_ref, const char *dirname, rpm_mode_t fmode,
	     uid_t user_id, gid_t group_id, time_t fmtime)
{
  /* allocate node */
  struct node* new_node = malloc(sizeof(struct node));

  /* put in the data  */
  new_node->dirname  = strdup (dirname);
  new_node->fmode = fmode;
  new_node->user_id = user_id;
  new_node->group_id = group_id;
  new_node->fmtime = fmtime;

  /* link the old list off the new node */
  new_node->next = (*head_ref);

  /* move the head to point to the new node */
  (*head_ref) = new_node;
}

/* Returns the last node of the list */
struct node *
get_last_node (struct node *cur)
{
  while (cur != NULL && cur->next != NULL)
    cur = cur->next;
  return cur;
}

/* Partitions the list taking the last element as the pivot */
struct node *
partition(struct node *head, struct node *end,
	  struct node **newHead, struct node **newEnd)
{
  struct node *pivot = end;
  struct node *prev = NULL, *cur = head, *tail = pivot;

  /* During partition, both the head and end of the list might change
     which is updated in the newHead and newEnd variables */
  while (cur != pivot)
    {
      if (strcmp (cur->dirname, pivot->dirname) < 0)
        {
	  /* First node that has a value less than the pivot - becomes
	     the new head */
	  if ((*newHead) == NULL)
	    (*newHead) = cur;

	  prev = cur;
	  cur = cur->next;
        }
      else  /* If cur node is greater than pivot */
        {
	  /* Move cur node to next of tail, and change tail */
	  if (prev)
	    prev->next = cur->next;
	  struct node *tmp = cur->next;
	  cur->next = NULL;
	  tail->next = cur;
	  tail = cur;
	  cur = tmp;
        }
    }

  /* If the pivot data is the smallest element in the current list,
     pivot becomes the head */
  if ((*newHead) == NULL)
    (*newHead) = pivot;

  (*newEnd) = tail;

  return pivot;
}


struct node *
quicksort_rec(struct node *head, struct node *end)
{
  struct node *newHead = NULL, *newEnd = NULL;
  struct node *pivot;

  if (!head || head == end)
    return head;

  /* Partition the list, newHead and newEnd will be updated
     by the partition function */
  pivot = partition(head, end, &newHead, &newEnd);

  /* If pivot is the smallest element - no need to recur for
     the left part. */
  if (newHead != pivot)
    {
      /* Set the node before the pivot node as NULL */
      struct node *tmp = newHead;
      while (tmp->next != pivot)
	tmp = tmp->next;
      tmp->next = NULL;

      newHead = quicksort_rec(newHead, tmp);

      /* Change next of last node of the left half to pivot */
      tmp = get_last_node(newHead);
      tmp->next =  pivot;
    }

  /* Recur for the list after the pivot element */
  pivot->next = quicksort_rec(pivot->next, newEnd);

  return newHead;
}

/* The main function for quick sort. This is a wrapper over recursive
   function quicksort_rec() */
void
quicksort (struct node **headRef)
{
  (*headRef) = quicksort_rec(*headRef, get_last_node(*headRef));
    return;
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

		      printf ("Missing %s (%s,%s,%s,%s)\n",  fn, perms, fuser,
			      fgroup, timefield);
		      free (perms);
		    }

		  pwd = getpwnam (fuser);
		  grp = getgrnam (fgroup);

		  if (pwd == NULL || grp == NULL)
		    {
	              fprintf (stderr, "Failed to resolve %s/%s\n", 
	                       fuser, fgroup);
		      ec = 1;
		      goto exit;
		    }

		  user_id = pwd->pw_uid;
		  group_id = grp->gr_gid;

		  insert_node (&dir_list, fn, fmode, user_id, group_id, fmtime);
		}
	    }
	}
    }

 exit:
  rpmfiFree(fi);

  return ec;
}

int
create_dirs (struct node *node)
{
  int rc = 0;

  while (node != NULL)
    {
      struct timeval stamps[2] = {
	{ .tv_sec = node->fmtime, .tv_usec = 0 },
	{ .tv_sec = node->fmtime, .tv_usec = 0 }};

      if (verbose_flag)
	printf ("Create %s\n", node->dirname);

      rc = mkdir (node->dirname, node->fmode);
      if (rc < 0)
	{
	  fprintf (stderr, "Failed to create directory '%s': %m\n", node->dirname);
	  rc = 1;
	  goto exit;
	}

      rc = chown (node->dirname, node->user_id, node->group_id);
      if (rc < 0)
	{
	  fprintf (stderr, "Failed to set owner/group for '%s': %m\n", node->dirname);
	  /* wrong permissions are bad, remove dir and continue */
	  rmdir (node->dirname);
	  rc = 1;
	  goto exit;
	}
      /* ignore errors here, time stamps are not critical */
      utimes (node->dirname, stamps);
    exit:
      node = node->next;
    }

  return rc;
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

  if (dir_list != NULL)
    {
      int rc;
      quicksort (&dir_list);
      if ((rc = create_dirs (dir_list)) != 0)
	ec = rc;
    }

  /* XXX missing: free list */

  rpmdbFreeIterator (mi);
  rpmtsFree (ts);

  return ec;
}
