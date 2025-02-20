/* colors.c -- functions to control interface color */

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

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __linux__
#include <sys/capability.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "aux.h"
#include "colors.h"
#include "listing.h"
#include "mime.h"
#include "misc.h"

/* Returns a pointer to the corresponding color code for EXT, if some
 * color was defined */
char *
get_ext_color(const char *ext)
{
	if (!ext || !ext_colors_n)
		return (char *)NULL;

	ext++;

	int i = (int)ext_colors_n;
	while (--i >= 0) {

		if (!ext_colors[i] || !*ext_colors[i] || !ext_colors[i][2])
			continue;

		char *p = (char *)ext, *q = ext_colors[i];

		/* +2 because stored extensions have this form: *.ext */
		q += 2;

		size_t match = 1;
		while (*p) {
			if (*p++ != *q++) {
				match = 0;
				break;
			}
		}

		if (!match || *q != '=')
			continue;

		return ++q;
	}

	return (char *)NULL;
}

/* Check if STR has the format of a color code string (a number or a
 * semicolon list (max 12 fields) of numbers of at most 3 digits each).
 * Returns 1 if true and 0 if false. */
int
is_color_code(const char *str)
{
	if (!str || !*str)
		return 0;

	size_t digits = 0, semicolon = 0;

	while (*str) {

		if (*str >= '0' && *str <= '9')
			digits++;

		else if (*str == ';') {

			if (*(str + 1) == ';') /* Consecutive semicolons */
				return 0;

			digits = 0;
			semicolon++;
		}

		/* Neither digit nor semicolon */
		else if (*str != '\n')
			return 0;

		str++;
	}

	/* No digits at all, ending semicolon, more than eleven fields, or
	 * more than three consecutive digits */
	if (!digits || digits > 3 || semicolon > 11)
		return 0;

	/* At this point, we have a semicolon separated string of digits (3
	 * consecutive max) with at most 12 fields. The only thing not
	 * validated here are numbers themselves */

	return 1;
}

/* Strip color lines from the config file (FiletypeColors, if mode is
 * 't', and ExtColors, if mode is 'x') returning the same string
 * containing only allowed characters */
char *
strip_color_line(const char *str, char mode)
{
	if (!str || !*str)
		return (char *)NULL;

	char *buf = (char *)xnmalloc(strlen(str) + 1, sizeof(char));
	size_t len = 0;

	switch (mode) {

	case 't': /* di=01;31: */
		while (*str) {
			if ((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'z')
			|| *str == '=' || *str == ';' || *str == ':')
				buf[len++] = *str;
			str++;
		}
		break;

	case 'x': /* *.tar=01;31: */
		while (*str) {
			if ((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'z')
			|| (*str >= 'A' && *str <= 'Z') || *str == '*' || *str == '.'
			|| *str == '=' || *str == ';' || *str == ':')
				buf[len++] = *str;
			str++;
		}
		break;
	}

	if (!len || !*buf) {
		free(buf);
		return (char *)NULL;
	}

	buf[len] = '\0';

	return buf;
}

void
free_colors(void)
{
	/* Reset whatever value was loaded */
	*bm_c = '\0';
	*dl_c = '\0';
	*el_c = '\0';
	*mi_c = '\0';
	*tx_c = '\0';
	*df_c = '\0';
	*dc_c = '\0';
	*wc_c = '\0';
	*dh_c = '\0';
	*li_c = '\0';
	*ti_c = '\0';
	*em_c = '\0';
	*wm_c = '\0';
	*nm_c = '\0';
	*si_c = '\0';
	*nd_c = '\0';
	*nf_c = '\0';
	*di_c = '\0';
	*ed_c = '\0';
	*ne_c = '\0';
	*ex_c = '\0';
	*ee_c = '\0';
	*bd_c = '\0';
	*ln_c = '\0';
	*mh_c = '\0';
	*or_c = '\0';
	*so_c = '\0';
	*pi_c = '\0';
	*cd_c = '\0';
	*fi_c = '\0';
	*ef_c = '\0';
	*su_c = '\0';
	*sg_c = '\0';
	*ca_c = '\0';
	*st_c = '\0';
	*tw_c = '\0';
	*ow_c = '\0';
	*no_c = '\0';
	*uf_c = '\0';
	return;
}

int
cschemes_function(char **args)
{
	if (xargs.stealth_mode == 1) {
		fprintf(stderr, _("%s: The color schemes function is "
				  "disabled in stealth mode\nTIP: To change the current "
				  "color scheme use the following environment "
				  "variables: CLIFM_FILE_COLORS, CLIFM_IFACE_COLORS, "
				  "and CLIFM_EXT_COLORS\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (!args)
		return EXIT_FAILURE;

	if (!args[1]) {

		if (!cschemes_n) {
			printf(_("%s: No color schemes found\n"), PROGRAM_NAME);
			return EXIT_SUCCESS;
		}

		size_t i;

		for (i = 0; color_schemes[i]; i++) {

			if (cur_cscheme == color_schemes[i])
				printf("%s%s%s\n", mi_c, color_schemes[i], df_c);

			else
				printf("%s\n", color_schemes[i]);
		}

		return EXIT_SUCCESS;
	}

	if (*args[1] == '-' && strcmp(args[1], "--help") == 0) {
		puts(_("Usage: cs, colorschemes [edit] [COLORSCHEME]"));
		return EXIT_SUCCESS;
	}

	if (*args[1] == 'e' && (!args[1][1] || strcmp(args[1], "edit") == 0)) {
		char file[PATH_MAX] = "";
		sprintf(file, "%s/%s.cfm", COLORS_DIR, cur_cscheme);

		struct stat attr;
		stat(file, &attr);
		time_t mtime_bfr = (time_t)attr.st_mtime;

		char *cmd[] = {"mm", file, NULL};
		int ret = mime_open(cmd);

		if (ret != EXIT_FAILURE) {

			stat(file, &attr);

			if (mtime_bfr != (time_t)attr.st_mtime
			&& set_colors(cur_cscheme, 0) == EXIT_SUCCESS
			&& cd_lists_on_the_fly) {

				free_dirlist();
				list_dir();
			}
		}

		return ret;
	}

	if (*args[1] == 'n' && (!args[1][1] || strcmp(args[1], "name") == 0)) {
		printf(_("%s: current color scheme: %s\n"), PROGRAM_NAME,
		    cur_cscheme ? cur_cscheme : "?");
		return EXIT_SUCCESS;
	}

	size_t i, cs_found = 0;

	for (i = 0; color_schemes[i]; i++) {
		if (*args[1] == *color_schemes[i]
		&& strcmp(args[1], color_schemes[i]) == 0) {

			cs_found = 1;

			if (set_colors(args[1], 0) == EXIT_SUCCESS) {

				cur_cscheme = color_schemes[i];

				switch_cscheme = 1;

				if (cd_lists_on_the_fly) {
					free_dirlist();
					list_dir();
				}

				switch_cscheme = 0;

				return EXIT_SUCCESS;
			}
		}
	}

	if (!cs_found)
		fprintf(stderr, _("%s: No such color scheme\n"), PROGRAM_NAME);

	return EXIT_FAILURE;
}

/* Open the config file, get values for filetype and extension colors
 * and copy these values into the corresponding variable. If some value
 * is not found, or if it's a wrong value, the default is set. */
int
set_colors(const char *colorscheme, int env)
{
	char *filecolors = (char *)NULL, *extcolors = (char *)NULL,
	     *ifacecolors = (char *)NULL;

	*dir_ico_c = '\0';

	/* Set a pointer to the current color scheme */
	if (colorscheme && *colorscheme && color_schemes) {

		char *def_cscheme = (char *)NULL;

		size_t i;

		for (i = 0; color_schemes[i]; i++) {

			if (*colorscheme == *color_schemes[i]
			&& strcmp(colorscheme, color_schemes[i]) == 0) {
				cur_cscheme = color_schemes[i];
				break;
			}

			if (*color_schemes[i] == 'd'
			&& strcmp(color_schemes[i], "default") == 0)
				def_cscheme = color_schemes[i];
		}

		if (!cur_cscheme) {
			_err('w', PRINT_PROMPT, _("%s: %s: No such color scheme. "
				"Falling back to the default one\n"), PROGRAM_NAME, colorscheme);

			if (def_cscheme)
				cur_cscheme = def_cscheme;
		}
	}

	/* env is true only when the function is called from main() */
	if (env) {
		/* Try to get colors from environment variables */
		char *env_filecolors = getenv("CLIFM_FILE_COLORS");
		char *env_extcolors = getenv("CLIFM_EXT_COLORS");
		char *env_ifacecolors = getenv("CLIFM_IFACE_COLORS");

		if (env_filecolors && !filecolors)
			filecolors = savestring(env_filecolors, strlen(env_filecolors));

		env_filecolors = (char *)NULL;

		if (env_extcolors && !extcolors)
			extcolors = savestring(env_extcolors, strlen(env_extcolors));

		env_extcolors = (char *)NULL;

		if (env_ifacecolors && !ifacecolors)
			ifacecolors = savestring(env_ifacecolors, strlen(env_ifacecolors));

		env_ifacecolors = (char *)NULL;
	}

	if (config_ok && (!filecolors || !extcolors || !ifacecolors)) {
		/* Get color lines, for both file types and extensions, from
	 * COLORSCHEME file */

		char *colorscheme_file = (char *)xnmalloc(strlen(COLORS_DIR)
							+ strlen(colorscheme) + 6, sizeof(char));
		sprintf(colorscheme_file, "%s/%s.cfm", COLORS_DIR,
		    colorscheme ? colorscheme : "default");

		FILE *fp_colors = fopen(colorscheme_file, "r");

		if (fp_colors) {

			/* If called from the color scheme function, reset all
			 * color values before proceeding */
			if (!env)
				free_colors();

			char *line = (char *)NULL;
			ssize_t line_len = 0;
			size_t line_size = 0;
			int file_type_found = 0, ext_type_found = 0,
			    iface_found = 0, dir_icon_found = 0;

			while ((line_len = getline(&line, &line_size,
				    fp_colors)) > 0) {

				/* Interface colors */
				if (!ifacecolors && *line == 'I'
				&& strncmp(line, "InterfaceColors=", 16) == 0) {
					iface_found = 1;
					char *opt_str = strchr(line, '=');

					if (!opt_str)
						continue;

					opt_str++;

					char *color_line = strip_color_line(opt_str, 't');

					if (!color_line)
						continue;

					ifacecolors = savestring(color_line, strlen(color_line));
					free(color_line);
				}

				/* Filetype Colors */
				if (!filecolors && *line == 'F'
				&& strncmp(line, "FiletypeColors=", 15) == 0) {
					file_type_found = 1;
					char *opt_str = strchr(line, '=');

					if (!opt_str)
						continue;

					opt_str++;

					char *color_line = strip_color_line(opt_str, 't');

					if (!color_line)
						continue;

					filecolors = savestring(color_line, strlen(color_line));
					free(color_line);
				}

				/* File extension colors */
				if (!extcolors && *line == 'E' && strncmp(line, "ExtColors=", 10) == 0) {
					ext_type_found = 1;
					char *opt_str = strchr(line, '=');

					if (!opt_str)
						continue;

					opt_str++;

					char *color_line = strip_color_line(opt_str, 'x');

					if (!color_line)
						continue;

					extcolors = savestring(color_line, strlen(color_line));
					free(color_line);
				}

				/* Dir icons Color */
				if (*line == 'D' && strncmp(line, "DirIconsColor=", 14) == 0) {
					dir_icon_found = 1;
					char *opt_str = strchr(line, '=');

					if (!opt_str)
						continue;

					if (!*(++opt_str))
						continue;

					if (*opt_str == '\'' || *opt_str == '"')
						opt_str++;

					if (!*opt_str)
						continue;

					int nl_removed = 0;
					if (line[line_len - 1] == '\n') {
						line[line_len - 1] = '\0';
						nl_removed = 1;
					}

					int end_char = (int)line_len - 1;

					if (nl_removed)
						end_char--;

					if (line[end_char] == '\'' || line[end_char] == '"')
						line[end_char] = '\0';

					sprintf(dir_ico_c, "\x1b[%sm", opt_str);
				}

				if (file_type_found && ext_type_found
				&& iface_found && dir_icon_found)
					break;
			}

			free(line);
			line = (char *)NULL;

			fclose(fp_colors);
		}

		/* If fopen failed */
		else {
			if (!env) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				    colorscheme_file, strerror(errno));
				free(colorscheme_file);
				return EXIT_FAILURE;
			}

			else {
				_err('w', PRINT_PROMPT, _("%s: %s: No such color scheme. "
					"Falling back to the default one\n"), PROGRAM_NAME,
					colorscheme);
			}
		}

		free(colorscheme_file);
		colorscheme_file = (char *)NULL;
	}

			/* ##############################
			 * #    FILE EXTENSION COLORS   #
			 * ############################## */

	/* Split the colors line into substrings (one per color) */

	if (!extcolors) {

		/* Unload current extension colors */
		if (ext_colors_n) {
			int i = (int)ext_colors_n;

			while (--i >= 0)
				free(ext_colors[i]);

			free(ext_colors);
			ext_colors = (char **)NULL;
			free(ext_colors_len);
			ext_colors_n = 0;
		}
	}

	else {

		char *p = extcolors, *buf = (char *)NULL;
		size_t len = 0;
		int eol = 0;

		if (ext_colors_n) {
			int i = (int)ext_colors_n;

			while (--i >= 0)
				free(ext_colors[i]);

			free(ext_colors);
			ext_colors = (char **)NULL;
			free(ext_colors_len);
			ext_colors_n = 0;
		}

		while (!eol) {
			switch (*p) {

			case '\0': /* fallthrough */
			case '\n': /* fallthrough */
			case ':':
				buf[len] = '\0';
				ext_colors = (char **)xrealloc(ext_colors,
				    (ext_colors_n + 1) * sizeof(char *));
				ext_colors[ext_colors_n++] = savestring(buf, len);
				*buf = '\0';

				if (!*p)
					eol = 1;

				len = 0;
				p++;
				break;

			default:
				buf = (char *)xrealloc(buf, (len + 2) * sizeof(char));
				buf[len++] = *(p++);
				break;
			}
		}

		p = (char *)NULL;
		free(extcolors);
		extcolors = (char *)NULL;

		if (buf) {
			free(buf);
			buf = (char *)NULL;
		}

		if (ext_colors) {
			ext_colors = (char **)xrealloc(ext_colors, (ext_colors_n + 1) * sizeof(char *));
			ext_colors[ext_colors_n] = (char *)NULL;
		}

		/* Make sure we have valid color codes and store the length
		 * of each stored extension: this length will be used later
		 * when listing files */
		ext_colors_len = (size_t *)xnmalloc(ext_colors_n, sizeof(size_t));

		int i = (int)ext_colors_n;
		while (--i >= 0) {
			char *ret = strrchr(ext_colors[i], '=');

			if (!ret || !is_color_code(ret + 1)) {
				*ext_colors[i] = '\0';
				ext_colors_len[i] = 0;
				continue;
			}

			size_t j, ext_len = 0;

			for (j = 2; ext_colors[i][j] && ext_colors[i][j] != '='; j++)
				ext_len++;

			ext_colors_len[i] = ext_len;
		}
	}

			/* ##############################
			 * #      INTERFACE COLORS      #
			 * ############################## */

	if (!ifacecolors) {

		/* Free and reset whatever value was loaded */
		*bm_c = '\0';
		*dl_c = '\0';
		*el_c = '\0';
		*mi_c = '\0';
		*tx_c = '\0';
		*df_c = '\0';
		*dc_c = '\0';
		*wc_c = '\0';
		*dh_c = '\0';
		*li_c = '\0';
		*ti_c = '\0';
		*em_c = '\0';
		*wm_c = '\0';
		*nm_c = '\0';
		*si_c = '\0';
	}

	else {

		char *p = ifacecolors, *buf = (char *)NULL,
		     **colors = (char **)NULL;
		size_t len = 0, words = 0;
		int eol = 0;

		while (!eol) {
			switch (*p) {

			case '\0': /* fallthrough */
			case '\n': /* fallthrough */
			case ':':
				buf[len] = '\0';
				colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
				colors[words++] = savestring(buf, len);
				*buf = '\0';

				if (!*p)
					eol = 1;

				len = 0;
				p++;
				break;

			default:
				buf = (char *)xrealloc(buf, (len + 2) * sizeof(char));
				buf[len++] = *(p++);
				break;
			}
		}

		p = (char *)NULL;
		free(ifacecolors);
		ifacecolors = (char *)NULL;

		if (buf) {
			free(buf);
			buf = (char *)NULL;
		}

		if (colors) {
			colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
			colors[words] = (char *)NULL;
		}

		int i = (int)words;
		/* Set the color variables */
		while (--i >= 0) {

			if (*colors[i] == 't' && strncmp(colors[i], "tx=", 3) == 0) {

				if (!is_color_code(colors[i] + 3))
					/* zero the corresponding variable as a flag for
					 * the check after this for loop to prepare the
					 * variable to hold the default color */
					*tx_c = '\0';

				else
					snprintf(tx_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
					    colors[i] + 3);
			}

			else if (*colors[i] == 'b' && strncmp(colors[i], "bm=", 3) == 0) {

				if (!is_color_code(colors[i] + 3))
					*bm_c = '\0';

				else
					snprintf(bm_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'l' && strncmp(colors[i], "li=", 3) == 0) {

				if (!is_color_code(colors[i] + 3))
					*li_c = '\0';

				else
					snprintf(li_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
					    colors[i] + 3);
			}

			else if (*colors[i] == 't' && strncmp(colors[i], "ti=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*ti_c = '\0';
				else
					snprintf(ti_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
					    colors[i] + 3);
			}

			else if (*colors[i] == 'e' && strncmp(colors[i], "em=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*em_c = '\0';
				else
					snprintf(em_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
					    colors[i] + 3);
			}

			else if (*colors[i] == 'w' && strncmp(colors[i], "wm=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*wm_c = '\0';
				else
					snprintf(wm_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
					    colors[i] + 3);
			}

			else if (*colors[i] == 'n' && strncmp(colors[i], "nm=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*nm_c = '\0';
				else
					snprintf(nm_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
					    colors[i] + 3);
			}

			else if (*colors[i] == 's' && strncmp(colors[i], "si=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*si_c = '\0';
				else
					snprintf(si_c, MAX_COLOR + 2, "\001\x1b[%sm\002",
					    colors[i] + 3);
			}

			else if (*colors[i] == 'e' && strncmp(colors[i], "el=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*el_c = '\0';
				else
					snprintf(el_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'm' && strncmp(colors[i], "mi=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*mi_c = '\0';
				else
					snprintf(mi_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'd' && strncmp(colors[i], "dl=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*dl_c = '\0';
				else
					snprintf(dl_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'd' && strncmp(colors[i], "df=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*df_c = '\0';
				else
					snprintf(df_c, MAX_COLOR - 1, "\x1b[%s;49m", colors[i] + 3);
			}

			else if (*colors[i] == 'd' && strncmp(colors[i], "dc=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*dc_c = '\0';
				else
					snprintf(dc_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'w' && strncmp(colors[i], "wc=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*wc_c = '\0';
				else
					snprintf(wc_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			else if (*colors[i] == 'd' && strncmp(colors[i], "dh=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*dh_c = '\0';
				else
					snprintf(dh_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			free(colors[i]);
		}

		free(colors);
		colors = (char **)NULL;
	}

	if (!filecolors) {
		*nd_c = '\0';
		*nf_c = '\0';
		*di_c = '\0';
		*ed_c = '\0';
		*ne_c = '\0';
		*ex_c = '\0';
		*ee_c = '\0';
		*bd_c = '\0';
		*ln_c = '\0';
		*mh_c = '\0';
		*or_c = '\0';
		*so_c = '\0';
		*pi_c = '\0';
		*cd_c = '\0';
		*fi_c = '\0';
		*ef_c = '\0';
		*su_c = '\0';
		*sg_c = '\0';
		*ca_c = '\0';
		*st_c = '\0';
		*tw_c = '\0';
		*ow_c = '\0';
		*no_c = '\0';
		*uf_c = '\0';

		/* Set the LS_COLORS environment variable with default values */
		char lsc[] = DEF_LS_COLORS;

		if (setenv("LS_COLORS", lsc, 1) == -1)
			fprintf(stderr, _("%s: Error registering environment colors\n"),
					PROGRAM_NAME);
	}

	else {
		/* Set the LS_COLORS environment variable to use CliFM own
		 * colors. In this way, files listed for TAB completion will
		 * use CliFM colors instead of system colors */

		/* Strip CLiFM custom filetypes (nd, ne, nf, ed, ef, ee, uf,
		 * bm, el, mi, dl, tx, df, dc, wc, dh, li, ti, em, wm, nm, si,
		 * and ca), from filecolors to construct a valid value for
		 * LS_COLORS */
		size_t buflen = 0, linec_len = strlen(filecolors);
		char *ls_buf = (char *)NULL;
		int i = 0;

		ls_buf = (char *)xnmalloc(linec_len + 1, sizeof(char));

		while (filecolors[i]) {

			int rem = 0;

			if ((int)i < (int)(linec_len - 3) && ((filecolors[i] == 'n'
			&& (filecolors[i + 1] == 'd' || filecolors[i + 1] == 'e'
			|| filecolors[i + 1] == 'f')) || (filecolors[i] == 'e'
			&& (filecolors[i + 1] == 'd' || filecolors[i + 1] == 'f'
			|| filecolors[i + 1] == 'e')) || (filecolors[i] == 'u'
			&& filecolors[i + 1] == 'f') || (filecolors[i] == 'c'
			&& filecolors[i + 1] == 'a')) && filecolors[i + 2] == '=') {

				/* If one of the above is found, move to the next
				 * color code */
				rem = 1;
				for (i += 3; filecolors[i] && filecolors[i] != ':'; i++)
					;
			}

			if (filecolors[i]) {

				if (!rem)
					ls_buf[buflen++] = filecolors[i];
			}

			else
				break;

			i++;
		}

		if (buflen) {
			ls_buf[buflen] = '\0';

			if (setenv("LS_COLORS", ls_buf, 1) == -1)
				fprintf(stderr, _("%s: Error registering environment "
						"colors\n"), PROGRAM_NAME);
			free(ls_buf);
			ls_buf = (char *)NULL;
		}

		/* Split the colors line into substrings (one per color) */
		char *p = filecolors, *buf = (char *)NULL, **colors = (char **)NULL;
		size_t len = 0, words = 0;
		int eol = 0;

		while (!eol) {
			switch (*p) {

			case '\0': /* fallthrough */
			case '\n': /* fallthrough */
			case ':':
				buf[len] = '\0';
				colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
				colors[words++] = savestring(buf, len);
				*buf = '\0';

				if (!*p)
					eol = 1;

				len = 0;
				p++;
				break;

			default:
				buf = (char *)xrealloc(buf, (len + 2) * sizeof(char));
				buf[len++] = *(p++);
				break;
			}
		}

		p = (char *)NULL;
		free(filecolors);
		filecolors = (char *)NULL;

		if (buf) {
			free(buf);
			buf = (char *)NULL;
		}

		if (colors) {
			colors = (char **)xrealloc(colors, (words + 1) * sizeof(char *));
			colors[words] = (char *)NULL;
		}

		/* Set the color variables */
		i = (int)words;
		while (--i >= 0) {

			if (*colors[i] == 'd' && strncmp(colors[i], "di=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*di_c = '\0';
				else
					snprintf(di_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'd' && strncmp(colors[i], "df=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*df_c = '\0';
				else
					snprintf(df_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'd' && strncmp(colors[i], "dc=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*dc_c = '\0';
				else
					snprintf(dc_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'w' && strncmp(colors[i], "wc=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*wc_c = '\0';
				else
					snprintf(wc_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'd' && strncmp(colors[i], "dh=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*dh_c = '\0';
				else
					snprintf(dh_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			else if (*colors[i] == 'n' && strncmp(colors[i], "nd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*nd_c = '\0';
				else
					snprintf(nd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'e' && strncmp(colors[i], "ed=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ed_c = '\0';
				else
					snprintf(ed_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'n' && strncmp(colors[i], "ne=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ne_c = '\0';
				else
					snprintf(ne_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'f' && strncmp(colors[i], "fi=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*fi_c = '\0';
				else
					snprintf(fi_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'e' && strncmp(colors[i], "ef=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ef_c = '\0';
				else
					snprintf(ef_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'n' && strncmp(colors[i], "nf=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*nf_c = '\0';
				else
					snprintf(nf_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'l' && strncmp(colors[i], "ln=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ln_c = '\0';
				else
					snprintf(ln_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'o' && strncmp(colors[i], "or=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*or_c = '\0';
				else
					snprintf(or_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'e' && strncmp(colors[i], "ex=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ex_c = '\0';
				else
					snprintf(ex_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'e' && strncmp(colors[i], "ee=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ee_c = '\0';
				else
					snprintf(ee_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'b' && strncmp(colors[i], "bd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*bd_c = '\0';
				else
					snprintf(bd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'c' && strncmp(colors[i], "cd=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*cd_c = '\0';
				else
					snprintf(cd_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'p' && strncmp(colors[i], "pi=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*pi_c = '\0';
				else
					snprintf(pi_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 's' && strncmp(colors[i], "so=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*so_c = '\0';
				else
					snprintf(so_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 's' && strncmp(colors[i], "su=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*su_c = '\0';
				else
					snprintf(su_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 's' && strncmp(colors[i], "sg=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*sg_c = '\0';
				else
					snprintf(sg_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 't' && strncmp(colors[i], "tw=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*tw_c = '\0';
				else
					snprintf(tw_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 's' && strncmp(colors[i], "st=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*st_c = '\0';
				else
					snprintf(st_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'o' && strncmp(colors[i], "ow=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ow_c = '\0';
				else
					snprintf(ow_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'c' && strncmp(colors[i], "ca=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*ca_c = '\0';
				else
					snprintf(ca_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'n' && strncmp(colors[i], "no=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*no_c = '\0';
				else
					snprintf(no_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'm' && strncmp(colors[i], "mh=", 3) == 0)
				if (!is_color_code(colors[i] + 3))
					*mh_c = '\0';
				else
					snprintf(mh_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);

			else if (*colors[i] == 'u' && strncmp(colors[i], "uf=", 3) == 0) {
				if (!is_color_code(colors[i] + 3))
					*uf_c = '\0';
				else
					snprintf(uf_c, MAX_COLOR - 1, "\x1b[%sm", colors[i] + 3);
			}

			free(colors[i]);
		}

		free(colors);
		colors = (char **)NULL;
	}

	/* If some color was not set or it was a wrong color code, set the
	 * default */
	if (!*el_c)
		strcpy(el_c, DEF_EL_C);
	if (!*mi_c)
		strcpy(mi_c, DEF_MI_C);
	if (!*dl_c)
		strcpy(dl_c, DEF_DL_C);
	if (!*df_c)
		strcpy(df_c, DEF_DF_C);
	if (!*dc_c)
		strcpy(dc_c, DEF_DC_C);
	if (!*wc_c)
		strcpy(wc_c, DEF_WC_C);
	if (!*dh_c)
		strcpy(dh_c, DEF_DH_C);
	if (!*tx_c)
		strcpy(tx_c, DEF_TX_C);
	if (!*li_c)
		strcpy(li_c, DEF_LI_C);
	if (!*ti_c)
		strcpy(ti_c, DEF_TI_C);
	if (!*em_c)
		strcpy(em_c, DEF_EM_C);
	if (!*wm_c)
		strcpy(wm_c, DEF_WM_C);
	if (!*nm_c)
		strcpy(nm_c, DEF_NM_C);
	if (!*si_c)
		strcpy(si_c, DEF_SI_C);
	if (!*bm_c)
		strcpy(bm_c, DEF_BM_C);

	if (!*di_c)
		strcpy(di_c, DEF_DI_C);
	if (!*nd_c)
		strcpy(nd_c, DEF_ND_C);
	if (!*ed_c)
		strcpy(ed_c, DEF_ED_C);
	if (!*ne_c)
		strcpy(ne_c, DEF_NE_C);
	if (!*fi_c)
		strcpy(fi_c, DEF_FI_C);
	if (!*ef_c)
		strcpy(ef_c, DEF_EF_C);
	if (!*nf_c)
		strcpy(nf_c, DEF_NF_C);
	if (!*ln_c)
		strcpy(ln_c, DEF_LN_C);
	if (!*or_c)
		strcpy(or_c, DEF_OR_C);
	if (!*pi_c)
		strcpy(pi_c, DEF_PI_C);
	if (!*so_c)
		strcpy(so_c, DEF_SO_C);
	if (!*bd_c)
		strcpy(bd_c, DEF_BD_C);
	if (!*cd_c)
		strcpy(cd_c, DEF_CD_C);
	if (!*su_c)
		strcpy(su_c, DEF_SU_C);
	if (!*sg_c)
		strcpy(sg_c, DEF_SG_C);
	if (!*st_c)
		strcpy(st_c, DEF_ST_C);
	if (!*tw_c)
		strcpy(tw_c, DEF_TW_C);
	if (!*ow_c)
		strcpy(ow_c, DEF_OW_C);
	if (!*ex_c)
		strcpy(ex_c, DEF_EX_C);
	if (!*ee_c)
		strcpy(ee_c, DEF_EE_C);
	if (!*ca_c)
		strcpy(ca_c, DEF_CA_C);
	if (!*no_c)
		strcpy(no_c, DEF_NO_C);
	if (!*uf_c)
		strcpy(uf_c, DEF_UF_C);
	if (!*mh_c)
		strcpy(mh_c, DEF_MH_C);

	if (!*dir_ico_c)
		strcpy(dir_ico_c, DEF_DIR_ICO_C);

	return EXIT_SUCCESS;
}

/* Print ENTRY using color codes and I as ELN, right padding PAD
 * chars and terminating ENTRY with or without a new line char (NEW_LINE
 * 1 or 0 respectivelly) */
void
colors_list(const char *ent, const int i, const int pad, const int new_line)
{
	size_t i_digits = (size_t)DIGINUM(i);

	/* Num (i) + space + null byte */
	char *index = (char *)xnmalloc(i_digits + 2, sizeof(char));

	if (i > 0) /* When listing files in CWD */
		sprintf(index, "%d ", i);

	else if (i == -1) /* ELN for entry could not be found */
		sprintf(index, "? ");

	/* When listing files NOT in CWD (called from search function and
	 * first argument is a path: "/search_str /path") 'i' is zero. In
	 * this case, no index should be printed at all */
	else
		index[0] = '\0';

	struct stat file_attrib;

	if (lstat(ent, &file_attrib) == -1) {
		fprintf(stderr, "%s%s%s%s%-*s%s%s", el_c, index, df_c,
		    uf_c, pad, ent, df_c, new_line ? "\n" : "");
		free(index);
		return;
	}

	char *linkname = (char *)NULL;
	char ext_color[MAX_COLOR] = "";
	char *color = fi_c;

#ifdef _LINUX_CAP
	cap_t cap;
#endif

	switch (file_attrib.st_mode & S_IFMT) {

	case S_IFREG:

		if (access(ent, R_OK) == -1)
			color = nf_c;

		else if (file_attrib.st_mode & S_ISUID) /* set uid file */
			color = su_c;

		else if (file_attrib.st_mode & S_ISGID) /* set gid file */
			color = sg_c;

		else {
#ifdef _LINUX_CAP
			cap = cap_get_file(ent);

			if (cap) {
				color = ca_c;
				cap_free(cap);
			}

			else if (file_attrib.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
#else

			if (file_attrib.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
#endif
				if (file_attrib.st_size == 0)
					color = ee_c;
				else
					color = ex_c;
			}

			else if (file_attrib.st_size == 0)
				color = ef_c;

			else if (file_attrib.st_nlink > 1)
				color = mh_c;

			else {
				char *ext = (strrchr(ent, '.'));

				if (ext) {
					char *extcolor = get_ext_color(ext);

					if (extcolor) {
						snprintf(ext_color, MAX_COLOR, "\x1b[%sm",
						    extcolor);
						color = ext_color;
						extcolor = (char *)NULL;
					}

					ext = (char *)NULL;
				}
			}
		}

		break;

	case S_IFDIR:

		if (access(ent, R_OK | X_OK) != 0)
			color = nd_c;

		else {
			int is_oth_w = 0;

			if (file_attrib.st_mode & S_IWOTH)
				is_oth_w = 1;

			int files_dir = count_dir(ent);

			color = (file_attrib.st_mode & S_ISVTX) ? (is_oth_w
					? tw_c : st_c) : (is_oth_w ? ow_c :
					/* If folder is empty, it contains only "."
					 * and ".." (2 elements). If not mounted (ex:
					 * /media/usb) the result will be zero. */
					(files_dir == 2 || files_dir == 0) ? ed_c : di_c);
		}

		break;

	case S_IFLNK:
		linkname = realpath(ent, (char *)NULL);

		if (linkname) {
			color = ln_c;
			free(linkname);
		}

		else
			color = or_c;

		break;

	case S_IFIFO:
		color = pi_c;
		break;
	case S_IFBLK:
		color = bd_c;
		break;
	case S_IFCHR:
		color = cd_c;
		break;
	case S_IFSOCK:
		color = so_c;
		break;

	/* In case all the above conditions are false... */
	default:
		color = no_c;
		break;
	}

	printf("%s%s%s%s%s%s%s%-*s", el_c, index, df_c, color,
	    ent, df_c, new_line ? "\n" : "", pad, "");

	free(index);
}

size_t
get_colorschemes(void)
{
	if (!COLORS_DIR)
		return 0;

	struct stat attr;

	if (stat(COLORS_DIR, &attr) == -1)
		return 0;

	int schemes_total = count_dir(COLORS_DIR);

	if (schemes_total <= 2)
		return 0;

	color_schemes = (char **)xcalloc((size_t)schemes_total + 2, sizeof(char *));

	size_t i = 0;

	DIR *dir_p;
	struct dirent *ent;

	/* count_dir already opened and read this directory succesfully,
	 * so that we don't need to check opendir for errors */
	dir_p = opendir(COLORS_DIR);

	while ((ent = readdir(dir_p)) != NULL) {

		/* Skipp . and .. */
		char *name = ent->d_name;

		if (*name == '.' && (!name[1] || (name[1] == '.' && !name[2])))
			continue;

		char *ret = strchr(name, '.');

		/* If the file contains not dot, or if its extension is not
		 * .cfm, or if it's just a hidden file named ".cfm", skip it */
		if (!ret || strcmp(ret, ".cfm") != 0 || ret == name)
			continue;

		*ret = '\0';

		color_schemes[i++] = savestring(name, strlen(name));
	}

	color_schemes[i] = (char *)NULL;

	closedir(dir_p);

	return i;
}

/* List color codes for file types used by the program */
void
color_codes(void)
{
	if (!colorize) {
		printf(_("%s: Currently running without colors\n"),
		    PROGRAM_NAME);
		return;
	}

	if (ext_colors_n)
		printf(_("%sFile type colors%s\n\n"), bold, df_c);

	printf(_(" %sfile name%s: Directory with no read permission (nd)\n"),
	    nd_c, df_c);
	printf(_(" %sfile name%s: File with no read permission (nf)\n"),
	    nf_c, df_c);
	printf(_(" %sfile name%s: Directory* (di)\n"), di_c, df_c);
	printf(_(" %sfile name%s: EMPTY directory (ed)\n"), ed_c, df_c);
	printf(_(" %sfile name%s: EMPTY directory with no read "
		 "permission (ne)\n"),
	    ne_c, df_c);
	printf(_(" %sfile name%s: Executable file (ex)\n"), ex_c, df_c);
	printf(_(" %sfile name%s: Empty executable file (ee)\n"), ee_c, df_c);
	printf(_(" %sfile name%s: Block special file (bd)\n"), bd_c, df_c);
	printf(_(" %sfile name%s: Symbolic link* (ln)\n"), ln_c, df_c);
	printf(_(" %sfile name%s: Broken symbolic link (or)\n"), or_c, df_c);
	printf(_(" %sfile name%s: Multi-hardlink (mh)\n"), mh_c, df_c);
	printf(_(" %sfile name%s: Socket file (so)\n"), so_c, df_c);
	printf(_(" %sfile name%s: Pipe or FIFO special file (pi)\n"), pi_c, df_c);
	printf(_(" %sfile name%s: Character special file (cd)\n"), cd_c, df_c);
	printf(_(" %sfile name%s: Regular file (fi)\n"), fi_c, df_c);
	printf(_(" %sfile name%s: Empty (zero-lenght) file (ef)\n"), ef_c, df_c);
	printf(_(" %sfile name%s: SUID file (su)\n"), su_c, df_c);
	printf(_(" %sfile name%s: SGID file (sg)\n"), sg_c, df_c);
	printf(_(" %sfile name%s: File with capabilities (ca)\n"), ca_c, df_c);
	printf(_(" %sfile name%s: Sticky and NOT other-writable "
		 "directory* (st)\n"),
	    st_c, df_c);
	printf(_(" %sfile name%s: Sticky and other-writable "
		 "directory* (tw)\n"),
	    tw_c, df_c);
	printf(_(" %sfile name%s: Other-writable and NOT sticky "
		 "directory* (ow)\n"),
	    ow_c, df_c);
	printf(_(" %sfile name%s: Unknown file type (no)\n"), no_c, df_c);
	printf(_(" %sfile name%s: Unaccessible (non-stat'able) file "
		 "(uf)\n"),
	    uf_c, df_c);

	printf(_("\n*The slash followed by a number (/xx) after directories "
		 "or symbolic links to directories indicates the amount of "
		 "files contained by the corresponding directory, excluding "
		 "self (.) and parent (..) directories.\n"));
	printf(_("\nThe value in parentheses is the code that is to be used "
		 "to modify the color of the corresponding filetype in the "
		 "color scheme file (in the \"FiletypeColors\" line), "
		 "using the same ANSI style color format used by dircolors. "
		 "By default, %s uses only 8 colors, but you can use 256 "
		 "and RGB colors as well.\n\n"), PROGRAM_NAME);

	if (ext_colors_n) {
		size_t i, j;

		printf(_("%sExtension colors%s\n\n"), bold, df_c);
		for (i = 0; i < ext_colors_n; i++) {
			char *ret = strrchr(ext_colors[i], '=');

			if (!ret)
				continue;

			printf(" \x1b[%sm", ret + 1);

			for (j = 0; ext_colors[i][j] != '='; j++)
				putchar(ext_colors[i][j]);

			puts("\x1b[0m");
		}

		putchar('\n');
	}
}
