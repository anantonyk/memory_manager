#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

typedef int bool;
#define true 1
#define false 0

#define START_SEQ 0xDEADBEEF


typedef struct {
	int block_ident;
	size_t size;
} start_of_block;

struct free_block_ {
	struct free_block_* next;
};

typedef struct free_block_ free_block;

bool process_memory_init = false;
size_t process_memory_size = 100000;
void* MEM_BEGIN;
void*  MEM_END;
free_block* first_free_block;

void init_malloc() {
	printf("first init\n");
	MEM_BEGIN = malloc(process_memory_size);
	process_memory_init = true;
	first_free_block = (free_block*)MEM_BEGIN;
	MEM_END = MEM_BEGIN + process_memory_size - 1;
	process_memory_size--;
	first_free_block->next = MEM_END;
}

bool validate_empty_block(void*  ptr, size_t size) {
	if (ptr < MEM_BEGIN)
		return false;

	if (ptr + size >= MEM_END)
		return false;
	
	for (size_t i = 0; i < size; i++) {
		if (((start_of_block*)(ptr + i))->block_ident == START_SEQ) {
			//printf("valid empty block: find start sequence\n");
			return false;
		}	
	}
	return true;
}

int size_of_free_block(free_block * fb) {
	int size = 0;
	void* tmp = (void*) fb;
	while (tmp <= MEM_END) {
		//printf("size fb: calculate size step\n");
		if(((start_of_block*)tmp)->block_ident == START_SEQ)
			break;
		size++;
		tmp++;
	}
	printf("Size of free block : %d\n", size);
	return size;
}

void init_allocated_block(void* ptr, size_t block_size) {
	printf("allocate block: set block\n");
	//start_of_block* start = (start_of_block*)(&ptr);
	((start_of_block*)(ptr))->block_ident = START_SEQ;
	((start_of_block*)(ptr))->size = block_size - sizeof(start_of_block);
}

void* allocate_wOne_free_bl(size_t size_to_allocate){
	free_block * tmp = first_free_block;
	void* memory_location = (void*)(first_free_block + sizeof(start_of_block));

	if(size_to_allocate == size_of_free_block(first_free_block)) {
		first_free_block = MEM_END;
	} else {
		first_free_block = (free_block *)((void*)first_free_block + size_to_allocate);
		first_free_block->next = tmp->next;
	}
	init_allocated_block((void*)tmp, size_to_allocate);
	return memory_location;
}

void* allocate_between_some_free_blocks(free_block* previous_block, size_t size_to_allocate) {
	free_block* tmp = previous_block->next;
	free_block* tmp_next = tmp->next;
	void* memory_location = (void*)(tmp + sizeof(start_of_block));
	
	if(size_to_allocate == size_of_free_block(tmp)) {
		previous_block->next = tmp->next;
	} else {
		tmp = (free_block *) ((void*)tmp + size_to_allocate);
		previous_block->next = tmp;
		tmp->next = tmp_next;	
	}
	init_allocated_block((void*)tmp, size_to_allocate);
	return memory_location;
}

void* my_malloc(size_t size) {
	
	if (!process_memory_init) 
		init_malloc();

	if (size <= 0) 
		return NULL;

	size_t size_wStart_bl = size + sizeof(start_of_block);

	if (size_wStart_bl >= process_memory_size) 
		return NULL;

	void* memory_location;
	/*
	 * proceed through linked list of the empty blocks
	 * until found a valid empty block (validate_empty_block)
	 * or until the last link in the list
	 */
	//validate first free block
	if (first_free_block == MEM_END) {
		printf("malloc: ffb is null\n");
		return NULL;
	}
	
	
	//if it is one free block
	if (first_free_block->next == MEM_END) {
		printf("malloc: one fb\n");
		if (validate_empty_block(first_free_block, size_wStart_bl)) {
			printf("Malloc: valid f b\n");
			return allocate_wOne_free_bl(size_wStart_bl);
		} else {
			printf("Malloc: not valid f b\n");
			return NULL;
		}
			
	}
	//else check all next free block
	free_block* tmp_free_block = first_free_block;
	tmp_free_block->next = first_free_block->next;
	int counter = 0;
	int number = -1;
	int difference = 10000000;
	while (tmp_free_block->next != MEM_END) {
		if(validate_empty_block(tmp_free_block, size_wStart_bl)) {
			if(size_of_free_block(tmp_free_block) - size_wStart_bl < difference) {
				number = counter;
				
			}
			difference = size_of_free_block(tmp_free_block) - size_wStart_bl;
		}
		counter++;
		tmp_free_block = tmp_free_block->next;
	}
	if(number == 0) {		
		return allocate_wOne_free_bl(size_wStart_bl);
	}
	free_block* block_to_alloc = first_free_block;
	for (int i = 0; i < number - 1; i++) {
		block_to_alloc = block_to_alloc->next;
	}
	return allocate_between_some_free_blocks(block_to_alloc, size_wStart_bl);

}

void merge_diapazons(free_block* ptr) {
	// Validate ptr if not validated in free
	free_block *ptr_i = first_free_block;
	
	// Check if the next_ptr is after the given ptr or the next ptr is the end
	while (ptr_i->next != MEM_END) {
		if (ptr_i->next > ptr)
			break;
		ptr_i = ptr_i->next;
	}

	free_block* next_ptr = ptr_i->next;

	//we are on such pointers
   	 // ptr_i      ptr      next_ptr
	
	//check blocks for emptiness
	bool leftblock = validate_empty_block((void*)ptr_i, ptr - ptr_i);
	bool rightblock;
	if(next_ptr != MEM_END)
		rightblock = validate_empty_block((void*)ptr, next_ptr - ptr);
	
	//check if points to the end of available memory
	if (next_ptr == MEM_END) {
		printf("Merge: before the end\n");
		// add new start of free block
		if (!leftblock) {
			ptr_i->next = ptr;
			(ptr->next) = MEM_END;
			printf("left m\n");
		}
	} else {
		printf("Merge: not before end\n");
		if (rightblock) {
			//check the case with three sequential empty block
			if (leftblock) {
				printf("Merge: three empty blocks\n");
				ptr_i->next =(next_ptr->next);
			} else {
				printf("Merge: two empty blocks\n");
				(ptr->next) = (next_ptr->next);
				ptr_i->next = ptr;
			}
		} else if (!leftblock) {
			printf("Merge: no emptines around\n");
			(ptr_i->next) = ptr;
			(ptr->next) = next_ptr;
		}
	}
}


void my_free(void* ptr) {
	//validate ptr
	if (ptr == NULL) {
		printf("Free: NULL ptr\n");
		return;
	}
	
	if (ptr < MEM_BEGIN) {
		printf("Free: < begin\n");
		return;
	}
		

	if (ptr >= MEM_END) {
		printf("Free: > end\n");
		return;
	}
		

	//move back to the start of allocated block
	ptr -= sizeof(start_of_block);
	
	for (size_t i = 0; i < sizeof(start_of_block); i++) {
		((start_of_block*)(ptr + i))->block_ident = 0;
	} 
	
	free_block* new_free_block = (free_block*)ptr;
	if (new_free_block < first_free_block) {
		printf("free: nwb before ffb\n");
		new_free_block->next = first_free_block;
		new_free_block->next->next = first_free_block->next;
		first_free_block = new_free_block;
	}

	if (new_free_block > first_free_block) {
		if(first_free_block->next == MEM_END) {
			printf("free: nfb after ffb, before mem end\n");
			first_free_block = new_free_block;
			first_free_block->next = MEM_END;
		} else {
			printf("free: start merge\n");
			merge_diapazons(new_free_block);
		}
	}
}

int main() {
	
	void* ptr0[10];
	my_malloc(5);

	//using of malloc and free
	clock_t t0 = clock();
	for (int i = 0; i < 7; i++) {
		ptr0[i] = malloc(15000);
	}
	clock_t t1 = clock();
	for (int i = 0; i < 7; i++) {
		free(ptr0[i]);
	}
	clock_t t2 = clock();
	for (int i = 0; i < 10; i+=4) {
		ptr0[i] = malloc(10*i);
	}
	clock_t t3 = clock();
	double time_for_malloc = (double)(t1 - t0)/CLOCKS_PER_SEC;
	double time_for_free = (double)(t2 - t1)/CLOCKS_PER_SEC;
	double time_for_malloc2 = (double)(t3 - t2)/CLOCKS_PER_SEC;
	printf("malloc:\t\t%f\n", time_for_malloc);
	printf("free:\t\t%f\n", time_for_free);
	printf("malloc2:\t%f\n", time_for_malloc2);

	int* ptr[1000];

	//using of my malloc and my free
	clock_t t4 = clock();
	for (int i = 0; i < 1000; i+=1) {
		ptr[i] = my_malloc(15000);
	}
	clock_t t5 = clock();
	for (int i = 0; i < 1000; i+=2) {
		my_free(ptr[i]);
	}
	clock_t t6 = clock();
	for (int i = 0; i < 1000; i+=2) {
		ptr[i] = my_malloc(10*i);
	}
	clock_t t7 = clock();
	for (int i = 0; i < 1000; i+=4) {
		my_free(ptr[i]);
	}
	clock_t t8 = clock();
	for (int i = 0; i < 1000; i+=4) {
		ptr[i] = my_malloc(10*i);
	}
	clock_t t9 = clock();
	double time_for_my_malloc = (double)(t5 - t4)/CLOCKS_PER_SEC;
	double time_for_my_free = (double)(t6 - t5)/CLOCKS_PER_SEC;
	double time_for_my_malloc2 = (double)(t7 - t6)/CLOCKS_PER_SEC;
	double time_for_my_free2 = (double)(t8 - t7)/CLOCKS_PER_SEC;
	double time_for_my_malloc3 = (double)(t9 - t8)/CLOCKS_PER_SEC;
	printf("my malloc:\t%f\n", time_for_my_malloc);
	printf("my free:\t%f\n", time_for_my_free);
	printf("my malloc2:\t%f\n", time_for_my_malloc2);
	printf("my free2:\t%f\n", time_for_my_free2);
	printf("my malloc3:\t%f\n", time_for_my_malloc3);


	/*void* ptr [5];
	for(int i = 0; i < 5; i++) {
		ptr[i] = my_malloc(i*2);
	}

	for(int i = 0; i < 5; i+=2) {
		my_free(ptr[i]);
	}

	for(int i = 0; i < 5; i+=2) {
		ptr[i] = my_malloc(10*i);
	}

	for(int i = 0; i < 5; i+=2) {
		my_free(ptr[i]);
	}

	for(int i = 0; i < 5; i+=2) {
		ptr[i] = my_malloc(100*i);
	}*/
}
