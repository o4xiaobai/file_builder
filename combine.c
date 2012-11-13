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

enum {
    E_FOPEN = 1,
    E_ALLOC,
	E_EMPTY_TARGET,
};
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

struct target {
	struct target  *prev;
	struct target  *next;
	struct segment *seg_head;
	char   name[SIZE_SEGMENT_NAME];
	char   filename[SIZE_FILENAME];
	int    size;
	int    build;
};

#define SEG_STAT_VALID   1
#define SEG_STAT_INVALID 0

#define TARGET_BEGIN    "target_begin"
#define TARGET_NAME     "target_name="
#define TARGET_SIZE     "target_size="
#define TARGET_FILENAME "target_filename="
#define TARGET_BUILD    "target_build="

#define SEG_BEGIN       "seg_begin"
#define SEG_NAME        "seg_name="
#define SEG_OFFSET      "seg_offset="
#define SEG_FILENAME    "seg_filename="
#define SEG_SIZE        "seg_size="
#define SEG_VALID       "seg_valid="
#define SEG_END         "seg_end"

#define TARGET_END      "target_end"

#define STRLEN(seg) (strlen(seg))

static struct target *g_target_head = NULL;
static int g_verbose = 0;

static int print_segment(struct segment *segment)
{
	if (segment) {
		printf("  %s\n",       SEG_BEGIN);
		printf("    %s%s\n",   SEG_NAME,     segment->name);
		printf("    %s0x%x\n", SEG_OFFSET,   segment->offset);
		printf("    %s%s\n",   SEG_FILENAME, segment->filename);
		printf("#   %s0x%x\n", SEG_SIZE,     segment->size);
		printf("#   %s%d\n",   SEG_VALID,    segment->valid);
		printf("  %s\n",       SEG_END);
	}
	return 0;
}

static int print_segment_list(struct segment *head)
{
	while (head) {
		print_segment(head);
	//	printf("\n");
		head = head->next;
	}
	return 0;
}

static int print_target(struct target *target)
{
	if (target) {
		printf("%s\n",     TARGET_BEGIN);
		printf("  %s%s\n",   TARGET_NAME,     target->name);
		printf("  %s0x%x\n", TARGET_SIZE,     target->size);
		printf("  %s%s\n",   TARGET_FILENAME, target->filename);
		printf("  %s%d\n",   TARGET_BUILD,    target->build);
		if (target->seg_head) {
			print_segment_list(target->seg_head);
		}
		printf("%s\n",     TARGET_END);
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

static int free_target(struct target *target)
{
	if (NULL == target) {
		return 0;
	}
	if (target->seg_head) {
		free_segmnet_list(target->seg_head);
		target->seg_head = NULL;
	}
	memset(target, 0, sizeof(struct target));
	free(target);
	return 0;
}

static int add_target(struct target *target)
{
	if (NULL == g_target_head) {
		g_target_head = target;
	} else {
		target->next = g_target_head;
		g_target_head->prev = target;
		g_target_head = target;
	}
	return 0;
}

static int add_segment_to_target(struct target *target, struct segment *segment)
{
	struct segment *prev, *next;
	int retvalue = 0;

	if (NULL == target) {
		printf("%s: empty target\n", __func__);
		return -E_EMPTY_TARGET;
	}
	if (NULL == target->seg_head) {
		target->seg_head = segment;
		return retvalue;
	}

	prev = next = target->seg_head;

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
		if (prev == target->seg_head) {
			target->seg_head = segment;
		}
		if (prev != next) {
			prev->next = segment;
		}
	}

	return retvalue;
}

static int parse_config(FILE *f_config)
{
	char buffer[1024];
	char *buff;
	int line_no = 0;
	int target_config;
	int seg_config;
	struct segment *new_seg = NULL;
	struct target *new_target = NULL;
	int retvalue = 0;
	int line_length;

	line_no       = 0;
	seg_config    = 0;
	target_config = 0;
	buff = buffer;

	while(fgets(buffer, sizeof(buffer), f_config)) {
		buff = buffer;

		++line_no;
		/* ignore comment line */
		if ('#' == buff[0]) {
			continue;
		}

		/* ignore '\r' '\n' ' ' '\t' and the start of line */
		while ('\r' == *buff || '\n' == *buff || '\t' == *buff || ' ' == *buff) {
			buff++;
		}

		/* ignore '\r' '\n' ' ' '\t' and the end of line */
		line_length = strlen(buff)- 1;
		while (buff[line_length] == '\n' || buff[line_length] == '\r' || buff[line_length] == ' ' 
				|| buff[line_length] == '\t')  {
			buff[line_length] = '\0';
			line_length--;
		}

		if (line_length < 0) {
			continue;
		}

		if(0 == strncasecmp(buff, TARGET_BEGIN, STRLEN(TARGET_BEGIN))) {
			if (target_config) {
				printf("target config finish mark missing at line %d\n", line_no);
				/*  */
				if (seg_config) {
					seg_config = 0;
					retvalue = add_segment_to_target(new_target, new_seg);
					new_seg = NULL;
				}
				retvalue = add_target(new_target);
				new_target = NULL;
			}

			target_config = 1;
			new_target = (struct target *)malloc(sizeof(struct target));
			if (NULL == new_target) {
				printf("alloc buffer for new target faild\n");
				retvalue = -ENOMEM;
				break;
			}
			memset(new_target, 0, sizeof(struct target));
			continue;
		} 
		
		if(0 == strncasecmp(buff, TARGET_END, STRLEN(TARGET_END))) {
			if (target_config) {
				/*  */
				if (seg_config) {
					retvalue = add_segment_to_target(new_target, new_seg);
					new_seg = NULL;
					seg_config = 0;
				}
				retvalue = add_target(new_target);
				new_target = NULL;
				target_config = 0;
			}
			continue;
		} 

		if (!target_config) {
			printf("config out of target range at line %d\n", line_no);
			continue;
		}
		
		if(0 == strncasecmp(buff, TARGET_NAME, STRLEN(TARGET_NAME))) {
			snprintf(new_target->name, SIZE_SEGMENT_NAME, "%s", buff+STRLEN(TARGET_NAME));
			continue;
		} 
		
		if(0 == strncasecmp(buff, TARGET_FILENAME, STRLEN(TARGET_FILENAME))) {
			snprintf(new_target->filename, SIZE_FILENAME, "%s", buff+STRLEN(TARGET_FILENAME));
			continue;
		} 
		
		if(0 == strncasecmp(buff, TARGET_BUILD, STRLEN(TARGET_BUILD))) {
			new_target->build = strtoul(buff+STRLEN(TARGET_BUILD), NULL, 0);
			continue;
		} 

		if(0 == strncasecmp(buff, TARGET_SIZE, STRLEN(TARGET_SIZE))) {
			new_target->size = strtoul(buff+STRLEN(TARGET_SIZE), NULL, 0);
			continue;
		} 
		
		if(0 == strncasecmp(buff, SEG_BEGIN, STRLEN(SEG_BEGIN))) {
			if (seg_config) {
				printf("segment config finish mark missing at line %d\n", line_no);
				retvalue = add_segment_to_target(new_target, new_seg);
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
			continue;
		} 
		
		if(0 == strncasecmp(buff, SEG_END, STRLEN(SEG_END))) {
			if(seg_config) {
				retvalue = add_segment_to_target(new_target, new_seg);
				new_seg = NULL;
				seg_config = 0;
			}
			continue;
		} 
		
		if (!seg_config) {
			printf("config out of segment range at line %d\n", line_no);
			continue;
		}

		if(0 == strncasecmp(buff, SEG_NAME, STRLEN(SEG_NAME))) {
			snprintf(new_seg->name, SIZE_SEGMENT_NAME, "%s", buff+STRLEN(SEG_NAME));
			continue;
		} 
		
		if(0 == strncasecmp(buff, SEG_OFFSET, STRLEN(SEG_OFFSET))) {
			new_seg->offset = strtoul(buff+STRLEN(SEG_OFFSET), NULL, 0);
			continue;
		} 
		
		if(0 == strncasecmp(buff, SEG_FILENAME, STRLEN(SEG_FILENAME))) {
			snprintf(new_seg->filename, SIZE_FILENAME, "%s", buff+STRLEN(SEG_FILENAME));
			continue;
		} 

		printf("invalid config at line %d\n", line_no);
	}

	if (seg_config) {
		printf("segment config finish mark missing at the end of file\n");
		seg_config = 0;
		retvalue = add_segment_to_target(new_target, new_seg);
		new_seg = NULL;
	}

	if (target_config) {
		printf("target config finish mark missing at the end of file\n");
		target_config = 0;
		retvalue = add_target(new_target);
		new_target = NULL;
	}

	return retvalue;
}

static int get_file_size(char *filename, int *size)
{
    struct stat fstat;
    if (filename == NULL) {
        return 0;
    }

    if(stat(filename, &fstat)) {
        perror("stat");
        printf("get size of file \"%s\" failed.\n", filename);
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

			if ((head->offset+head->size)>(segment->offset)) {
				printf("invalid segmnet %s\n", segment->name);
				segment->valid = SEG_STAT_INVALID;
			}
		}
	}

	return 0;
}

//static int append_file1_to_file2(FILE *f_out, char *filename)
//
static int append_segment_to_file(FILE *f_out, struct segment *segment)
{
	if (segment && (SEG_STAT_VALID == segment->valid)) {
		return append_file1_to_file2(f_out, segment->filename);
	}
	return -1;
}

static int output_segment_list(FILE *f_out, struct segment *head)
{
	int last_offset = 0;

	for (last_offset = 0; head; head = head->next) {
		if (SEG_STAT_VALID != head->valid) {
			continue;
		}
		if (head->offset > last_offset) {
			append_byte_to_file(f_out, 0xff, head->offset - last_offset);
			last_offset = head->offset;
		}
		append_segment_to_file(f_out, head);
		last_offset = head->offset + head->size;
	}

	return last_offset;
}

static int build_target(struct target *target)
{
	FILE *f_out = NULL;
	int last_offset;

	if (NULL == target) {
		printf("%s: empty target\n", __func__);
		return -E_EMPTY_TARGET;
	}

	/* parameter check to be added here */

	/*  */

	check_segment_list(target->seg_head);
	if (g_verbose) {
		print_target(target);
	}

	if (target->build) {
		f_out = fopen(target->filename, "wb+");
		if (NULL == f_out) {
			perror("open file error");
			return -errno;
		}

		last_offset = output_segment_list(f_out, target->seg_head);
		if (target->size > last_offset) {
			append_byte_to_file(f_out, 0xff, target->size - last_offset);
		} else if (target->size < last_offset) {
			printf("Warning: target file size is big than expected!\n");
		}
		fclose(f_out);
	}  

	return 0;
}

static int usage(char *name)
{
	printf("Usage:\n");
	printf("\t%s [-v] <config_file> \n", name);
	return 0;
}

int main(int argc, char **argv)
{
	FILE *f_in = NULL;
	struct target *target_head;

	if (argc != 2 && argc != 3) {
		return (usage(argv[0]));
	}

	if (argc == 3) {
		if (strcmp(argv[1], "-v") == 0) {
			g_verbose = 1;
		}
	}

	f_in = fopen(argv[argc - 1], "r");
	if (NULL == f_in) {
		perror("open file error");
		return -errno;
	}

	parse_config(f_in);
	fclose(f_in);

	while(g_target_head) {
		target_head = g_target_head;
		g_target_head = target_head->next;
		build_target(target_head);
		free_target(target_head);
	}

	return 0;

}
