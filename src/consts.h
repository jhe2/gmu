#ifndef WEJ_CONSTS_H
#define WEJ_CONSTS_H

/* Total maximum length for a file path */
#define PATH_LEN_MAX 512
/* Maximum legth of the filename part of a file path */
#define PATH_LEN_FILENAME_MAX 255
/* Maximum length of the directory part of a file path */
#define PATH_LEN_DIR_MAX 255

#ifdef URL_WITH_CURL
#define IS_URL(path) (strncasecmp((path), "http://", 7) == 0 || strncasecmp((path), "https://", 8) == 0)
#else
#define IS_URL(path) (strncasecmp((path), "http://", 7) == 0)
#endif
#endif
