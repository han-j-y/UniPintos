#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);
int fd = 1;

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

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	void *esp = f->esp;

	//Validation
	if (safe_ptr (esp))
	{
		int *arg;
		int *syscall_number = esp;
	
		switch (*syscall_number)
		{
		case SYS_EXIT:
			{
				arg = esp+4;
				if (safe_ptr (arg))
					exit (*(arg));
				break;
			}
		
		case SYS_WRITE:
			{
				arg = esp+4;
				if (safe_ptr (arg+1)&& safe_ptr (arg+2))
				   putbuf (*(arg+1), *(arg+2));
				//write (*(arg), *(arg+1), *(arg+2));
				break;
			} 

		case SYS_HALT:
			break;
		case SYS_EXEC:
			break;
		case SYS_WAIT:
			
			break;
		case SYS_CREATE: // const char *file, unsigned initial_size
			{
				char** file = esp+4;
				unsigned int* size = esp+8;

				if ( safe_ptr(file) && safe_ptr (size) )
				{
					if (*file[0] == '\0')
						f->eax = 0;
					else
					{
						int suc = filesys_create (*file, *size);
						f->eax = suc;
					}
				}
				break;
			}
		case SYS_REMOVE:
			break;
		case SYS_OPEN:
			{
				char** file = esp+4;
				if (safe_ptr (file))
				{
					if (filesys_open (*file) == NULL)
					    f->eax = -1;
					else
						f->eax = fd++;
				}
				break;
			}
		case SYS_FILESIZE:
			break;
		case SYS_READ:
			break;
		case SYS_SEEK:
			break;
		case SYS_TELL:
			break;
		case SYS_CLOSE:
			{
				break;
			}
		}
	}
}

void exit (int status)
{
	printf ("%s: exit(%d)\n", thread_current()->name, status);
	thread_exit();
}