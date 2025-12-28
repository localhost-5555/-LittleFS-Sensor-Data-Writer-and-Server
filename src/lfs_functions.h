#ifndef LFS_FUNCTIONS_H
#define LFS_FUNCTIONS_H

// This header declares what functions are available
void listDir(const char *dirname);
void readFile(const char *path);
void writeFile(const char *path, const char *message);
void renameFile(const char *path1, const char *path2);
void deleteFile(const char *path);
void appendFile(const char *path, const char *message);

#endif
