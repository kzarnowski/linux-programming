#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define LINE_LENGTH 40
size_t LINE_SIZE = sizeof(char) * LINE_LENGTH;
char* format = "%-7d  %-16s  %-10.6lf";

typedef struct {
	int32_t key;
	char info[16];
	double value;
} RECORD;

RECORD data = { -1, {'\0'}, -1.0};

//  MODE:  
//  1 - read
//  2 - write
//  3 - delete
// -1 - error

int mode = -1;
char* mode_name;
int32_t key = -1;
char info[16] = {'\0'};
double value;

int isset_mode = 0, isset_k = 0, isset_i = 0, isset_v = 0;

int parse_args(int argc, char** argv);
int check_args();
int db_read();
int db_write();
int db_delete();
off_t find_record(int fd, int key);


int main(int argc, char** argv) {
	int res;

	// Parse arguments and assign them to global variables	
	res = parse_args(argc, argv);
	if (res != 0) {
		return res;
	}

	// Validate arguments' values
	res = check_args();
	if (res != 0) {
		return res;
	}

	// Open database in correct mode
	const char* path = argv[argc-1];
	int fd;

	if (mode == 1) {
		fd = open(path, O_RDONLY);
		res = db_read(fd);
		if (res != 0) {
			fprintf(stderr, "Item with key = %d was not found.\n", key);
			return 1;
		}
	} else if (mode == 2) {
		fd = open(path, O_RDWR | O_APPEND | O_CREAT, S_IRWXU);
		res = db_write(fd);
	} else if (mode == 3) {
		fd = open(path, O_RDWR);
		res = db_delete(fd);
		if (res != 0) {
			return 1;
		}
	}

	// Close file.	
	res = close(fd);
	
	if (res == -1) {
		fprintf(stderr, "Error occured during closing a file.\n");
		return 1;
	}

	return 0;
}

int db_read(int fd) {
	char line[LINE_LENGTH] = {'\0'};
	off_t end = lseek(fd, 0, SEEK_END);
	off_t start = lseek(fd, 0, SEEK_SET);
	ssize_t len;	
	while (start != end) {
		len = read(fd, (void*)line, LINE_SIZE);
		if (len < LINE_SIZE) {
			// error
		}
		sscanf(line, "%d %s %lf", &(data.key), data.info, &(data.value)); 
		if (key == data.key) {
			printf("%s\n", line);
			return 0;
		}

		start += LINE_SIZE;
	}	
	return 1;
}

int db_write(int fd) {
	RECORD data;
	data.key = key;
	strcpy(data.info, info);
	data.value = value;
	char line[LINE_LENGTH];
	snprintf(line, LINE_LENGTH, format, data.key, data.info, data.value);
	line[LINE_LENGTH-2] = '\n';
	ssize_t len = write(fd, line, LINE_SIZE);
	return len == LINE_SIZE ? 0 : 1;
}

int db_delete(int fd) {
	off_t offset = find_record(fd, key);
	if (offset == -1) {
		fprintf(stderr, "Item with key = %d was not found.", key);
		return 1;
	}
	lseek(fd, offset, SEEK_SET);
	RECORD empty = {-1, {'\0'}, 0};
	char line[LINE_LENGTH];
	snprintf(line, LINE_LENGTH, format, empty.key, empty.info, empty.value);
	line[LINE_LENGTH-2] = '\n';
	ssize_t len = write(fd, line, LINE_SIZE);
	if (len != LINE_SIZE) {
		fprintf(stderr, "Error during writing empty record/\n");
		return 1;
	}
	return 0;
}

off_t find_record(int fd, int key) {
	char buffer[LINE_LENGTH];
	off_t end = lseek(fd, 0, SEEK_END);
	off_t start = lseek(fd, 0, SEEK_SET);
	ssize_t len;
	while (start != end) {
		buffer[0] = '\0';
		len = read(fd, (void*)buffer, LINE_SIZE);
		if (len < LINE_SIZE) {
			//error
		}
		sscanf(buffer, "%d %s %lf", &(data.key), data.info, &(data.value));
		if (key == data.key) {
			return start;	
		}
	}
	return -1;
}	

int parse_args(int argc, char** argv) {
	int option;
	char* end;

	while((option = getopt(argc, argv, "rwdk:i::v::")) != -1) {
		switch (option) {
			case 'r':
				mode = isset_mode == 0 ? 1 : -1;
				isset_mode = 1;
				mode_name = "read";
				break;
			case 'w':
				mode = isset_mode == 0 ? 2 : -1;
				isset_mode = 1;
				mode_name = "write";
				break;
			case 'd':
				mode = isset_mode == 0 ? 3 : -1;
				isset_mode = 1;
				mode_name = "delete";
				break;
			case 'k':
				isset_k = 1;
				key = (int32_t)(strtol(optarg, &end, 10));
				break;
			case 'i':
				isset_i = 1;
				strncpy(info, optarg, 16);
			   	break;
			case 'v':
				isset_v = 1;
				value = strtod(optarg, NULL);
				break;
			default:
				fprintf(stderr, "Unvalid argument found\n");
				return 1;
		}
	}

	//printf("IS_SET:\nk: %d\ni: %d\nv: %d\n", isset_k, isset_i, isset_v);
	//printf("VALUES:\nmode: %d\nkey: %d\ninfo: %s\nvalue: %lf\n", mode, data.key, data.info, data.value);
	return 0;
}

int check_args() {
	int result = 0;
	
	if (mode == -1) {
		fprintf(stderr, "Mode error\n");
		result = 2;
	}	

	if (mode == 1 || mode == 3) {
		if (isset_k == 0) {
			fprintf(stderr, "You must provide key in %s mode.\n", mode_name);
			result = 3;
		}
		if (isset_v || isset_i) {
			fprintf(stderr, "Arguments other than key are forbidden in %s mode.\n", mode_name);
			result = 4;
		}
	}
	
	if (mode == 2) {
		if (isset_k == 0 || isset_i == 0) {
			fprintf(stderr, "You must provide key and info in %s mode.\n", mode_name);
			result = 5;
		}
	}

	return result;
}


