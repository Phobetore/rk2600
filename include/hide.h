#ifndef HIDE_H
#define HIDE_H

int init_hide(void);
void cleanup_hide(void);

// Add file to hide list
void hide_add_file(const char *filename);

#endif
