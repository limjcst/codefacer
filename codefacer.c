#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <signal.h>
#include <mysql/mysql.h>
#include <time.h>

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
        syslog(LOG_ERR, "%s", mysql_error(&db));
        exit(1);
    }

    srand(time(0));
    running = 0;
    syslog(LOG_INFO, "codefacer daemon initialized");
}

char recs[buf_size][43];
long long commitTime[buf_size];
int head;
const int tail = buf_size - 1;

/**
 * Translate from python code
 * codeface/codeface/util.py
 *   generate_analysis_windows(repo, window_size_months)
 */

int generateRevision(FILE *conf, char *repo) {
    char command[buf_size];
    char buf[buf_size];
    char t_buf[buf_size];
    head = buf_size;
    int window_size_weeks = 1;
    struct tm latest_commit, date_temp;
    time_t temp, week;
    week = 7 * 86400;
    sprintf(command, "git --git-dir=%s log --format=%%ad --date=iso8601 --all --max-count=1", repo);
    FILE *stream = popen(command, "r");
    if (fgets(buf, buf_size, stream) == 0 || buf[0] < '0' || buf[0] > '9') {
        fclose(stream);
        return 0;
    }
    strptime(buf, "%Y-%m-%d %H:%M:%S %z", &latest_commit);
    fclose(stream);
    temp = mktime(&latest_commit);
    latest_commit = *localtime(&temp);
    strftime(buf, buf_size, "--before=%Y-%m-%dT%H:%M:%S%z", &latest_commit);
    sprintf(command, "git --git-dir=%s log --no-merges --format='%%H %%ct' --all --max-count=1 %s", repo, buf);
    stream = popen(command, "r");
    while (fgets(buf, buf_size, stream)) {
        --head;
        sscanf(buf, "%s %lld", recs[head], &commitTime[head]);
    }
    fclose(stream);
    strftime(buf, buf_size, "--before=%Y-%m-%dT%H:%M:%S%z", &date_temp);
    int start = window_size_weeks;  // Window size weeks ago
    int end = 0;
    while (start != end) {
        temp = mktime(&latest_commit);
        temp -= week * start;
        date_temp = *gmtime(&temp);
        strftime(buf, buf_size, "--before=%Y-%m-%dT%H:%M:%S%z", &date_temp);
        sprintf(command, "git --git-dir=%s log --no-merges --format='%%H %%ct' --all --max-count=1 %s", repo, buf);
        stream = popen(command, "r");
        if (fgets(buf, buf_size, stream)) {
            end = start;
            start += window_size_weeks;
        } else {
            fclose(stream);
            start = end;
            sprintf(command, "git --git-dir=%s log --no-merges --format='%%H %%ct' --all --reverse", repo);
            stream = popen(command, "r");
            if (fgets(buf, buf_size, stream) == 0) {
                fclose(stream);
                continue;
            }
        }
        buf[strlen(buf) - 1] = '\0';
        sprintf(t_buf, "%s %lld", recs[head], commitTime[head]);
        if (strcmp(buf, t_buf) != 0) {
            --head;
            sscanf(buf, "%s %lld", recs[head], &commitTime[head]);
        }
        fclose(stream);
    }
    // Check that commit dates are monotonic, in some cases the earliest
    // first commit does not carry the earliest commit date
    if (head < tail && commitTime[head] > commitTime[head + 1]) {
        ++head;
    }
    int i = 0;
    for (; head <= tail; ++ head) {
        fprintf(conf, "'%s', ", recs[head]);
        ++i;
    }
    return i;
}

int createConf(char *confPath, char *repoPath, char *username, char *name) {
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
    char tagExist = 0;
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
                tagExist = 1;
            }
        }
    }
    fclose(stream);
    fprintf(conf, "project: %s@%s\n", username, name);
    fprintf(conf, "description: \n");
    fprintf(conf, "repo: .\n");
    /**
     * tag analysis leads to some errors.
     * window analysis is proper.
     **/
    tagExist = 0;
    if (tagExist) {
        fprintf(conf, "revisions: [%s]\n", revisions);
        fprintf(conf, "rcs: [%s]\n", rcs);
    } else {
        //use commit hash substitute
        fprintf(conf, "revisions: [");
        int i = generateRevision(conf, repoPath);
        if (i == 0) {
            fclose(conf);
            return 1;
        }
        fprintf(conf, "]\n");
        fprintf(conf, "rcs: [");
        for (; i > 0; --i) {
            fprintf(conf, "'', ");
        }
        fprintf(conf, "]\n");
    }
    fprintf(conf, "tagging: committer2author\n");
    fprintf(conf, "sloccount: true\n");
    fprintf(conf, "understand: true\n");
    fclose(conf);
    return 0;
}

int run(int jobs){
    char command[buf_size];
    // remove the results left
    sprintf(command, "rm -f %slog/codeface.log.R.*", global_conf.codeface_path);
    if (system(command)) {
        printf("Command:'%s' failed", command);
    }
    sprintf(command, "rm -rf %s*", global_conf.result_path);
    if (system(command)) {
        printf("Command:'%s' failed", command);
    }
    // empty database
    sprintf(command, "mysql -ucodeface -pcodeface -Nse 'show tables' codeface | while read table; do mysql -ucodeface -pcodeface -e \"SET FOREIGN_KEY_CHECKS = 0; truncate table $table\" codeface; done");
    if (system(command)) {
        // nothing
    }
    char query[buf_size] = "SELECT max(id) FROM projects";
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
    MYSQL_ROW row = mysql_fetch_row(result);
    int maxID = atoi(row[0]);
    printf("%d\n\n", maxID);
    mysql_free_result(result);
    int i;
    for (i = 1; i <= maxID; ++i) {
        sprintf(query, "select name, path, creator_id from projects WHERE id=%d", i);
        if (mysql_query(&db, query) == 0) {
            result = mysql_store_result(&db);
            if (mysql_num_rows(result) == 0) {
                mysql_free_result(result);
                continue;
            }
        }
        MYSQL_ROW row = mysql_fetch_row(result);
        char *name = row[0];
        char *path = row[1];
        int id = atoi(row[2]);
        printf("%s %s %d\n", name, path, id);
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

            if (createConf(confPath, repoPath, username, name)) {
                mysql_free_result(resultUN);
                printf("Nothing to analyze!\n");
                continue;
            }

            sprintf(command, "codeface -j %d run -c %scodeface.conf -p %s %s %s", jobs, global_conf.codeface_path, confPath, global_conf.result_path, repoPath);
            if (system(command) < 0) {}

            mysql_free_result(resultUN);
        }
        mysql_free_result(result);
    }

    return 0;
}

int main(void) {
    init();
    FILE *stream = popen("nproc", "r");
    int jobs = 0;
    if (fscanf(stream, "%d", &jobs)) {
       jobs /= 2;
    }
    if (jobs == 0) {
        jobs = 1;
    }
    run(jobs);

    return 0;
} 
