#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);  

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	void *esp = f->esp;
	
	//Validation
	if (esp == NULL)
		thread_exit();


	int *arg;
	int *syscall_number = esp;
	
	switch (*syscall_number)
	{
	case SYS_EXIT:
		{
			arg = esp+4;
			printf ("%s: exit(%d)\n", thread_current()->name, *(arg));
			thread_exit();

			break;
		}
		
	case SYS_WRITE:
		{
			arg = esp+4;

			putbuf (*(arg+1), *(arg+2));
			//write (*(arg), *(arg+1), *(arg+2));
			break;
		} 

	case SYS_HALT:
	case SYS_EXEC:
	case SYS_WAIT:
	case SYS_CREATE: // const char *file, unsigned initial_size
		{
			char** file = esp+4;
			unsigned int* size = esp+8;

			if (*file[0] == '\0')
			printf ("\n%s\n", *file);

			//filesys_create (file, *size);
			break;
		}
	case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
    case SYS_READ:
    case SYS_SEEK:
    case SYS_TELL:
    case SYS_CLOSE:
		{
			break;
		}
	}
}