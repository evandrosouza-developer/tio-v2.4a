/*
 * tio - a simple TTY terminal I/O application
 *
 * Copyright (c) 2022  Evandro Souza
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef EXTENSION_H
#define EXTENSION_H


#define ENABLE_SEND_FILE					true
#define ENABLE_PARALLEL_KEYBOARD	true
#define TIO_ABORT	0x1B

void check_input_kb_event(char input_char, char*mount_string);
bool load_key_matrix(void);
void file_send(void);

#endif	//#ifndef UPGRADES_H
