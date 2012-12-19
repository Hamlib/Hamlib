/*
    ltdl.h - ltdl emulation for android
    Copyright (C) 2012 Ladislav Vaiz <ok1zia@nagano.cz>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

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
