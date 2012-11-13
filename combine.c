/*
 * =====================================================================================
 *
 *       Filename:  combine.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2012年11月12日 20时03分43秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Hu Liupeng (), hu.liupeng@embedway.com
 *        Company:  
 *
 * =====================================================================================
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFF_SIZE (64<<10)

#define SIZE_SEGMENT_NAME 16
#define SIZE_FILENAME 128
struct segment {
	struct segment *prev;
	struct segment *next;
	char name[SIZE_SEGMENT_NAME];
	char filename[SIZE_FILENAME];
	int offset;
	int size;
	int valid;
};

#define SEG_STAT_VALID   1
#define SEG_STAT_INVALID 0

#define SEG_BEGIN    "seg_begin"
#define SEG_NAME     "seg_name="
#define SEG_OFFSET   "seg_offset="
#define SEG_FILENAME "seg_filename="
#define SEG_SIZE     "seg_size="
#define SEG_VALID    "seg_valid="
#define SEG_END      "seg_end"
#define SEG_LENGTH_X(seg) (strlen(seg))

static struct segment *seg_head;

static int print_segment(struct segment *segment)
{
	if (segment) {
		printf("%s\n",     SEG_BEGIN);
		printf("%s%s\n",   SEG_NAME,     segment->name);
		printf("%s0x%x\n", SEG_OFFSET,   segment->offset);
		printf("%s%s\n",   SEG_FILENAME, segment->filename);
		printf("%s0x%x\n", SEG_SIZE,     segment->size);
		printf("%s%d\n",   SEG_VALID,   segment->valid);
		printf("%s\n",     SEG_END);
	}
	return 0;
}

static int print_segment_list(struct segment *head)
{
	while (head) {
		print_segment(head);
		printf("\n");
		head = head->next;
	}
	return 0;
}

static int free_segmnet_list(struct segment *head)
{
	struct segment *next;

	while (head) {
		next = head->next;
		memset(head, 0, sizeof(struct segment));
		free(head);
		head = next;
	}

	return 0;
}

static int add_segment(struct segment *segment)
{
	struct segment *prev, *next;
	int retvalue = 0;

	if (NULL == seg_head) {
		seg_head = segment;
		return retvalue;
	}

	prev = next = seg_head;

	while(next) {
		if (next->offset <= segment->offset) {
			prev = next;
			next = next->next;
		} else {
			break;
		}
	}

	if (NULL == next) {
		prev->next = segment;
		segment->prev = prev;
	} else {
		next->prev = segment;
		segment->next = next;
		if (prev == seg_head) {
			seg_head = segment;
		}
		if (prev != next) {
			prev->next = segment;
		}
	}

	return retvalue;
}

static int parse_config(FILE *f_config)
{
	char buff[1024];
	int line_no = 0;
	int seg_config;
	struct segment *new_seg = NULL;
	int retvalue = 0;
	int line_length;

	line_no = 0;
	seg_config = 0;
	while(fgets(buff, sizeof(buff), f_config)) {
		++line_no;
		line_length = strlen(buff)- 1;
		while (buff[line_length] == '\n' || buff[line_length] == '\r')  {
			buff[line_length] = '\0';
			line_length--;
		}
		if ('#' == buff[0]) {
			continue;
		}
		if(0 == strncasecmp(buff, SEG_BEGIN, SEG_LENGTH_X(SEG_BEGIN))) {
			if (seg_config) {
				printf("segment config finish mark missing line %d\n", line_no);
				seg_config = 0;
				retvalue = add_segment(new_seg);
				new_seg = NULL;
			}
			seg_config = 1;
			new_seg = (struct segment *)malloc(sizeof(struct segment));
			if (NULL == new_seg) {
				printf("alloc buffer for new segment faild\n");
				retvalue = -ENOMEM;
				break;
			}
			memset(new_seg, 0, sizeof(struct segment));
		} else if(0 == strncasecmp(buff, SEG_END, SEG_LENGTH_X(SEG_END))) {
			seg_config = 0;
			retvalue = add_segment(new_seg);
			new_seg = NULL;
		} else if(0 == strncasecmp(buff, SEG_NAME, SEG_LENGTH_X(SEG_NAME))) {
			if (!seg_config) {
				printf("config out of segment range at line %d\n", line_no);
				continue;
			}
			snprintf(new_seg->name, SIZE_SEGMENT_NAME, "%s", buff+SEG_LENGTH_X(SEG_NAME));
		} else if(0 == strncasecmp(buff, SEG_OFFSET, SEG_LENGTH_X(SEG_OFFSET))) {
			if (!seg_config) {
				printf("config out of segment range at line %d\n", line_no);
				continue;
			}
			new_seg->offset = strtoul(buff+SEG_LENGTH_X(SEG_OFFSET), NULL, 0);
		} else if(0 == strncasecmp(buff, SEG_FILENAME, SEG_LENGTH_X(SEG_FILENAME))) {
			if (!seg_config) {
				printf("config out of segment range at line %d\n", line_no);
				continue;
			}
			snprintf(new_seg->filename, SIZE_FILENAME, "%s", buff+SEG_LENGTH_X(SEG_FILENAME));
		} else {
			if(line_length >= 0) {
				printf("invalid config at line %d\n", line_no);
			}
		}
	}

	if (seg_config) {
		printf("segment config finish mark missing at the end of file\n");
		seg_config = 0;
		retvalue = add_segment(new_seg);
		new_seg = NULL;
	}

	return retvalue;
}


enum {
    E_FOPEN = 1,
    E_ALLOC,
};

static int get_file_size(char *filename, int *size)
{
    struct stat fstat;
    if (filename == NULL) {
        return 0;
    }

    if(stat(filename, &fstat)) {
        perror("stat");
        printf("get size of file \"%s\" failed.\"\n", filename);
        return -1;
    } else {
        *size = fstat.st_size;
    }

    return 0;
}

static int append_file1_to_file2(FILE *f_out, char *filename)
{
    FILE *fin1;
    char *buff;
    int rv=0;
	int r_size;

    fin1 = NULL;
    buff = NULL;

    if (NULL == (fin1 = fopen(filename, "rb"))) {
        perror("file open");
        printf("open file \"%s\" failed\n", filename);
        rv = -E_FOPEN;
        goto _out;
    }

    if (NULL == (buff = (char*)malloc(BUFF_SIZE))) {
        perror("malloc");
        printf("alloc buffer failed\n");
        rv = -E_ALLOC;
        goto _out;
    }

	while ((r_size = fread(buff, 1, BUFF_SIZE, fin1))>0) {
        fwrite(buff, 1, r_size, f_out);
    }

_out:
    if (buff) free(buff);
    if (fin1) fclose(fin1);    
    return rv;
}

static int append_byte_to_file(FILE *f_out, unsigned char byte, int size)
{
    char *buff;
	int rv=0;
	int w_size;

    buff = NULL;

    if (NULL == (buff = (char*)malloc(BUFF_SIZE))) {
        perror("malloc");
        printf("alloc buffer failed\n");
        rv = -E_ALLOC;
        goto _out;
    }

	memset(buff, byte, BUFF_SIZE);

	while (size > 0) {
        w_size = fwrite(buff, 1, (size>BUFF_SIZE)?BUFF_SIZE:size, f_out);
		size -= w_size;
    }

_out:
    if (buff) free(buff);
    return rv;
}

static int check_segment(struct segment *segment)
{
	int retvalue;
	int filesize;

	if (NULL == segment) {
		return -1;
	}

	segment->valid = SEG_STAT_INVALID;
	if (segment->filename) {
		if (strlen(segment->filename)>0) {
			retvalue = get_file_size(segment->filename, &filesize);
			if (0 == retvalue) {
				segment->size = filesize;
			} else {
				goto _out;
			}
		} else {
			goto _out;
		}
	}
	segment->valid = SEG_STAT_VALID;
_out:
	return retvalue;
}

static int check_segment_list(struct segment *head)
{
	struct segment *segment = head;
	while(segment) {
		check_segment(segment);
		segment = segment->next;
	}

	for(; head; head = head->next) {
		if (SEG_STAT_VALID != head->valid) {
			continue;
		}
		for(segment = head->next; segment; segment = segment->next) {
			if (SEG_STAT_VALID != segment->valid) {
				continue;
			}

			if ((head->offset+head->size)>=(segment->offset)) {
				printf("invalid segmnet %s\n", segment->name);
				segment->valid = SEG_STAT_INVALID;
			}
		}
	}

	return 0;
}

static int usage(char *name)
{
	printf("Usage:\n");
	printf("\t%s <config_file> <total_size>\n", name);
	return 0;
}

int main(int argc, char ** argv)
{
	FILE *f_in = NULL;
	int total_size;

	if (argc != 3) {
		return (usage(argv[0]));
	}

	total_size = strtoul(argv[2], NULL, 0);
	seg_head = NULL;

	f_in = fopen(argv[1], "r");
	if (NULL == f_in) {
		perror("open file error");
		return -errno;
	}

	parse_config(f_in);
	fclose(f_in);

//	print_segment_list(seg_head);
	check_segment_list(seg_head);
	print_segment_list(seg_head);
	free_segmnet_list(seg_head);

	return 0;

}
