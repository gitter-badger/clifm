/* search.c -- functions for the search system */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
 * All rights reserved.

 * CliFM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CliFM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

#include "helpers.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "exec.h"
#include "glob.h"
#include "navigation.h"
#include "sort.h"

/* List matching filenames in the specified directory */
int
search_glob(char **comm, int invert)
{
	if (!comm || !comm[0])
		return EXIT_FAILURE;

	char *search_str = (char *)NULL, *search_path = (char *)NULL;
	mode_t file_type = 0;
	struct stat file_attrib;

	/* If there are two arguments, the one starting with '-' is the
	 * filetype and the other is the path */
	if (comm[1] && comm[2]) {

		if (comm[1][0] == '-') {
			file_type = (mode_t)comm[1][1];
			search_path = comm[2];
		}

		else if (comm[2][0] == '-') {
			file_type = (mode_t)comm[2][1];
			search_path = comm[1];
		}

		else
			search_path = comm[1];
	}

	/* If just one argument, '-' indicates filetype. Else, we have a
	 * path */
	else if (comm[1]) {

		if (comm[1][0] == '-')
			file_type = (mode_t)comm[1][1];
		else
			search_path = comm[1];
	}

	/* If no arguments, search_path will be NULL and file_type zero */

	int recursive = 0;

	if (file_type) {

		/* Convert filetype into a macro that can be decoded by stat().
		 * If file type is specified, matches will be checked against
		 * this value */
		switch (file_type) {
		case 'd':
			file_type = invert ? DT_DIR : S_IFDIR;
			break;
		case 'r':
			file_type = invert ? DT_REG : S_IFREG;
			break;
		case 'l':
			file_type = invert ? DT_LNK : S_IFLNK;
			break;
		case 's':
			file_type = invert ? DT_SOCK : S_IFSOCK;
			break;
		case 'f':
			file_type = invert ? DT_FIFO : S_IFIFO;
			break;
		case 'b':
			file_type = invert ? DT_BLK : S_IFBLK;
			break;
		case 'c':
			file_type = invert ? DT_CHR : S_IFCHR;
			break;
		case 'x':
			recursive = 1;
			break;

		default:
			fprintf(stderr, _("%s: '%c': aUnrecognized filetype\n"),
			    PROGRAM_NAME, (char)file_type);
			return EXIT_FAILURE;
		}
	}

	if (recursive) {
		char *cmd[] = {"find", (search_path && *search_path) ? search_path
					: ".", "-name", comm[0] + 1, NULL};
		launch_execve(cmd, FOREGROUND, E_NOFLAG);
		return EXIT_SUCCESS;
	}

	/* If we have a path ("/str /path"), chdir into it, since
	 * glob() works on CWD */
	if (search_path && *search_path) {

		/* Deescape the search path, if necessary */
		if (strchr(search_path, '\\')) {
			char *deq_dir = dequote_str(search_path, 0);

			if (!deq_dir) {
				fprintf(stderr, _("%s: %s: Error dequoting filename\n"),
				    PROGRAM_NAME, comm[1]);
				return EXIT_FAILURE;
			}

			strcpy(search_path, deq_dir);
			free(deq_dir);
		}

		size_t path_len = strlen(search_path);
		if (search_path[path_len - 1] == '/')
			search_path[path_len - 1] = '\0';

		/* If search is current directory */
		if ((*search_path == '.' && !search_path[1]) ||
		    (search_path[1] == ws[cur_ws].path[1]
		    && strcmp(search_path, ws[cur_ws].path) == 0))
			search_path = (char *)NULL;

		else if (xchdir(search_path, NO_TITLE) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, search_path,
			    strerror(errno));
			return EXIT_FAILURE;
		}
	}

	int i;

	char *tmp = comm[0];

	if (invert)
		tmp++;

	/* Search for globbing char */
	int glob_char_found = 0;
	for (i = 1; tmp[i]; i++) {
		if (tmp[i] == '*' || tmp[i] == '?' || tmp[i] == '[' || tmp[i] == '{'
		    /* Consider regex chars as well: we don't want this "r$"
		 * to become this "*r$*" */
		    || tmp[i] == '|' || tmp[i] == '^' || tmp[i] == '+' || tmp[i] == '$'
		    || tmp[i] == '.') {
			glob_char_found = 1;
			break;
		}
	}

	/* If search string is just "STR" (no glob chars), change it
	 * to "*STR*" */
	size_t search_str_len = 0;

	if (!glob_char_found) {
		search_str_len = strlen(comm[0]);

		comm[0] = (char *)xrealloc(comm[0], (search_str_len + 2) *
							sizeof(char));
		tmp = comm[0];
		if (invert) {
			++tmp;
			search_str_len = strlen(tmp);
		}

		tmp[0] = '*';
		tmp[search_str_len] = '*';
		tmp[search_str_len + 1] = '\0';
		search_str = tmp;
	}

	else
		search_str = tmp + 1;

	/* Get matches, if any */
	glob_t globbed_files;
	int ret = glob(search_str, GLOB_BRACE, NULL, &globbed_files);

	if (ret != 0) {
		puts(_("Glob: No matches found. Trying regex..."));

		globfree(&globbed_files);

		if (search_path) {
			/* Go back to the directory we came from */
			if (xchdir(ws[cur_ws].path, NO_TITLE) == -1)
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				    ws[cur_ws].path, strerror(errno));
		}

		return EXIT_FAILURE;
	}

	/* We have matches */
	int last_column = 0, scandir_files = 0, found = 0, columns_n = 0;
	size_t flongest = 0;

	/* We need to store pointers to matching filenames in array of
	 * pointers, just as the filename length (to construct the
	 * columned output), and, if searching in CWD, its index (ELN)
	 * in the dirlist array as well */
	char **pfiles = (char **)NULL;
	int *eln = (int *)0;
	size_t *files_len = (size_t *)0;
	struct dirent **ent = (struct dirent **)NULL;

	if (invert) {
		if (!search_path) {
			int k;

			pfiles = (char **)xnmalloc(files + 1, sizeof(char *));
			eln = (int *)xnmalloc(files + 1, sizeof(int));
			files_len = (size_t *)xnmalloc(files + 1, sizeof(size_t));

			for (k = 0; file_info[k].name; k++) {
				int l, f = 0;

				for (l = 0; globbed_files.gl_pathv[l]; l++) {
					if (*globbed_files.gl_pathv[l] == *file_info[k].name
					&& strcmp(globbed_files.gl_pathv[l], file_info[k].name) == 0) {
						f = 1;
						break;
					}
				}

				if (!f) {

					if (file_type && file_info[k].type != file_type)
						continue;

					eln[found] = (int)(k + 1);
					files_len[found] = file_info[k].len + file_info[k].eln_n + 1;
					if (files_len[found] > flongest)
						flongest = files_len[found];

					pfiles[found++] = file_info[k].name;
				}
			}
		}

		else {
			scandir_files = scandir(search_path, &ent, skip_files,
			    xalphasort);

			if (scandir_files != -1) {

				pfiles = (char **)xnmalloc((size_t)scandir_files + 1,
				    sizeof(char *));
				eln = (int *)xnmalloc((size_t)scandir_files + 1,
				    sizeof(int));
				files_len = (size_t *)xnmalloc((size_t)scandir_files + 1,
				    sizeof(size_t));

				int k, l;

				for (k = 0; k < scandir_files; k++) {
					int f = 0;

					for (l = 0; globbed_files.gl_pathv[l]; l++) {
						if (*ent[k]->d_name == *globbed_files.gl_pathv[l]
						&& strcmp(ent[k]->d_name, globbed_files.gl_pathv[l]) == 0) {
							f = 1;
							break;
						}
					}

					if (!f) {

						if (file_type && ent[k]->d_type != file_type)
							continue;

						eln[found] = -1;
						files_len[found] = unicode
								       ? wc_xstrlen(ent[k]->d_name)
								       : strlen(ent[k]->d_name);

						if (files_len[found] > flongest)
							flongest = files_len[found];

						pfiles[found++] = ent[k]->d_name;
					}
				}
			}
		}
	}

	else { /* No invert search */

		pfiles = (char **)xnmalloc(globbed_files.gl_pathc + 1,
		    sizeof(char *));
		eln = (int *)xnmalloc(globbed_files.gl_pathc + 1, sizeof(int));
		files_len = (size_t *)xnmalloc(globbed_files.gl_pathc + 1, sizeof(size_t));

		for (i = 0; globbed_files.gl_pathv[i]; i++) {

			if (*globbed_files.gl_pathv[i] == '.'
			&& (!globbed_files.gl_pathv[i][1]
			|| (globbed_files.gl_pathv[i][1] == '.'
			&& !globbed_files.gl_pathv[i][2])))
				continue;

			if (file_type) {

				/* Simply skip all files not matching file_type */
				if (lstat(globbed_files.gl_pathv[i], &file_attrib) == -1)
					continue;

				if ((file_attrib.st_mode & S_IFMT) != file_type)
					continue;
			}

			pfiles[found] = globbed_files.gl_pathv[i];

			/* Get the longest filename in the list */

			/* If not searching in CWD, we only need to know the file's
			 * length (no ELN) */
			if (search_path) {

				/* This will be passed to colors_list(): -1 means no ELN */
				eln[found] = -1;

				files_len[found] = unicode ? wc_xstrlen(pfiles[found])
							   : strlen(pfiles[found]);

				if (files_len[found] > flongest)
					flongest = files_len[found];

				found++;
			}

			/* If searching in CWD, take into account the file's ELN
			 * when calculating its legnth */
			else {
				size_t j;

				for (j = 0; file_info[j].name; j++) {

					if (*pfiles[found] != *file_info[j].name
					|| strcmp(pfiles[found], file_info[j].name) != 0)
						continue;

					eln[found] = (int)(j + 1);

					files_len[found] = file_info[j].len + file_info[j].eln_n + 1;

					if (files_len[found] > flongest)
						flongest = files_len[found];
				}

				found++;
			}
		}
	}

	/* Print the results using colors and columns */
	if (found) {

		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		unsigned short tcols = w.ws_col;

		if (flongest <= 0 || flongest > tcols)
			columns_n = 1;

		else
			columns_n = (int)(tcols / (flongest + 1));

		if (columns_n > found)
			columns_n = found;

		for (i = 0; i < found; i++) {

			if (!pfiles[i])
				continue;

			if ((i + 1) % columns_n == 0)
				last_column = 1;
			else
				last_column = 0;

			colors_list(pfiles[i], (eln[i] && eln[i] != -1) ? eln[i] : 0,
			    (last_column || i == (found - 1)) ? 0 :
			    (int)(flongest - files_len[i]) + 1,
			    (last_column || i == found - 1) ? 1 : 0);
			/* Second argument to colors_list() is:
			 * 0: Do not print any ELN
			 * Positive number: Print positive number as ELN
			 * -1: Print "?" instead of an ELN */
		}

		printf(_("Matches found: %d\n"), found);
	}

	/*  else
		printf(_("%s: No matches found\n"), PROGRAM_NAME); */

	/* Free stuff */
	if (invert && search_path) {
		i = scandir_files;
		while (--i >= 0)
			free(ent[i]);
		free(ent);
	}

	free(eln);
	free(files_len);
	free(pfiles);
	globfree(&globbed_files);

	/* If needed, go back to the directory we came from */
	if (search_path) {
		if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
			    ws[cur_ws].path, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	if (!found)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* List matching (or not marching, if inverse is set to 1) filenames
 * in the specified directory */
int
search_regex(char **comm, int invert)
{
	if (!comm || !comm[0])
		return EXIT_FAILURE;

	char *search_str = (char *)NULL, *search_path = (char *)NULL;
	mode_t file_type = 0;

	/* If there are two arguments, the one starting with '-' is the
	 * filetype and the other is the path */
	if (comm[1] && comm[2]) {

		if (*comm[1] == '-') {
			file_type = (mode_t) * (comm[1] + 1);
			search_path = comm[2];
		}

		else if (*comm[2] == '-') {
			file_type = (mode_t) * (comm[2] + 1);
			search_path = comm[1];
		}

		else
			search_path = comm[1];
	}

	/* If just one argument, '-' indicates filetype. Else, we have a
	 * path */
	else if (comm[1]) {
		if (*comm[1] == '-')
			file_type = (mode_t) * (comm[1] + 1);
		else
			search_path = comm[1];
	}

	/* If no arguments, search_path will be NULL and file_type zero */

	if (file_type) {

		/* If file type is specified, matches will be checked against
		 * this value */
		switch (file_type) {
		case 'd':
			file_type = DT_DIR;
			break;
		case 'r':
			file_type = DT_REG;
			break;
		case 'l':
			file_type = DT_LNK;
			break;
		case 's':
			file_type = DT_SOCK;
			break;
		case 'f':
			file_type = DT_FIFO;
			break;
		case 'b':
			file_type = DT_BLK;
			break;
		case 'c':
			file_type = DT_CHR;
			break;

		default:
			fprintf(stderr, _("%s: '%c': Unrecognized filetype\n"),
			    PROGRAM_NAME, (char)file_type);
			return EXIT_FAILURE;
		}
	}

	struct dirent **reg_dirlist = (struct dirent **)NULL;
	int tmp_files = 0;

	/* If we have a path ("/str /path"), chdir into it, since
	 * regex() works on CWD */
	if (search_path && *search_path) {

		/* Deescape the search path, if necessary */
		if (strchr(search_path, '\\')) {
			char *deq_dir = dequote_str(search_path, 0);

			if (!deq_dir) {
				fprintf(stderr, _("%s: %s: Error dequoting filename\n"),
				    PROGRAM_NAME, comm[1]);
				return EXIT_FAILURE;
			}

			strcpy(search_path, deq_dir);
			free(deq_dir);
		}

		size_t path_len = strlen(search_path);
		if (search_path[path_len - 1] == '/')
			search_path[path_len - 1] = '\0';

		if ((*search_path == '.' && !search_path[1])
		|| (search_path[1] == ws[cur_ws].path[1]
		&& strcmp(search_path, ws[cur_ws].path) == 0))
			search_path = (char *)NULL;

		if (search_path && *search_path) {
			if (xchdir(search_path, NO_TITLE) == -1) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				    search_path, strerror(errno));
				return EXIT_FAILURE;
			}

			tmp_files = scandir(".", &reg_dirlist, skip_files, xalphasort);

			/*      tmp_files = scandir(".", &reg_dirlist, skip_files,
							sort == 0 ? NULL : sort == 1 ? m_alphasort
							: sort == 2 ? size_sort : sort == 3
							? atime_sort : sort == 4 ? btime_sort
							: sort == 5 ? ctime_sort : sort == 6
							? mtime_sort : sort == 7 ? m_versionsort
							: sort == 8 ? ext_sort : inode_sort); */

			if (tmp_files == -1) {
				fprintf(stderr, "scandir: %s: %s\n", search_path,
				    strerror(errno));

				if (xchdir(ws[cur_ws].path, NO_TITLE) == -1)
					fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
					    ws[cur_ws].path, strerror(errno));

				return EXIT_FAILURE;
			}
		}
	}

	size_t i;

	/* Search for regex expression */
	int regex_found = check_regex(comm[0] + 1);

	/* If search string is just "STR" (no regex chars), change it
	 * to ".*STR.*" */
	if (regex_found == EXIT_FAILURE) {
		size_t search_str_len = strlen(comm[0]);

		comm[0] = (char *)xrealloc(comm[0], (search_str_len + 5) *
							sizeof(char));

		char *tmp_str = (char *)xnmalloc(search_str_len + 1, sizeof(char));

		strcpy(tmp_str, comm[0] + (invert ? 2 : 1));

		*comm[0] = '.';
		*(comm[0] + 1) = '*';
		*(comm[0] + 2) = '\0';
		strcat(comm[0], tmp_str);
		free(tmp_str);
		*(comm[0] + search_str_len + 1) = '.';
		*(comm[0] + search_str_len + 2) = '*';
		*(comm[0] + search_str_len + 3) = '\0';
		search_str = comm[0];
	}

	else
		search_str = comm[0] + (invert ? 2 : 1);

	/* Get matches, if any, using regular expressions */
	regex_t regex_files;
	int ret = regcomp(&regex_files, search_str, REG_NOSUB | REG_EXTENDED);

	if (ret != EXIT_SUCCESS) {
		fprintf(stderr, _("'%s': Invalid regular expression\n"), search_str);

		regfree(&regex_files);

		if (search_path) {
			for (i = 0; i < (size_t)tmp_files; i++)
				free(reg_dirlist[i]);

			free(reg_dirlist);

			if (xchdir(ws[cur_ws].path, NO_TITLE) == -1)
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				    ws[cur_ws].path, strerror(errno));
		}

		return EXIT_FAILURE;
	}

	size_t found = 0;
	int *regex_index = (int *)xnmalloc((search_path ? (size_t)tmp_files
							: files) +
					       1,
	    sizeof(int));

	for (i = 0; i < (search_path ? (size_t)tmp_files : files); i++) {
		if (regexec(&regex_files, (search_path ? reg_dirlist[i]->d_name
		: file_info[i].name), 0, NULL, 0) == EXIT_SUCCESS) {
			if (!invert)
				regex_index[found++] = (int)i;
		} else if (invert)
			regex_index[found++] = (int)i;
	}

	regfree(&regex_files);

	if (!found) {
		fprintf(stderr, _("No matches found\n"));
		free(regex_index);

		if (search_path) {

			int j = tmp_files;
			while (--j >= 0)
				free(reg_dirlist[j]);

			free(reg_dirlist);

			if (xchdir(ws[cur_ws].path, NO_TITLE) == -1)
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				    ws[cur_ws].path, strerror(errno));
		}

		return EXIT_FAILURE;
	}

	/* We have matches */
	int last_column = 0;
	size_t flongest = 0, total_cols = 0, type_ok = 0;

	size_t *files_len = (size_t *)xnmalloc(found + 1, sizeof(size_t));
	int *match_type = (int *)xnmalloc(found + 1, sizeof(int));

	/* Get the longest filename in the list */
	int j = (int)found;
	while (--j >= 0) {

		/* Simply skip all files not matching file_type */
		if (file_type) {

			match_type[j] = 0;

			if (search_path) {
				if (reg_dirlist[regex_index[j]]->d_type != file_type)
					continue;
			}

			else if (file_info[regex_index[j]].type != file_type)
				continue;
		}

		/* Amount of non-filtered files */
		type_ok++;
		/* Index of each non-filtered files */
		match_type[j] = 1;

		/* If not searching in CWD, we only need to know the file's
		 * length (no ELN) */
		if (search_path) {
			files_len[j] = unicode ? wc_xstrlen(
							reg_dirlist[regex_index[j]]->d_name)
							: strlen(reg_dirlist[regex_index[j]]->d_name);

			if (files_len[j] > flongest)
				flongest = files_len[j];
		}

		/* If searching in CWD, take into account the file's ELN
		 * when calculating its legnth */
		else {
			files_len[j] = file_info[regex_index[j]].len
							+ (size_t)DIGINUM(regex_index[j] + 1) + 1;

			if (files_len[j] > flongest)
				flongest = files_len[j];
		}
	}

	if (type_ok) {

		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		unsigned short terminal_cols = w.ws_col;

		if (flongest <= 0 || flongest > terminal_cols)
			total_cols = 1;
		else
			total_cols = (size_t)terminal_cols / (flongest + 1);

		if (total_cols > type_ok)
			total_cols = type_ok;

		/* cur_col: Current columns number */
		size_t cur_col = 0, counter = 0;

		for (i = 0; i < found; i++) {

			if (match_type[i] == 0)
				continue;

			/* Print the results using colors and columns */
			cur_col++;

			/* If the current file is in the last column or is the last
			 * listed file, we need to print no pad and a newline char.
			 * Else, print the corresponding pad, to equate the longest
			 * file length, and no newline char */
			if (cur_col == total_cols) {
				last_column = 1;
				cur_col = 0;
			} else
				last_column = 0;

			/* Counter: Current amount of non-filtered files: if
			 * COUNTER equals TYPE_OK (total amount of non-filtered
			 * files), we have the last file to be printed */
			counter++;

			colors_list(search_path ? reg_dirlist[regex_index[i]]->d_name
					: file_info[regex_index[i]].name, search_path ? NO_ELN
					: regex_index[i] + 1, (last_column || counter == type_ok)
					? NO_PAD : (int)(flongest - files_len[i]) + 1,
					(last_column || counter == type_ok) ? PRINT_NEWLINE
					: NO_NEWLINE);
		}

		printf(_("Matches found: %zu\n"), counter);
	}

	else
		fputs(_("No matches found\n"), stderr);

	/* Free stuff */
	free(files_len);
	free(match_type);
	free(regex_index);
	regfree(&regex_files);

	/* If needed, go back to the directory we came from */
	if (search_path) {
		j = tmp_files;
		while (--j >= 0)
			free(reg_dirlist[j]);

		free(reg_dirlist);

		if (xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
			    ws[cur_ws].path, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	if (type_ok)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}
