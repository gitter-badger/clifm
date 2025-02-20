/* archives.c -- archiving functions */

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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "aux.h"
#include "exec.h"
#include "history.h"
#include "jump.h"
#include "listing.h"
#include "navigation.h"
#include "readline.h"

int zstandard(char *in_file, char *out_file, char mode, char op);

int
handle_iso(char *file)
{
	int exit_status = EXIT_SUCCESS;

	/* Use 7z to
	 * list (l)
	 * extract (e)
	 * extrat to dir (x -oDIR FILE)
	 * test (t) */

	printf(_("%s[e]%sxtract %s[E]%sxtract-to-dir %s[l]%sist "
		 "%s[t]%stest %s[m]%sount %s[q]%suit\n"), bold, df_c, bold,
	    df_c, bold, df_c, bold, df_c, bold, df_c, bold, df_c);

	char *operation = (char *)NULL;
	char sel_op = 0;

	while (!operation) {
		operation = rl_no_hist(_("Operation: "));

		if (!operation)
			continue;

		if (operation && (!operation[0] || operation[1] != '\0')) {
			free(operation);
			operation = (char *)NULL;
			continue;
		}

		switch (*operation) {
		case 'e': /* fallthrough */
		case 'E': /* fallthrough */
		case 'l': /* fallthrough */
		case 'm': /* fallthrough */
		case 't':
			sel_op = *operation;
			free(operation);
			break;

		case 'q':
			free(operation);
			return EXIT_SUCCESS;

		default:
			free(operation);
			operation = (char *)NULL;
			break;
		}

		if (sel_op)
			break;
	}

	char *ret = strchr(file, '\\');
	if (ret) {
		char *deq_file = dequote_str(file, 0);
		if (deq_file) {
			strcpy(file, deq_file);
			free(deq_file);
		}
		ret = (char *)NULL;
	}

	switch (sel_op) {

	/* ########## EXTRACT #######*/
	case 'e': {
		/* 7z x -oDIR FILE (use FILE as DIR) */
		char *o_option = (char *)xnmalloc(strlen(file) + 7, sizeof(char));
		sprintf(o_option, "-o%s.dir", file);

		/* Construct and execute cmd */
		char *cmd[] = {"7z", "x", o_option, file, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;

		free(o_option);
	} break;

	/* ########## EXTRACT TO DIR ####### */
	case 'E': {
		/* 7z x -oDIR FILE (ask for DIR) */
		char *ext_path = (char *)NULL;

		while (!ext_path) {
			ext_path = rl_no_hist(_("Extraction path: "));

			if (!ext_path)
				continue;

			if (ext_path && !*ext_path) {
				free(ext_path);
				ext_path = (char *)NULL;
				continue;
			}
		}

		char *o_option = (char *)xnmalloc(strlen(ext_path) + 3,
		    sizeof(char));
		sprintf(o_option, "-o%s", ext_path);

		/* Construct and execute cmd */
		char *cmd[] = {"7z", "x", o_option, file, NULL};

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;

		free(ext_path);
		free(o_option);
		ext_path = (char *)NULL;
	} break;

	/* ########## LIST ####### */
	case 'l': {
		/* 7z l FILE */
		char *cmd[] = {"7z", "l", file, NULL};

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	} break;

		/* ########## MOUNT ####### */

	case 'm': {
		/* Create mountpoint */
		char *mountpoint = (char *)NULL;

		if (xargs.stealth_mode == 1) {
			mountpoint = (char *)xnmalloc(strlen(file) + 19, sizeof(char));

			sprintf(mountpoint, "/tmp/clifm-mounts/%s", file);
		}

		else {
			mountpoint = (char *)xnmalloc(strlen(CONFIG_DIR) + strlen(file) + 9, sizeof(char));

			sprintf(mountpoint, "%s/mounts/%s", CONFIG_DIR, file);
		}

		char *dir_cmd[] = {"mkdir", "-pm700", mountpoint, NULL};

		if (launch_execve(dir_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			free(mountpoint);
			return EXIT_FAILURE;
		}

		/* Construct and execute cmd */
		char *cmd[] = {"sudo", "mount", "-o", "loop", file,
		    mountpoint, NULL};

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			free(mountpoint);
			return EXIT_FAILURE;
		}

		/* List content of mountpoint */
		if (xchdir(mountpoint, SET_TITLE) == -1) {
			fprintf(stderr, "archiver: %s: %s\n", mountpoint,
			    strerror(errno));
			free(mountpoint);
			return EXIT_FAILURE;
		}

		free(ws[cur_ws].path);
		ws[cur_ws].path = savestring(mountpoint, strlen(mountpoint));
		add_to_jumpdb(ws[cur_ws].path);

		if (cd_lists_on_the_fly) {
			free_dirlist();
			if (list_dir() != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
			add_to_dirhist(ws[cur_ws].path);
		} else
			printf("%s: Successfully mounted on %s\n", file, mountpoint);

		free(mountpoint);
	} break;

	/* ########## TEST #######*/
	case 't': {
		/* 7z t FILE */
		char *cmd[] = {"7z", "t", file, NULL};

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	} break;
	}

	return exit_status;
}

int
create_iso(char *in_file, char *out_file)
{
	int exit_status = EXIT_SUCCESS;
	struct stat file_attrib;

	if (lstat(in_file, &file_attrib) == -1) {
		fprintf(stderr, "archiver: %s: %s\n", in_file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* If IN_FILE is a directory */
	if ((file_attrib.st_mode & S_IFMT) == S_IFDIR) {
		char *cmd[] = {"mkisofs", "-R", "-o", out_file, in_file, NULL};

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	/* If IN_FILE is a block device */
	else if ((file_attrib.st_mode & S_IFMT) == S_IFBLK) {

		char *if_option = (char *)xnmalloc(strlen(in_file) + 4, sizeof(char));
		sprintf(if_option, "if=%s", in_file);

		char *of_option = (char *)xnmalloc(strlen(out_file) + 4, sizeof(char));
		sprintf(of_option, "of=%s", out_file);

		char *cmd[] = {"sudo", "dd", if_option, of_option, "bs=64k",
		    "conv=noerror,sync", "status=progress", NULL};

		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;

		free(if_option);
		free(of_option);
	}

	else {
		fprintf(stderr, "archiver: %s: Invalid file format\nFile "
				"should be either a directory or a block device\n",
				in_file);
		return EXIT_FAILURE;
	}

	return exit_status;
}

/* Run the 'file' command on FILE and look for "ISO 9660" and
 * string in its output. Returns zero if found, one if not, and -1
 * in case of error */
int
check_iso(char *file)
{
	if (!file || !*file) {
		fputs(_("Error opening temporary file\n"), stderr);
		return -1;
	}

	char ISO_TMP_FILE[PATH_MAX] = "";
	char *rand_ext = gen_rand_str(6);

	if (!rand_ext)
		return -1;

	if (xargs.stealth_mode == 1)
		sprintf(ISO_TMP_FILE, "/tmp/clifm-archiver.%s", rand_ext);
	else
		sprintf(ISO_TMP_FILE, "%s/archiver.%s", TMP_DIR, rand_ext);

	free(rand_ext);

	FILE *file_fp = fopen(ISO_TMP_FILE, "w");

	if (!file_fp) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, ISO_TMP_FILE,
				strerror(errno));
		return -1;
	}

	FILE *file_fp_err = fopen("/dev/null", "w");

	if (!file_fp_err) {
		fprintf(stderr, "%s: /dev/null: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		return -1;
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	/* Redirect stdout to the desired file */
	if (dup2(fileno(file_fp), STDOUT_FILENO) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(file_fp_err), STDERR_FILENO) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	fclose(file_fp);
	fclose(file_fp_err);

	char *cmd[] = {"file", "-b", file, NULL};
	int retval = launch_execve(cmd, FOREGROUND, E_NOFLAG);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (retval != EXIT_SUCCESS)
		return -1;

	int is_iso = 0;

	if (access(ISO_TMP_FILE, F_OK) == 0) {

		file_fp = fopen(ISO_TMP_FILE, "r");

		if (file_fp) {
			char line[255] = "";
			fgets(line, (int)sizeof(line), file_fp);
			char *ret = strstr(line, "ISO 9660");

			if (ret)
				is_iso = 1;

			fclose(file_fp);
		}
		unlink(ISO_TMP_FILE);
	}

	if (is_iso)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

/* Run the 'file' command on FILE and look for "archive" and
 * "compressed" strings in its output. Returns zero if compressed,
 * one if not, and -1 in case of error.
 * test_iso is used to determine if ISO files should be checked as
 * well: this is the case when called from open_function() or
 * mime_open(), since both need to check compressed and ISOs as
 * well (and there is no need to run two functions (is_compressed and
 * check_iso), when we can run just one) */
int
is_compressed(char *file, int test_iso)
{
	if (!file || !*file) {
		fputs(_("Error opening temporary file\n"), stderr);
		return -1;
	}

	char *rand_ext = gen_rand_str(6);

	if (!rand_ext)
		return -1;

	char ARCHIVER_TMP_FILE[PATH_MAX];

	if (xargs.stealth_mode == 1)
		sprintf(ARCHIVER_TMP_FILE, "/tmp/clifm-archiver.%s", rand_ext);

	else
		sprintf(ARCHIVER_TMP_FILE, "%s/archiver.%s", TMP_DIR, rand_ext);

	free(rand_ext);

	if (access(ARCHIVER_TMP_FILE, F_OK) == 0)
		unlink(ARCHIVER_TMP_FILE);

	FILE *file_fp = fopen(ARCHIVER_TMP_FILE, "w");

	if (!file_fp) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
		    ARCHIVER_TMP_FILE, strerror(errno));
		return -1;
	}

	FILE *file_fp_err = fopen("/dev/null", "w");

	if (!file_fp_err) {
		fprintf(stderr, "%s: /dev/null: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		return -1;
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	/* Redirect stdout to the desired file */
	if (dup2(fileno(file_fp), STDOUT_FILENO) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(file_fp_err), STDERR_FILENO) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		fclose(file_fp);
		fclose(file_fp_err);
		return -1;
	}

	fclose(file_fp);
	fclose(file_fp_err);

	char *cmd[] = {"file", "-b", file, NULL};
	int retval = launch_execve(cmd, FOREGROUND, E_NOFLAG);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (retval != EXIT_SUCCESS)
		return -1;

	int compressed = 0;

	if (access(ARCHIVER_TMP_FILE, F_OK) == 0) {

		file_fp = fopen(ARCHIVER_TMP_FILE, "r");

		if (file_fp) {
			char line[255];
			fgets(line, (int)sizeof(line), file_fp);
			char *ret = strstr(line, "archive");

			if (ret)
				compressed = 1;

			else {
				ret = strstr(line, "compressed");

				if (ret)
					compressed = 1;

				else if (test_iso) {
					ret = strstr(line, "ISO 9660");

					if (ret)
						compressed = 1;
				}
			}

			fclose(file_fp);
		}

		unlink(ARCHIVER_TMP_FILE);
	}

	if (compressed)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

/* Handle archives and/or compressed files (ARGS) according to MODE:
 * 'c' for archiving/compression, and 'd' for dearchiving/decompression
 * (including listing, extracting, repacking, and mounting). Returns
 * zero on success and one on error. Depends on 'zstd' for Zdtandard
 * files 'atool' and 'archivemount' for the remaining types. */
int
archiver(char **args, char mode)
{
	size_t i;
	int exit_status = EXIT_SUCCESS;

	if (!args[1])
		return EXIT_FAILURE;

	if (mode == 'c') {

			/* ##################################
			 * #        1 - COMPRESSION         #
			 * ##################################*/

		/* Get archive name/type */

		puts(_("Use extension to specify archive/compression type.\n"
		       "Defaults to .tar.gz"));
		char *name = (char *)NULL;
		while (!name) {
			name = rl_no_hist(_("Filename ('q' to quit): "));

			if (!name)
				continue;

			if (!*name) {
				free(name);
				name = (char *)NULL;
				continue;
			}

			if (*name == 'q' && name[1] == '\0') {
				free(name);
				return EXIT_SUCCESS;
			}
		}

				/* ##########################
				 * #        ZSTANDARD       #
				 * ########################## */

		char *ret = strrchr(name, '.');
		if (strcmp(ret, ".zst") == 0) {

			/* Multiple files */
			if (args[2]) {

				printf(_("\n%sNOTE%s: Zstandard does not support "
					 "compression of multiple files into one single "
					 "compressed file. Files will be compressed rather "
					 "into multiple compressed files using original "
					 "filenames\n"), bold, df_c);

				for (i = 1; args[i]; i++) {
					if (zstandard(args[i], NULL, 'c', 0) != EXIT_SUCCESS)
						exit_status = EXIT_FAILURE;
				}
			}

			/* Only one file */
			else
				exit_status = zstandard(args[1], name, 'c', 0);

			free(name);

			return exit_status;
		}

				/* ##########################
				 * #        ISO 9660        #
				 * ########################## */

		if (strcmp(ret, ".iso") == 0) {
			exit_status = create_iso(args[1], name);
			free(name);
			return exit_status;
		}

				/* ##########################
				 * #          OTHERS        #
				 * ########################## */

		/* Escape the string, if needed */
		char *esc_name = escape_str(name);
		free(name);

		if (!esc_name) {
			fprintf(stderr, _("archiver: %s: Error escaping string\n"),
					name);
			return EXIT_FAILURE;
		}

		/* Construct the command */
		char *cmd = (char *)NULL;
		char *ext_ok = strchr(esc_name, '.');
		size_t cmd_len = strlen(esc_name) + 10 + ((!ext_ok) ? 8 : 0);

		cmd = (char *)xcalloc(cmd_len, sizeof(char));

		/* If name has no extension, add the default */
		sprintf(cmd, "atool -a %s%s", esc_name, (!ext_ok) ? ".tar.gz" : "");

		for (i = 1; args[i]; i++) {
			cmd_len += strlen(args[i]) + 1;
			cmd = (char *)xrealloc(cmd, (cmd_len + 1) * sizeof(char));
			strcat(cmd, " ");
			strcat(cmd, args[i]);
		}

		if (launch_execle(cmd) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;

		free(cmd);
		free(esc_name);

		return exit_status;
	}

	/* mode == 'd' */

			/* ##################################
			 * #      2 - DECOMPRESSION         #
			 * ##################################*/

	/* Exit if at least one non-compressed file is found */
	for (i = 1; args[i]; i++) {
		char *deq = (char *)NULL;

		if (strchr(args[i], '\\')) {
			deq = dequote_str(args[i], 0);
			strcpy(args[i], deq);
			free(deq);
		}

		if (is_compressed(args[i], 1) != 0) {
			fprintf(stderr, _("archiver: %s: Not an archive/compressed file\n"),
					args[i]);
			return EXIT_FAILURE;
		}
	}

				/* ##########################
				 * #        ISO 9660        #
				 * ########################## */

	char *ret = strrchr(args[1], '.');

	if ((ret && strcmp(ret, ".iso") == 0) || check_iso(args[1]) == 0)
		return handle_iso(args[1]);

				/* ##########################
				 * #        ZSTANDARD       #
				 * ########################## */

	/* Check if we have at least one Zstandard file */

	int zst_index = -1;
	size_t files_num = 0;

	for (i = 1; args[i]; i++) {
		files_num++;
		if (args[i][strlen(args[i]) - 1] == 't') {
			char *retval = strrchr(args[i], '.');
			if (retval) {
				if (strcmp(retval, ".zst") == 0)
					zst_index = (int)i;
			}
		}
	}

	if (zst_index != -1) {

		/* Multiple files */
		if (files_num > 1) {

			printf(_("%sNOTE%s: Using Zstandard\n"), bold, df_c);
			printf(_("%s[e]%sxtract %s[t]%sest %s[i]%snfo %s[q]%suit\n"),
			    bold, df_c, bold, df_c, bold, df_c, bold, df_c);

			char *operation = (char *)NULL;
			char sel_op = 0;
			while (!operation) {
				operation = rl_no_hist(_("Operation: "));

				if (!operation)
					continue;

				if (operation && (!operation[0] || operation[1] != '\0')) {
					free(operation);
					operation = (char *)NULL;
					continue;
				}

				switch (*operation) {
				case 'e': /* fallthrough */
				case 't': /* fallthrough */
				case 'i':
					sel_op = *operation;
					break;

				case 'q':
					free(operation);
					return EXIT_SUCCESS;

				default:
					free(operation);
					operation = (char *)NULL;
					break;
				}
			}

			for (i = 1; args[i]; i++) {
				if (zstandard(args[i], NULL, 'd', sel_op) != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
			}

			free(operation);
			return exit_status;
		}

		/* Just one file */
		else {
			if (zstandard(args[zst_index], NULL, 'd', 0) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			return exit_status;
		}
	}

				/* ##########################
				 * #          OTHERS        #
				 * ########################## */

	/* 1) Get operation to be performed
	 * ################################ */

	printf(_("%s[e]%sxtract %s[E]%sxtract-to-dir %s[l]%sist "
		 "%s[m]%sount %s[r]%sepack %s[q]%suit\n"), bold, df_c, bold,
	    df_c, bold, df_c, bold, df_c, bold, df_c, bold, df_c);

	char *operation = (char *)NULL;
	char sel_op = 0;

	while (!operation) {
		operation = rl_no_hist(_("Operation: "));

		if (!operation)
			continue;

		if (operation && (!operation[0] || operation[1] != '\0')) {
			free(operation);
			operation = (char *)NULL;
			continue;
		}

		switch (*operation) {
		case 'e': /* fallthrough */
		case 'E': /* fallthrough */
		case 'l': /* fallthrough */
		case 'm': /* fallthrough */
		case 'r':
			sel_op = *operation;
			free(operation);
			break;

		case 'q':
			free(operation);
			return EXIT_SUCCESS;

		default:
			free(operation);
			operation = (char *)NULL;
			break;
		}

		if (sel_op)
			break;
	}

	/* 2) Prepare files based on operation
	 * #################################### */

	char *dec_files = (char *)NULL;

	switch (sel_op) {
	case 'e': /* fallthrough */
	case 'r': {

		/* Store all filenames into one single variable */
		size_t len = 1;
		dec_files = (char *)xnmalloc(len, sizeof(char));
		*dec_files = '\0';

		for (i = 1; args[i]; i++) {

			/* Escape the string, if needed */
			char *esc_name = escape_str(args[i]);
			if (!esc_name)
				continue;

			len += strlen(esc_name) + 1;
			dec_files = (char *)xrealloc(dec_files, (len + 1) * sizeof(char));
			strcat(dec_files, " ");
			strcat(dec_files, esc_name);

			free(esc_name);
		}
	} break;

	case 'E':
	case 'l':
	case 'm': {

		/* These operation won't be executed via the system shell,
			 * so that we need to deescape files if necessary */
		for (i = 1; args[i]; i++) {

			if (strchr(args[i], '\\')) {
				char *deq_name = dequote_str(args[i], 0);

				if (!deq_name) {
					fprintf(stderr, _("archiver: %s: Error "
							"dequoting filename\n"), args[i]);
					return EXIT_FAILURE;
				}

				strcpy(args[i], deq_name);
				free(deq_name);
				deq_name = (char *)NULL;
			}
		}
	} break;
	}

	/* 3) Construct and run the corresponding commands
	 * ############################################### */

	switch (sel_op) {

		/* ########## EXTRACT ############## */

	case 'e': {
		char *cmd = (char *)NULL;
		cmd = (char *)xnmalloc(strlen(dec_files) + 13, sizeof(char));

		sprintf(cmd, "atool -x -e %s", dec_files);

		if (launch_execle(cmd) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;

		free(cmd);
		free(dec_files);
	} break;

		/* ########## EXTRACT TO DIR ############## */

	case 'E':
		for (i = 1; args[i]; i++) {

			/* Ask for extraction path */
			printf(_("%sFile%s: %s\n"), bold, df_c, args[i]);

			char *ext_path = (char *)NULL;

			while (!ext_path) {
				ext_path = rl_no_hist(_("Extraction path: "));

				if (!ext_path)
					continue;

				if (ext_path && !*ext_path) {
					free(ext_path);
					ext_path = (char *)NULL;
					continue;
				}
			}

			/* Construct and execute cmd */
			char *cmd[] = {"atool", "-X", ext_path, args[i], NULL};

			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;

			free(ext_path);
			ext_path = (char *)NULL;
		}
		break;

		/* ########## LIST ############## */

	case 'l':
		for (i = 1; args[i]; i++) {

			printf(_("%s%sFile%s: %s\n"), (i > 1) ? "\n" : "",
			    bold, df_c, args[i]);

			char *cmd[] = {"atool", "-l", args[i], NULL};

			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}
		break;

		/* ########## MOUNT ############## */

	case 'm':
		for (i = 1; args[i]; i++) {

			/* Create mountpoint */
			char *mountpoint = (char *)NULL;

			if (xargs.stealth_mode == 1) {
				mountpoint = (char *)xnmalloc(strlen(args[i]) + 19,
				    sizeof(char));

				sprintf(mountpoint, "/tmp/clifm-mounts/%s",
				    args[i]);
			}

			else {
				mountpoint = (char *)xnmalloc(strlen(CONFIG_DIR) + strlen(args[i]) + 9, sizeof(char));

				sprintf(mountpoint, "%s/mounts/%s", CONFIG_DIR,
				    args[i]);
			}

			char *dir_cmd[] = {"mkdir", "-pm700", mountpoint, NULL};

			if (launch_execve(dir_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
				free(mountpoint);
				return EXIT_FAILURE;
			}

			/* Construct and execute cmd */
			char *cmd[] = {"archivemount", args[i], mountpoint, NULL};

			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
				free(mountpoint);
				continue;
			}

			/* List content of mountpoint if there is only
				 * one archive */
			if (files_num > 1) {
				printf(_("%s%s%s: Succesfully mounted on %s\n"),
						bold, args[i], df_c, mountpoint);
				free(mountpoint);
				continue;
			}

			if (xchdir(mountpoint, SET_TITLE) == -1) {
				fprintf(stderr, "archiver: %s: %s\n", mountpoint, strerror(errno));
				free(mountpoint);
				return EXIT_FAILURE;
			}

			free(ws[cur_ws].path);
			ws[cur_ws].path = (char *)xcalloc(strlen(mountpoint) + 1,
			    sizeof(char));
			strcpy(ws[cur_ws].path, mountpoint);

			free(mountpoint);

			add_to_jumpdb(ws[cur_ws].path);

			if (cd_lists_on_the_fly) {
				free_dirlist();
				if (list_dir() != EXIT_SUCCESS)
					exit_status = EXIT_FAILURE;
				add_to_dirhist(ws[cur_ws].path);
			}
		}
		break;

		/* ########## REPACK ############## */

	case 'r': {
		/* Ask for new archive/compression format */
		puts(_("Enter 'q' to quit"));

		char *format = (char *)NULL;
		while (!format) {
			format = rl_no_hist(_("New format (Ex: .tar.xz): "));
			if (!format)
				continue;

			if (!*format || (*format != '.' && *format != 'q')) {
				free(format);
				format = (char *)NULL;
				continue;
			}

			if (*format == 'q' && format[1] == '\0') {
				free(format);
				free(dec_files);
				return EXIT_SUCCESS;
			}
		}

		/* Construct and execute cmd */
		char *cmd = (char *)NULL;
		cmd = (char *)xnmalloc(strlen(format) + strlen(dec_files) + 16, sizeof(char));
		sprintf(cmd, "arepack -F %s -e %s", format, dec_files);

		if (launch_execle(cmd) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;

		free(format);
		free(dec_files);
		free(cmd);
	} break;
	}

	return exit_status;
}

/* If MODE is 'c', compress IN_FILE producing a zstandard compressed
 * file named OUT_FILE. If MODE is 'd', extract, test or get
 * information about IN_FILE. OP is used only for the 'd' mode: it
 * tells if we have one or multiple file. Returns zero on success and
 * one on error */
int
zstandard(char *in_file, char *out_file, char mode, char op)
{
	int exit_status = EXIT_SUCCESS;

	char *deq_file = dequote_str(in_file, 0);

	if (!deq_file) {
		fprintf(stderr, _("archiver: %s: Error dequoting filename\n"), in_file);
		return EXIT_FAILURE;
	}

	if (mode == 'c') {

		if (out_file) {
			char *cmd[] = {"zstd", "-zo", out_file, deq_file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		else {
			char *cmd[] = {"zstd", "-z", deq_file, NULL};

			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		free(deq_file);
		return exit_status;
	}

	/* mode == 'd' */

	/* op is non-zero when multiple files, including at least one
	 * zst file, are passed to the archiver function */
	if (op != 0) {
		char option[3] = "";

		switch (op) {
		case 'e':
			strcpy(option, "-d");
			break;
		case 't':
			strcpy(option, "-t");
			break;
		case 'i':
			strcpy(option, "-l");
			break;
		}

		char *cmd[] = {"zstd", option, deq_file, NULL};

		exit_status = launch_execve(cmd, FOREGROUND, E_NOFLAG);

		free(deq_file);

		if (exit_status != EXIT_SUCCESS)
			return EXIT_FAILURE;

		return EXIT_SUCCESS;
	}

	printf(_("%s[e]%sxtract %s[t]%sest %s[i]%snfo %s[q]%suit\n"),
	    bold, df_c, bold, df_c, bold, df_c, bold, df_c);

	char *operation = (char *)NULL;

	while (!operation) {
		operation = rl_no_hist(_("Operation: "));

		if (!operation)
			continue;

		if (operation && (!operation[0] || operation[1] != '\0')) {
			free(operation);
			operation = (char *)NULL;
			continue;
		}

		switch (*operation) {
		case 'e': {
			char *cmd[] = {"zstd", "-d", deq_file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		} break;

		case 't': {
			char *cmd[] = {"zstd", "-t", deq_file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		} break;

		case 'i': {
			char *cmd[] = {"zstd", "-l", deq_file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		} break;

		case 'q':
			free(operation);
			free(deq_file);
			return EXIT_SUCCESS;

		default:
			free(operation);
			operation = (char *)NULL;
			break;
		}
	}

	free(operation);
	free(deq_file);

	return exit_status;
}
