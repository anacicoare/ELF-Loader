# Purpose
Here,in loader/loader.c, I implemented a handler that responded to SIGSEGV.

# Why does SIGSEGV occur
Sigsegv occurs when the memory is not mapped for that page or when it
does not have the needed access right.

# Implementation details
So, I started by checking if I received a SIGSEGV. If not, I called the
default handler.

Then, I searched the segments to check which one contained the fault
address. If I did not find any segment, I called the default handler.

When I found the segment and the index of the page in the segment, I 
checked to see if it was mapped.

If it was not mapped, I computed how much I had to read from the page
(nothing at all, a full page, or it was not full page (maybe at the last
page)). Then, I created a buffer, read data from the exec file to it
and checked to see if it contained a full page. If not, I set the 
remaining memory to a full page to 0.

Then, I mapped a page into memory, with write permissions, copied the
buffer to that address, and after I finished filling that page I assigned
it the right permissions (from the struct field).

If the page had already been mapped, then there is no SIGSEGV error, I
run the default handler.

Also, I keep tabs on which pages are mapped or not using the data field
in the segments'struct.
