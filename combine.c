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

#define SIZE_SEGMENT_NAME 16
#define SIZE_FILENAME 128
struct segment {
	struct segment *prev;
	struct segment *next;
	char name[SIZE_SEGMENT_NAME];
	char filename[SIZE_FILENAME];
	int offset;
	int size;
};

#define SEG_BEGIN         "seg_begin"
#define SEG_NAME          "seg_name="
#define SEG_OFFSET        "seg_offset="
#define SEG_FILENAME      "seg_filename="
#define SEG_END           "seg_end"
#define SEG_LENGTH_X(seg) (strlen(seg))

static struct segment *seg_head;

static int print_segment(struct segment *segment)
{
	if (segment) {
		printf("%s\n", SEG_BEGIN);
		printf("%s%s\n", SEG_NAME, segment->name);
		printf("%s0x%x\n", SEG_OFFSET, segment->offset);
		printf("%s%s\n", SEG_FILENAME, segment->filename);
		printf("%s\n", SEG_END);
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

static int usage(char *name)
{
	printf("Usage:\n");
	printf("\t%s <config_file>\n", name);
	return 0;
}

int main(int argc, char ** argv)
{
	FILE *f_in = NULL;
	if (argc != 2) {
		return (usage(argv[0]));
	}
	seg_head = NULL;

	f_in = fopen(argv[1], "r");
	if (NULL == f_in) {
		perror("open file error");
		return -errno;
	}

	parse_config(f_in);
	fclose(f_in);

	print_segment_list(seg_head);
	free_segmnet_list(seg_head);

	return 0;

}
