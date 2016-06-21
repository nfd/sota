// Free, public domain, CC0, contact me if any questions

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "fmemopen.h"

struct mmfile_struct {
	uint8_t *buf;
	fpos_t idx;
	size_t size;
};

static int mmfile_read(void *cookie, char *buf, int amt)
{
	struct mmfile_struct *mmfile = cookie;

	size_t avail = mmfile->size - mmfile->idx;

	if(amt > avail)
		amt = avail;

	if(amt > 0) {
		memcpy(buf, mmfile->buf + mmfile->idx, amt);
		mmfile->idx += amt;
		return amt;
	} else {
		return 0;
	}
}

static int mmfile_write(void *cookie, const char *buf, int amt)
{
	struct mmfile_struct *mmfile = cookie;

	size_t avail = mmfile->size - mmfile->idx;

	if(amt > avail)
		amt = avail;

	if(amt > 0) {
		memcpy(mmfile->buf + mmfile->idx, buf, amt);
		mmfile->idx += amt;
		return amt;
	} else {
		return 0;
	}
}

fpos_t mmfile_seek(void *cookie, fpos_t offset, int whence)
{
	struct mmfile_struct *mmfile = cookie;

	switch(whence) {
		case SEEK_SET:
			mmfile->idx = offset;
			break;
		case SEEK_CUR:
			mmfile->idx += offset;
			break;
		case SEEK_END:
			mmfile->idx = mmfile->size + offset;
			break;
		default:
			return -1;
	}

	return mmfile->idx;
}

int mmfile_close(void *cookie)
{
	free(cookie);
	return 0;
}

FILE *fmemopen(void *buf, size_t size, const char *mode)
{
	struct mmfile_struct *mmfile = malloc(sizeof *mmfile);
	if(mmfile == NULL)
		return NULL;

	mmfile->buf = buf;
	mmfile->size = size;
	mmfile->idx = 0;

	return funopen(mmfile, mmfile_read, mmfile_write, mmfile_seek, mmfile_close);
}

