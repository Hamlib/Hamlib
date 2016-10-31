/*
 *   ltdl.h - ltdl emulation for android
 *
 *   Copyright (C) 2012 Ladislav Vaiz <ok1zia@nagano.cz>
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

typedef	void *lt_dlhandle;

int	lt_dlinit(void);
int	lt_dlexit(void);
int	lt_dladdsearchdir(const char *search_dir);
lt_dlhandle lt_dlopen(const char *filename);
lt_dlhandle lt_dlopenext(const char *filename);
int lt_dlclose(lt_dlhandle handle);
void *lt_dlsym(lt_dlhandle handle, const char *name);
const char *lt_dlerror(void);
