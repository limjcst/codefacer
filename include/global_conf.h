#ifndef GLOBAL_CONF_H
#define GLOBAL_CONF_H

#ifndef BUFFER_SIZE
#define BUFFER_SIZE	512
#endif

/* global configuration */
typedef struct {
	char database[BUFFER_SIZE];
	char username[BUFFER_SIZE];
	char password[BUFFER_SIZE];
	char db_mode[BUFFER_SIZE];
	char address[BUFFER_SIZE];
	char conf_path[BUFFER_SIZE];
	char result_path[BUFFER_SIZE];
	char repo_path[BUFFER_SIZE];
	char codeface_path[BUFFER_SIZE];

	int poll_interval;
    int max_running;
} global_conf_t;

#endif