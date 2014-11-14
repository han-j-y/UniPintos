#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "pagedir.h"

bool safe_ptr (void*);
struct file* fe[255]={0,};
static struct lock file_lock;

void file_lock_acquire ()
{
	if (!lock_held_by_current_thread(&file_lock))
		lock_acquire (&file_lock);
}

void file_lock_release ()
{
	if (lock_held_by_current_thread(&file_lock))
		lock_release (&file_lock);
}

static void syscall_handler (struct intr_frame *);
int fdc = 2;

	void
syscall_init (void) 
{
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
	lock_init (&file_lock);
}

bool safe_ptr (void* ptr)
{
	if ( ptr == NULL || !is_user_vaddr (ptr) || pagedir_get_page (thread_current()->pagedir, ptr) == NULL ) //unmapped
	{
		exit (-1);
		return false;
	}
	else 
		return true;
}

int sys_open(char*);
int sys_filesize(int fd);
int sys_tell(int fd);
int sys_close(int fd);
int sys_read(int fd, void* buffer, off_t size);
bool sys_create(char* file, off_t size);
int sys_seek(int fd, off_t pos);
int sys_write(int fd, const void* buffer, off_t size);
int sys_remove(char *file);


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	int *esp = f->esp;

	//Validation
	if (safe_ptr (esp))
	{
		int *arg=esp+1;
		int *syscall_number = esp;

		switch (*syscall_number)
		{
			case SYS_EXIT:
				{
					if (safe_ptr (arg))
						exit (*(arg));
					break;
				}

			case SYS_WRITE:
				if (safe_ptr(arg) && safe_ptr(arg+1) && safe_ptr(arg+2))
				{
					if(*(int*)arg==1)
					{
						putbuf(*(void**)(arg+1), *(off_t*)(arg+2));
					}
					else
						f->eax = sys_write(*(int*)(arg), *(void**)(arg+1), *(off_t*)(arg+2));
				}
				break;
			case SYS_HALT:
				break;
			case SYS_EXEC:
				if (safe_ptr (arg) && safe_ptr (*arg))
				{
					tid_t pid = process_execute (*arg);
					//termination
					f->eax = pid;
				}
				break;
			case SYS_WAIT:
				if (safe_ptr (arg))
				{
					int status = process_wait (*arg);
					f->eax = status;
				}
				break;
			case SYS_CREATE: // const char *file, unsigned initial_size
				if (safe_ptr(arg) && safe_ptr(arg+1) && safe_ptr(arg+2))
					f->eax = sys_create(*(char**)arg, *(off_t*)(arg+1));
				break;
			case SYS_REMOVE:
				if (safe_ptr(arg))
					f->eax = sys_remove(*(char**)arg);
				break;
			case SYS_OPEN:
				if (safe_ptr(arg))
					f->eax = sys_open(*((char**)(arg)));
				break;
			case SYS_FILESIZE:
				if (safe_ptr(arg))
					f->eax = sys_filesize(*(int *)(arg));
				break;
			case SYS_READ:
				if (safe_ptr(arg) && safe_ptr(arg+1) && safe_ptr(arg+2))
					f->eax = sys_read(*(int *)(arg), *(void**)(arg+1), *(off_t*)(arg+2));
				break;
			case SYS_SEEK:
				if (safe_ptr(arg) && safe_ptr(arg+1))
					f->eax = sys_seek(*(int*)arg, *(off_t*)(arg+1));
				break;
			case SYS_TELL:
				if (safe_ptr(arg))
					f->eax = sys_tell(*(int*)arg);
				break;
			case SYS_CLOSE:
				if (safe_ptr(arg))
					f->eax = sys_close(*(int*)arg);
				break;
		}
	}
}

void exit (int status)
{
	struct thread* tmp = get_thread(thread_current()->parent_tid);
	printf ("%s: exit(%d)\n", thread_current()->name, status);

	if (tmp != NULL)
	{
		sema_up (&tmp->wait);
		tmp->child_exit_status = status;
	}

	thread_exit();
}
int sys_seek(int fd, off_t pos)
{
	struct file *fp = fe[fd] ;
	file_lock_acquire();
	file_seek(fp, pos);
	file_lock_release();
	return 0;
}
int sys_tell(int fd)
{
	int rsize;
	struct file *fp = fe[fd] ;
	file_lock_acquire();
	rsize = file_tell(fp);
	file_lock_release();
	if (rsize <= 0)
		return -1;
	else
		return rsize;
}
int sys_close(int fd)
{
	struct file *fp = fe[fd];
	if(fp!=NULL)
	{
		file_lock_acquire();
		file_close(fp);
		file_lock_release();
		fe[fd]=NULL;
		return  true;
	}
	else
		return false;
}

int sys_open(char *file)
{
	struct file *fp;

	if (safe_ptr (file));

	if (file[0] == 0)
		return -1;
	else{
		file_lock_acquire();
		fp = filesys_open(file);
		
		if(fp==NULL)
		{
			file_lock_release();
			return -1;
		}
		else
		{
			fe[fdc] = fp;
			file_lock_release();
			return fdc++;
		}
	}
}

int sys_filesize(int fd)
{
	struct file *fp = fe[fd];
	int size;
	if ((size=file_length (fp)) <=0)
		return -1;
	else
		return size;
}

int sys_read(int fd, void* buffer, off_t size)
{
	if (safe_ptr (buffer));

	if(fd<2) return -1;
	struct file *fp = fe[fd];
	if(fp==NULL) return -1;
	file_lock_acquire();
	int rsize = file_read(fp, buffer, size);
	file_lock_release();
	if (rsize < 0)
		return -1;
	else
		return rsize;
}

bool sys_create (char *file, off_t size)
{
	if (safe_ptr (file));

	if (file[0] == '\0' || strlen(file) > 256)
		return 0;

	else
	{
		file_lock_acquire();
		bool suc = filesys_create(file, size);
		file_lock_release();
		return suc;
	}
}

int sys_write(int fd, const void* buffer, off_t size)
{
	if (safe_ptr (buffer));

	struct file* fp = fe[fd];
	if(fp==NULL) return -1;

	file_lock_acquire();
	int wsize = file_write(fp, buffer, size);
	file_lock_release();
	if(wsize < 0)
		return -1;
	else
		return wsize;
}

int sys_remove(char *file)
{
	if (safe_ptr (file));

	if (file[0] == '\0')
		return 0;
	else
	{
		file_lock_acquire();
		int suc = filesys_remove (file);
		file_lock_release();
		return suc;
	}
}
