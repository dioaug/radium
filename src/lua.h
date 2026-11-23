#pragma once

void rlua_init(const char *config); /* NULL for ~/.config/radium/init.lua */
void rlua_startup(void);
void rlua_cleanup(void);
