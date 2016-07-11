#ifndef CONFIG_H
#define CONFIG_H

#include "global_conf.h"
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifndef CONF_FILE
#define CONF_FILE "/etc/codefacer.conf"
#endif

char* ltrim(char* str) {
	int len = strlen(str);
	int start = 0, i;
	while (start < len && isspace(str[start])) start++;

	if (start == len) {
		str[0] = '\0';
	} else {
		for (i = start; i < len; i++)
			str[i - start] = str[i];
	}
	return str;
}

char* rtrim(char* str) {
	int len = strlen(str);
	while (len > 0 && isspace(str[len - 1])) {
		len--;
	}
	str[len] = '\0';
	return str;
}

int parse_configuration(char* buf, global_conf_t* conf) {
	char *item, *param;
	item = strtok(buf, "=");
	param = strtok(NULL, "\0");
	rtrim(item);
	ltrim(param);
	rtrim(param);

	if ( !strcmp(item, "ADDRESS") ) {
		strcpy(conf->address, param);
		return 0;

	} else if ( !strcmp(item, "DATABASE") ) {
		strcpy(conf->database, param);
		return 0;

	} else if ( !strcmp(item, "USERNAME") ) {
		strcpy(conf->username, param);
		return 0;

	} else if ( !strcmp(item, "PASSWORD") ) {
		strcpy(conf->password, param);
		return 0;

	} else if ( !strcmp(item, "DB_MODE") ) {
		strcpy(conf->db_mode, param);
		return 0;

	} else if ( !strcmp(item, "MAX_RUNNING") ) {
		conf->max_running = atoi(param);
		return 0;

	} else if ( !strcmp(item, "POLL_INTERVAL") ) {
		conf->poll_interval = atoi(param);
		return 0;

	} else if ( !strcmp(item, "CONF_PATH") ) {
		strcpy(conf->conf_path, param);
		return 0;

	} else if ( !strcmp(item, "RESULT_PATH") ) {
		strcpy(conf->result_path, param);
		return 0;

	} else if ( !strcmp(item, "REPO_PATH") ) {
		strcpy(conf->repo_path, param);
		return 0;

	} else if ( !strcmp(item, "CODEFACE_PATH") ) {
		strcpy(conf->codeface_path, param);
		return 0;

	}
	return 1;
}

int init_configuration(global_conf_t *gconf) {
	FILE *conf = fopen(CONF_FILE, "r");
	if (conf == NULL) {
		fprintf(stderr, "[ERROR]Error opening configuration file!\n");
		return 1;
	}
	
	/* Cannot handle line longer than BUFFER_SIZE */
	int line_no = 0;
	char buf[BUFFER_SIZE];
	while ( fgets(buf, BUFFER_SIZE, conf) != NULL ) {
		line_no++;
		ltrim(buf);
		if (strlen(buf) == 0 || buf[0] == '#') continue;

		if ( parse_configuration(buf, gconf) ) {
			fprintf(stderr, "[WARN]Failed to parse configuration, line %d ommited.\nDetails:\"%s\"\n", line_no, buf);
		}
	}

	fclose(conf);
	return 0;
}

#endif // CONFIG_H

