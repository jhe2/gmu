/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2014 Johannes Heimansberg (wejp.k.vu)
 *
 * File: dirparser.h  Created: 130702
 *
 * Description: Directory parser
 */
#ifndef WEJ_DIRPARSER_H
#define WEJ_DIRPARSER_H

int dirparser_walk_through_directory_tree(const char *directory, int (fn(void *arg, const char *filename)), void *arg);
#endif
