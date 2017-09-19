#include "./syscall.h"
#include "../threads/interrupt.h"
#include "../threads/vaddr.h"
#include "../lib/debug.h"
#include "../threads/thread.h"
#include "../devices/shutdown.h"
#include "../lib/stdio.h"
#include "../lib/syscall-nr.h"
#include "../lib/kernel/list.h"
#include "process.h"
#include "pagedir.h"
#include "../filesys/filesys.h"
#include "../devices/input.h"
#include "../filesys/file.h"
#include "../lib/kernel/stdio.h"
#include "threads/pte.h"

void lock_filesystem(void) {
	if (!lock_held_by_current_thread(&filesys_lock))
		lock_acquire(&filesys_lock);
}

void release_filesystem(void) {
	if (lock_held_by_current_thread(&filesys_lock))
		lock_release(&filesys_lock);
}

typedef int pid_t;

int argu_num = 0;

static void syscall_handler(struct intr_frame *);

void syscall_init(void) {
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
	lock_init(&filesys_lock);
}

void * check_accessing_user_memory_using_intrframe(struct intr_frame * f) {
	if (f == NULL) {
		exit(-1);
		return NULL;
	}
	if (is_user_vaddr(f->esp + argu_num)) {
		if (pagedir_get_page(thread_current()->pagedir,
				f->esp + argu_num) != NULL) {
			return pagedir_get_page(thread_current()->pagedir,
					f->esp + argu_num);
		} else {
			exit(-1);
			return NULL;
		}
	} else {
		exit(-1);
		return NULL;
	}
}

void * check_accessing_user_memory(void *esp, struct intr_frame * f) {
	if (is_user_vaddr(esp)) {
		if (pagedir_get_page(thread_current()->pagedir, esp) != NULL) {
			return pagedir_get_page(thread_current()->pagedir, esp);
		} else {
			exit(-1);
			return NULL;
		}
	} else {
		exit(-1);
		return NULL;
	}
}

void memory_is_good(void * esp) {
	if (is_user_vaddr(esp)) {
		if (pagedir_get_page(thread_current()->pagedir, esp) != NULL) {
		} else {
			exit(-1);
		}
	} else {
		exit(-1);
	}
}

bool memory_writable(const void *p) // Check if the page has write access. If cannot locate the page in the page table, ignore the test and return true.
{
	if (p == NULL) {
				return false;
	}

	if (is_reserved(p)) {
				return false;
	}

	void *page_addr = pg_round_down(p);
	struct suppl_page *page = (struct suppl_page *) malloc(sizeof(struct suppl_page));
	page->upage = page_addr;
	struct hash_elem *elem = hash_find(&thread_current()->page_table,
			&page->hash_elem);
	free(page);
	if (elem == NULL) {
		return true;
	} // can't find page

	struct suppl_page *page_entry = hash_entry(elem, struct suppl_page,
			hash_elem);
	return page_entry->writable;
}

void* get_argument(struct intr_frame *f) {
	// check memory
	void *r = check_accessing_user_memory_using_intrframe(f);
	argu_num += sizeof(int);
	return r;
}

static void syscall_handler(struct intr_frame *f UNUSED) {
	argu_num = 0;
	uint32_t syscall_id = *(int*) get_argument(f);
	switch (syscall_id) {
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		f->eax = exit(*((int *) get_argument(f)));
		break;
	case SYS_EXEC: {
		char * real_buffer = (char *) check_accessing_user_memory(
				*(void **) get_argument(f), f);
		f->eax = exec(real_buffer);
	}
		break;
	case SYS_WAIT:
		f->eax = wait(*(pid_t *) get_argument(f));
		break;
	case SYS_CREATE: {
		void * fn = get_argument(f);
		if (*(void **) fn == NULL) {
			exit(-1);
		}
		char * real_file = (char *) check_accessing_user_memory(*(void **) fn,
				f);
		unsigned size = *((unsigned *) get_argument(f));
		f->eax = creat_file(real_file, size);
	}
		break;
	case SYS_REMOVE: {
		char * real_file = (char *) check_accessing_user_memory(
				*(void **) get_argument(f), f);
		f->eax = remove(real_file);
	}
		break;
	case SYS_OPEN: {
		char * real_file = (char *) check_accessing_user_memory(
				*(void **) get_argument(f), f);
		f->eax = open(real_file);
	}
		break;
	case SYS_FILESIZE:
		f->eax = filesize(*((int *) get_argument(f)));
		break;
	case SYS_READ: {
		int fd = *(int *) get_argument(f);
		char * real_buffer = (char *) check_accessing_user_memory(
				*(void **) get_argument(f), f);
		unsigned size = *(unsigned *) get_argument(f);
		f->eax = read(fd, real_buffer, size);
	}
		break;
	case SYS_WRITE: {
		int fd = *(int *) get_argument(f);
		void *fn = get_argument(f);
		unsigned size = *(unsigned *) get_argument(f);
		char * real_buffer = (char *) check_accessing_user_memory(*(void **) fn,
				f);
		f->eax = write(fd, real_buffer, size);
	}
		break;
	case SYS_SEEK: {
		int h = *(int *) get_argument(f);
		unsigned off = *(unsigned *) get_argument(f);
		seek(h, off);
	}
		break;
	case SYS_TELL:
		f->eax = tell(*((int *) get_argument(f)));
		break;
	case SYS_CLOSE:
		close(*(int *) get_argument(f));
		break;
	case SYS_MMAP: {
		int mmap_fd = *(int *) get_argument(f);
		char * realaddr = (char *) check_accessing_user_memory(
				*(void **) get_argument(f), f);
		f->eax = mmap(mmap_fd, realaddr);
	}
		break;
	case SYS_MUNMAP: {
		int mmap_fd2 = *(int *) get_argument(f);
		munmap(mmap_fd2);
	}
		break;
	default:
		printf("NOT REACCHED %i\n", syscall_id);
		NOT_REACHED ()
		;
	}
}

void halt() {
	shutdown_power_off();
}

int exit(int status) {
	thread_current()->exit_status = status;
	// unmap before close
	printf("%s: exit(%d)\n", thread_current()->name, status);
	thread_exit();
	return status;

}

pid_t exec(const char *cmd_line) {
	lock_filesystem();
	tid_t id = process_execute(cmd_line);
	release_filesystem();
	return id;
}

int wait(pid_t pid) {
	if (pid <= 0) {
		return -1;
	}
	int r = process_wait(pid);
	return r;
}

int creat_file(void *file, unsigned initial_size) {
	lock_filesystem();
	int filesys_cr = filesys_create(file, initial_size) ? 1 : 0;
	release_filesystem();
	return filesys_cr;
}

int remove(void *file) {
	lock_filesystem();
	int rem = filesys_remove(file);
	release_filesystem();
	return rem;
}

int open(void *file) {
	lock_filesystem();
	struct file* newfile = filesys_open(file);
	release_filesystem();
	if (newfile == NULL) {
		return -1;
	}
	int ret = add_file_to_thread(newfile);
	return ret;
}

int filesize(int fd) {
	struct thread *t = thread_current();
	struct file *f = get_file_from_fd(fd);
	if (f == NULL)
		exit(-1);
	return file_length(f);
}

int read(int fd, void *buffer, unsigned size) {
	if (!memory_writable(buffer)) {
		exit(-1);
	}
	struct thread * t = thread_current();
	if (fd == STDOUT_FILENO) {
		exit(-1);
		return -1;
		//ERROR, try to read from std out
	} else if (fd == STDIN_FILENO) { // read from key board
		int siz = 0;
		lock_filesystem();
		for (siz = 0; siz < size; siz++) {
			*(uint8_t*) (buffer + size) = input_getc();
		}
		release_filesystem();
		return size;
	} else {
		lock_filesystem();
		if (list_empty(&t->file_fd_list)) {
			release_filesystem();
			exit(-1);
			return -1;
		}
		struct file* f = get_file_from_fd(fd);
		if (f != NULL) {
			int l = file_length(f);
			int size_read = (int) file_read(f, buffer, size);
			if (size_read == size) {
				release_filesystem();
				return size_read;
			} else if (size_read == file_length(f) && size_read != size) {
				release_filesystem();
				return 0;
			} else {
				release_filesystem();
				exit(-1);
				return -1;
			}
		} else {
			release_filesystem();
			exit(-1);
			return -1;
		}
	}
}

int write(int fd, const void *buffer, unsigned size) {
	if (!memory_writable(buffer)) {
			exit(-1);
		}
	struct thread * t = thread_current();
	if (fd == STDIN_FILENO) {
		exit(-1);
		return -1;
	} else if (fd == STDOUT_FILENO) { // to console
		lock_filesystem();
		int written = 0;
		if (size < 200) {
			putbuf(buffer, size);
			written = size;
		} else {
			while (size > 200) {
				putbuf((buffer + written), 200);
				size -= 200;
				written += 200;
			}
			putbuf((buffer + written), size);
			written += size;
		}
		release_filesystem();
		return written;
	} else {
		if (list_empty(&t->file_fd_list)) {
			exit(-1);
			return -1;
		}
		struct file* f = get_file_from_fd(fd);
		if (f != NULL) {
			lock_filesystem();
			int size_wrote = file_write(f, buffer, size);
			release_filesystem();
			return size_wrote;
		} else {
			exit(-1);
			return -1;
		}
	}
}

void seek(int fd, unsigned position) {
	struct thread * t = thread_current();
	struct file * f = get_file_from_fd(fd);
	if (f == NULL) {
		exit(-1);
	}
	lock_filesystem();
	file_seek(f, position);
	release_filesystem();
}

int tell(int fd) {
	struct file * f = get_file_from_fd(fd);
	if (f == NULL)
		exit(-1); // not getting file
	lock_filesystem();
	off_t position = file_tell(f);
	release_filesystem();
	return position;
}

void close(int fd) {
	if (list_empty(&thread_current()->file_fd_list)) {
		exit(-1); // no file to close
	}
	struct file *f = get_file_from_fd(fd);
	if (f == NULL) {
		exit(-1); // not getting file
	} else {
		lock_filesystem();
		file_close(f);      //Close file in the system
		delete_file_from_thread(fd); // delete file in file list
		release_filesystem();
	}

}

// mapid_t mmap (int fd, void *addr)
int mmap(int fd, void *uaddr) { // maps a file with the fd to memory address uaddr
	struct thread * current = thread_current();
	if (fd == 0 || fd == 1) { // check fd
		return -1;
	}
	struct file_fd * fh = get_file_from_fd(fd); // get file handle from fd
	if (fh == NULL)
		exit(-1);

	int fillen = file_length(fh->fil); // get file length
	if (fillen == 0 || uaddr == 0 || (int) uaddr % PGSIZE > 0) { // file length empty or address is zero or not page-aligned
		return -1;
	}

	lock_filesystem();
	int mmap_fd = thread_add_mmap_file(file_reopen(fh->fil)); //add mmap file
	release_filesystem();

	struct file_fd * mmap_fh = get_file_from_fd_mmap(mmap_fd); // get mmap file handle

	mmap_fh->userPage = uaddr;
	int numPage = fillen / PGSIZE; // number of pages needed
	if (fillen % PGSIZE > 0) {
		numPage++;
	}

	int i;
	for (i = 0; i < numPage; i++) { // loop through pages needed to map
		size_t readLength = (i == numPage - 1) ? fillen % PGSIZE : PGSIZE;
		off_t offset = i * PGSIZE;
		int current_page_address = uaddr + offset;

		sema_down(&current->sema_pagedir);
		void * currentKernelAddress = pagedir_get_page(current->pagedir,
				current_page_address);
		sema_up(&current->sema_pagedir);

		if (currentKernelAddress != NULL) { // overlap with other data
			return -1;
		}
		void *kpage  = frame_get(uaddr,true,true);
		if (kpage==NULL) {
			exit(-1);
		}
		struct frame *fme = frame_find(kpage);
		fme->frame_sourcefile->content_length = readLength;
		fme->frame_sourcefile->file_offset = offset;
		fme->frame_sourcefile->filename = mmap_fh->fil;
		fme->frame_sourcefile->writable = true;
		add_supp_page(new_file_page(current_page_address,true,mmap_fh->fil,offset,readLength,true));
	}

	return mmap_fd;
}

// void munmap (mapid t mapping)
void munmap(int mmap_fd) {
	struct thread * current = thread_current();
	struct file_fd * fh = get_file_from_fd_mmap(mmap_fd);
	void * file_startaddr = fh->userPage; // take user virtual page
	int32_t fil_len = file_length(fh->fil); // take file length
	int page_num = fil_len / PGSIZE; // number of pages needed to contains the file
	if (fil_len % PGSIZE > 0) {
		page_num++;
	}
	int i = 0;
	for (i = 0; i < page_num; i++) { //loop though pages
		int offset = i * PGSIZE; // offset from file start address
		void * page_addr = file_startaddr + offset; // current user page address
		sema_down(&current->sema_pagedir);
		bool dirty = pagedir_is_dirty(current->pagedir, page_addr);
		void * kpage = pagedir_get_page(current->pagedir, page_addr); // get kernel memory address
		sema_up(&current->sema_pagedir);
		if ((pg_ofs(kpage) == 0) && dirty) {
			int write_length = (i == page_num - 1) ? fil_len % PGSIZE : PGSIZE; // get write back length
			lock_filesystem();
			file_seek(fh->fil, offset);
			release_filesystem();

			lock_frames();
			unlock_frames();

			lock_filesystem();
			file_write(fh->fil, page_addr, write_length); // write page back to file
			release_filesystem();

			lock_frames();
			unlock_frames();
		}
		struct suppl_page *p = (struct suppl_page *) malloc(sizeof(struct suppl_page));
		p->upage = page_addr;
		hash_delete(&current->page_table, hash_find(&current->page_table, &p->hash_elem));
		free(p);
		sema_down(&current->sema_pagedir);
		pagedir_clear_page(current->pagedir, page_addr);
		sema_up(&current->sema_pagedir);
	}
	list_remove(&fh->file_fd_list_elem);
	file_close(fh->fil);
	free(fh);
}
