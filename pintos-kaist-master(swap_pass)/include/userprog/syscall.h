#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "lib/user/syscall.h"
void syscall_init (void);
struct file* get_file(int fd);
int add_file(struct file *f);
void close_file(int fd);
struct thread* getchild(pid_t pid);
void halt(void);
void checkaddress(void *cad);
void exit (int status);
pid_t fork (const char *thread_name);
int exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove(const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);
#endif /* userprog/syscall.h */
