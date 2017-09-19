#include "vm/page.h"
#include "threads/malloc.h"

void add_supp_page(struct suppl_page *page) {
	hash_insert(&(thread_current()->page_table), &page->hash_elem);
	if (page->frame_sourcefile != NULL) {
		struct suppl_page *lpage = (struct suppl_page*) malloc(
					sizeof(struct suppl_page));
		if (lpage == NULL) {
		}
			lpage->upage = page->upage;
			struct hash_elem *e = hash_find(&(thread_current()->page_table),
					&lpage->hash_elem);
			if (e == NULL) {
			} else {
				struct suppl_page * f = hash_entry(e, struct suppl_page, hash_elem);
						if (f == NULL) {
									}
			}
			free(lpage);
	}
}

struct suppl_page* new_file_page(void *upage, bool writable,
		struct file *filename, off_t file_offset, size_t length, bool writable2) {
	struct file_info * fil = (struct file_info*) malloc(
			sizeof(struct file_info));
	fil->file_offset = file_offset;
	fil->filename = filename;
	fil->content_length = length;
	fil->writable = writable;
	struct suppl_page * spage = (struct suppl_page*) malloc(
			sizeof(struct suppl_page));
	spage->frame_sourcefile = fil;
	spage->upage = upage;
	spage->writable = writable2;
	spage->zeropage = NULL;
	spage->swap_slot = NULL;
	return spage;
}

struct suppl_page* new_swap_page(void *upage, bool writable,
		struct frame * frame, block_sector_t swap_addr) {
	struct swap_info * swp = (struct swap_info*) malloc(
			sizeof(struct swap_info));
	swp->frame = frame;
	swp->swap_addr = swap_addr;
	struct suppl_page * spage = (struct suppl_page*) malloc(
			sizeof(struct suppl_page));
	spage->frame_sourcefile = NULL;
	spage->upage = upage;
	spage->writable = writable;
	spage->zeropage = NULL;
	spage->swap_slot = swp;
	return spage;
}

struct suppl_page* new_zero_page(void *upage, bool writable) {
	struct zero_page_info * zeros = (struct zero_page_info*) malloc(
			sizeof(struct zero_page_info));
	struct suppl_page * spage = (struct suppl_page*) malloc(
			sizeof(struct suppl_page));
	spage->frame_sourcefile = NULL;
	spage->upage = upage;
	spage->writable = writable;
	spage->zeropage = zeros;
	spage->swap_slot = NULL;
	return spage;
}

unsigned hash_func(const struct hash_elem *e, UNUSED void *aux) {
	const struct suppl_page *page = hash_entry(e, struct suppl_page, hash_elem);
	return hash_int((int) (page->upage));
}

bool hash_less(const struct hash_elem *a, const struct hash_elem *b,
UNUSED void *aux) {
	const struct suppl_page *page_a = hash_entry(a, struct suppl_page,
			hash_elem);
	const struct suppl_page *page_b = hash_entry(b, struct suppl_page,
			hash_elem);
	return page_a->upage < page_b->upage;
}

void page_init(struct hash *page_table) {
	hash_init(page_table, &hash_func, &hash_less, NULL);
}
