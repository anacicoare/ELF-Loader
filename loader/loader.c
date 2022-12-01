/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "exec_parser.h"

static so_exec_t *exec;
static struct sigaction old_action;
static int fd;

static void segv_handler(int signum, siginfo_t *info, void *context)
{
	/* TODO - actual loader implementation */

	int map_flags = MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS;

	// implemented only sigsegv handler;
	if (signum != SIGSEGV) 
	{
		old_action.sa_sigaction(signum, info, context);
		return;
	}

	// (void*) si_addr from siginfo_t structure indicates
	// where the fault occured;
	
	// so, now we have to find out in which of the exec's segments this fault
	// address is;
	uintptr_t fault_addr = (uintptr_t) info->si_addr;
	
	// find the index of the segment
	int index = -1;
	int i;
	for (i = 0; i < exec->segments_no; i++)
		if(fault_addr >= (uintptr_t) exec->segments[i].vaddr &&
		   fault_addr <= (uintptr_t) exec->segments[i].vaddr + (uintptr_t) exec->segments[i].mem_size)
			{
				index = i;
				break;
			}
	
	//if no page from this segment caused the fault, run default handler
	if (index == -1)
	{
		old_action.sa_sigaction(signum, info, context);
		return;
	}

	// so, we have to map the unmapped pages for that segment
	// in order to do that we have to:
	// 1. calculate how many pages are in that segment
	// 2. map those pages
	
	int page_size = getpagesize();

	// compute the index of the page from the identified segment
	uint8_t page_index = ((uintptr_t)info->si_addr - exec->segments[index].vaddr) / page_size;

	// compute the start address of the page
	uint8_t* start_addr = (uint8_t*)(exec->segments[index].vaddr + (page_index * page_size));

	//begin to alloc memory and map	
	//first, we check to see if this page has been mapped
	if(exec->segments[index].data == NULL)
	{
		//no memory has been allocated for this segment
		
		//compute nr of pages
		int nr_pages = ceil(exec->segments[index].mem_size / page_size);

		//alloc memory enough to be able to see if every page is mapped
		exec->segments[index].data = (int8_t*) calloc(nr_pages + 1, page_size);
	}

	//if the page has not been mapped
	if(*((uint8_t*)exec->segments[index].data + page_index * sizeof(uint8_t)) == 0)
	{
		int left_to_read = 0;

		//check if there is anything left to read
		if (page_index * page_size > exec->segments[index].file_size) 
			left_to_read = 0;
		else
			{
				//if there is less than a page, we read a page
				if(exec->segments[index].file_size - page_index * page_size < page_size)
					left_to_read = exec->segments[index].file_size - page_index * page_size;
				else
				{
					//else we map the full page
					left_to_read = page_size;
				}
			}

		uint8_t *buff = calloc(page_size, sizeof(uint8_t));
		int offset = exec->segments[index].offset + page_index * page_size;
		int bytes_read;

		// if we still have data left to read
		if (left_to_read > 0)
		{
			// read exactly left_to_read bytes into the buffer
			lseek(fd, offset, SEEK_SET);

			bytes_read = 0;
			while(bytes_read < left_to_read)
			{
				int bytes = read(fd, buff + bytes_read, left_to_read - bytes_read);
				
				if(bytes < 0) // error
					break;

				if(bytes == 0) //end of file
					break;

				bytes_read += bytes;
			}

		}

		
		// if we have left to read less than a page
		// then fill the rest of the page with zeroes
		if (left_to_read < page_size)
			memset((void *)(buff + left_to_read), 0, page_size - left_to_read);

		// now we map the page with write access in order to be able to fill its contents
		mmap((void *)start_addr, page_size, PROT_WRITE, map_flags, fd, 0);

		// copy the data
		memcpy((void *)start_addr, buff, page_size);

		mprotect(start_addr, page_size, exec->segments[i].perm);

		// finally, we succeeded in mapping the page
		*((uint8_t *)exec->segments[index].data + page_index * sizeof(uint8_t)) = 1;

		free(buff);
	}
	else
	{
		//page has been mapped, run default handler
		old_action.sa_sigaction(signum, info, context);
		return;
	}

}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, NULL);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	fd = open(path, O_RDONLY);

	//mark all of the segments' data as unmapped
	int i;
	for (i = 0; i < exec->segments_no; i++)
		exec->segments[i].data = NULL;

	so_start_exec(exec, argv);

	close(fd);

	return -1;
}
