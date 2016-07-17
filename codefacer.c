#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <signal.h>
#include <mysql/mysql.h>

#include "config.h"

const int SETTINGS_COUNT = 6, MAX_RUNNING = 8;
MYSQL db;
int rows_count, running, max_running, id;
global_conf_t global_conf;

#define buf_size 2047

void sigterm_handler(int signo){ 
    if (signo == SIGTERM) {
        mysql_close(&db);
        syslog(LOG_INFO, "Codefacer daemon terminated."); 
        closelog(); 
        exit(0); 
    } 
}

void sigchld_handler(int signo){
    if (signo == SIGCHLD) {
        while (waitpid(-1, NULL, WNOHANG) > 0)
            running--;
    }
}

void init(){
    openlog("codefacer_daemon", LOG_PID, LOG_USER); 
    syslog(LOG_INFO, "Codefacer daemon started."); 
    signal(SIGTERM, sigterm_handler);
    signal(SIGCHLD, sigchld_handler);

    init_configuration(&global_conf);
    max_running = MAX_RUNNING;
	if (global_conf.max_running != 0)
		max_running = global_conf.max_running;

    mysql_init(&db);
    if (!mysql_real_connect(&db, global_conf.address, global_conf.username,
                global_conf.password, global_conf.database, 0, NULL, 0)) {
        syslog(LOG_ERR, "Failed to connec to database!");
	    syslog(LOG_ERR, mysql_error(&db));
        exit(1);
    }

    srand(time(0));
    running = 0;
    syslog(LOG_INFO, "codefacer daemon initialized");
}

void createConf(char *confPath, char *repoPath, char *username, char *name) {
    char revisions[100 * buf_size] = "\0";
    char rcs[100 * buf_size] = "\0";
    FILE *conf = fopen(confPath, "wb");
    char command[buf_size];
    char buf[buf_size];
    sprintf(command, "git --git-dir=%s for-each-ref --format='%%(*committerdate:raw)%%(committerdate:raw) %%(refname) %%(*objectname) %%(objectname)' refs/tags | sort -n | awk '{split($3, temp, \"refs/tags/\"); print temp[2]; }'", repoPath);
    FILE *stream = popen(command, "r");
    char cur[buf_size];
    char rc[buf_size] = "'', ";
    char preIsRC = 0;
    char first = 1;
    while (fgets(buf, buf_size, stream)) {
        {//if (buf[0] == 'v') {
            buf[strlen(buf) - 1] = '\0';
            if (first) {
                sprintf(cur, "'%s', ", buf);
                strcpy(revisions, cur);
                strcpy(rcs, cur);
                first = 0;
                continue;
            }
            //printf("%s ", buf);
            if (strstr(buf, "rc") > 0 || strstr(buf, "RC") > 0) {
                if (preIsRC) {
                    //do nothing
                } else {
                    preIsRC = 1;
                    sprintf(rc, "'%s', ", buf);
                }
                //printf("\n");
            } else {
                sprintf(cur, "'%s', ", buf);
                //printf("%s %s\n", cur, rc);
                strcat(revisions, cur);
                strcat(rcs, rc);
                strcpy(rc, "'',");
                preIsRC = 0;
            }
        }
    }
    printf("%s\n\n%s\n\n", revisions, rcs);
    fprintf(conf, "project: %s@%s\n", username, name);
    fprintf(conf, "description: \n");
    fprintf(conf, "repo: .\n");
    fprintf(conf, "revisions: [%s]\n", revisions);
    fprintf(conf, "rcs: [%s]\n", rcs);
    fprintf(conf, "tagging: committer2author\n");
    fprintf(conf, "sloccount: true\n");
    fprintf(conf, "understand: true\n");
    fclose(conf);
}

int run(int jobs){
    system("mysql -ucodeface -pcodeface < /home/git/codeface/datamodel/codeface_schema.sql");
    char query[buf_size] = "SELECT name,path,creator_id FROM projects";
    int ret = mysql_query(&db, query);
    if (ret != 0) {
        syslog(LOG_ERR, "SQL Query Failed!");
        return 0;
    }
    MYSQL_RES *result = mysql_store_result(&db);
    rows_count = mysql_num_rows(result);
    if (rows_count == 0) {
        mysql_free_result(result);
        return 0;
    }

    char command[buf_size];
    printf("%d projects to analyze\n", rows_count);
    for (int i = 0; i < rows_count; ++i) {
        MYSQL_ROW row = mysql_fetch_row(result);
        char *name = row[0];
        char *path = row[1];
        int id = atoi(row[2]);
        char *username;
        sprintf(query, "select username from users WHERE id=%d", id);
        if (mysql_query(&db, query) == 0) {
            MYSQL_RES *resultUN = mysql_store_result(&db);
            MYSQL_ROW rowUN = mysql_fetch_row(resultUN);
            username = rowUN[0];
            char repoPath[buf_size];
            strcpy(repoPath, global_conf.repo_path);
            strcat(repoPath, username);
            strcat(repoPath, "/");
            strcat(repoPath, path);
            strcat(repoPath, ".git/");

            char confPath[buf_size];
            strcpy(confPath, global_conf.conf_path);
            mkdir(confPath, S_IRWXU | S_IRWXG);
            strcat(confPath, username);
            strcat(confPath, "/");
            mkdir(confPath, S_IRWXU | S_IRWXG);
            strcat(confPath, name);
            strcat(confPath, ".conf");

            createConf(confPath, repoPath, username, name);

            sprintf(command, "codeface -j %d run -c %scodeface.conf -p %s %s %s", jobs, global_conf.codeface_path, confPath, global_conf.result_path, repoPath);
            if (system(command) < 0) {}

            mysql_free_result(resultUN);
        }
    }

    mysql_free_result(result);
    return 0;
}

int main(void) {
    init();
    FILE *stream = popen("nproc", "r");
    int jobs;
    fscanf(stream, "%d", &jobs);
    jobs -= 1;
    if (jobs == 0) {
        jobs = 1;
    }
    run(jobs);

    return 0;
} 
