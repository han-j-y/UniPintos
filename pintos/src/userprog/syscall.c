#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "pagedir.h"

bool safe_ptr (void*);
struct file* fe[255]={0,};

static void syscall_handler (struct intr_frame *);
int fdc = 2;

	void
syscall_init (void) 
{
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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
int sys_create(char* file, off_t size);
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
		int *arg = esp+1;;
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
				break;
			case SYS_WAIT:
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
	printf ("%s: exit(%d)\n", thread_current()->name, status);
	thread_exit();
}
int sys_seek(int fd, off_t pos)
{
	struct file *fp = fe[fd] ;
	file_seek(fp, pos);
	return 0;
}
int sys_tell(int fd)
{
	int rsize;
	struct file *fp = fe[fd] ;
	if ((rsize = file_tell(fp)) <= 0)
		return -1;
	else
		return rsize;
}
int sys_close(int fd)
{
	struct file *fp = fe[fd] ;
	if(fp!=NULL)
	{
		file_close(fp);
		fe[fd]=NULL;
		return  true;
	}
	else
		return false;
}
int sys_open(char *file)
{
	struct file *fp;
	if (file[0] == 0)
		return -1;
	else{
		fp = filesys_open(file);
		if(fp==NULL)
			return -1;
		else
		{
			fe[fdc] = fp;
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
	if(fd<2) return -1;
	struct file *fp = fe[fd];
	if(fp==NULL) return -1;
	int rsize = file_read(fp, buffer, size);;
	if (rsize < 0)
		return -1;
	else
		return rsize;
}

int sys_create (char *file, off_t size)
{

	if (file[0] == '\0')
		return 0;
	else
	{
		bool suc = filesys_create(file, size);
		return suc;
	}

}

int sys_write(int fd, const void* buffer, off_t size)
{
	struct file* fp = fe[fd];
	if(fp==NULL) return -1;
	int wsize = file_write(fp, buffer, size);
	if(wsize < 0)
		return -1;
	else
		return wsize;
}
int sys_remove(char *file)
{
	if (file[0] == '\0')
		return 0;
	else
	{
		int suc = filesys_remove (file);
		return suc;
	}
}
