/* init.h */

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

#pragma once

void check_env_filter(void);
void get_prompt_cmds(void);
void get_aliases(void);
int get_last_path(void);
void check_options(void);
int load_dirhist(void);
size_t get_path_env(void);
void get_prompt_cmds(void);
int get_sel_files(void);
void init_shell(void);
void load_jumpdb(void);
int load_bookmarks(void);
int load_actions(void);
int load_pinned_dir(void);
void get_path_programs(void);
void unset_xargs(void);
void external_arguments(int argc, char **argv);
char *get_date(void);
pid_t get_own_pid(void);
struct user_t get_user(void);

/* 
 * some useful structs
*/

/* stores information regarding the user */
struct user_t {
	char *home;
	size_t home_len;
	char *name;
	char *shell;
};

/*
 * some global variables
*/
extern struct user_t user;
