#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pty.h>
#include <sys/wait.h>

#define GDB "gdb"
#define GDB_MULTIARCH "gdb-multiarch"
#define SHELL_ANCHOR "(gdb) "
#define PROGRAM_END "The program is not being run."
#define CHILD 0

// commands
#define GDB_SHOW_INSTRUCTION_CMD "display/i $pc"
#define GDB_EXIT_CMD "exit"

static volatile bool IS_RUNNING = true;

typedef enum state_t{
    GDB_INIT = 0,
    GDB_SHELL,
    GDB_EXECUTING
} state_t;

typedef enum action_t{
    ACTION_NEXT_CMD = 0,
    ACTION_START_TRACE,
    ACTION_END
} action_t;

typedef enum option_t {
	OPTION_NONE             = 1 << 0,
	OPTION_SHOW_INSTRUCTION = 1 << 1,
	OPTION_SHOW_REGISTERS   = 1 << 2,
	OPTION_SHOW_STACK       = 1 << 3,
} option_t;

typedef struct gdb_t {
    int fd;
    pid_t pid;
    bool last_status;
    state_t state;
} gdb_t;

typedef struct cmd_t {
    char cmd[128];
    action_t action;
    option_t option;
} cmd_t;

void int_handler(int dummy) {
    printf("shutting down...\n");
    IS_RUNNING = 0;
}

void gdb_start(gdb_t *gdb, const char *binary, char *const argv[], void (*loop)(gdb_t *gdb, cmd_t *cmds), cmd_t *cmds){
    gdb->state = GDB_INIT;

    struct termios term;
    // disable echo
    term.c_lflag &= ~ECHO;
    tcgetattr(fileno(stdin), &term);

    pid_t pid = forkpty(&gdb->fd, NULL, &term, NULL);
    if(pid == -1){
        perror(__func__);
        gdb->last_status = false;
        return;
    }

    // set attributes
    tcsetattr(gdb->fd, 0, &term);

    if(pid == CHILD){
        if(execvp(binary, argv) == -1){
            perror(__func__);
            gdb->last_status = false;
            return;
        }
    }
    else{
        gdb->pid = pid;
        gdb->last_status = true;
        loop(gdb, cmds);
    }

    return;
}

void gdb_execute(gdb_t *gdb, const char *cmd){
    if(write(gdb->fd, cmd, strlen(cmd)) == -1){
        perror(__func__);
        gdb->last_status = false;
        return;
    }
    if(write(gdb->fd, "\n", 1) == -1){
        perror(__func__);
        gdb->last_status = false;
        return;
    }

    gdb->last_status = true;
    return;
}


void loop(gdb_t *gdb, cmd_t *cmds){
    char read_buf[4096];
    int used = 0;
    int i = 0;
    while(IS_RUNNING){

        switch (gdb->state) {
            case GDB_INIT:
                used += read(gdb->fd, read_buf+used, sizeof(read_buf)-used);
                assert(used <= sizeof(read_buf));
		            if(strstr(read_buf, SHELL_ANCHOR) != NULL){
                    gdb->state = GDB_SHELL;
                    bzero(read_buf, sizeof(read_buf));
                    used = 0;
                    break;
                }
                break;
            case GDB_EXECUTING:
                used += read(gdb->fd, read_buf+used, sizeof(read_buf)-used);
                assert(used <= sizeof(read_buf));
                if(strstr(read_buf, PROGRAM_END) != NULL){
                    IS_RUNNING = false;
                }
		            else if(strstr(read_buf, SHELL_ANCHOR) != NULL){
                    gdb->state = GDB_SHELL;
                    printf("%s", read_buf);
                    bzero(read_buf, sizeof(read_buf));
                    used = 0;
                    break;
                }
                break;
            case GDB_SHELL:
                switch (cmds[i].action) {
                    case ACTION_END:
                        puts("action end");
                        IS_RUNNING = false;
                        break;
                    case ACTION_NEXT_CMD:
                        gdb->state = GDB_EXECUTING;
                        gdb_execute(gdb, cmds[i].cmd);
                        i++;
                        break;
                    case ACTION_START_TRACE:
                        gdb->state = GDB_EXECUTING;
                        if( (cmds[i].option & OPTION_SHOW_INSTRUCTION) != 0){
                            cmds[i].option &= ~OPTION_SHOW_INSTRUCTION;
                            gdb_execute(gdb, GDB_SHOW_INSTRUCTION_CMD);	
                        }
                        gdb_execute(gdb, cmds[i].cmd);
                        break;
                }
                if(gdb->last_status == false){
                    IS_RUNNING = false;
                }
                break;
            default:
                break;
        }
    }
    gdb_execute(gdb, GDB_EXIT_CMD);
    close(gdb->fd);
    kill(gdb->pid, SIGTERM);
    return;
}

void usage(const char *p){
    printf("usage: %s [-mgv] [-b binary]\n", p);
    return;
}

int main(int argc, char **argv){
    gdb_t gdb;
    cmd_t cmds[] = {
	{"set logging enabled on", ACTION_NEXT_CMD, OPTION_NONE},
        {"break main", ACTION_NEXT_CMD, OPTION_NONE},
        {"run", ACTION_NEXT_CMD, OPTION_NONE},
        {"stepi", ACTION_START_TRACE, OPTION_SHOW_INSTRUCTION},
        {"", ACTION_END}
    };

    int opt;

    char *binary = NULL;
    char *gdb_binary = NULL;
    bool verbose = false;
    
    while((opt = getopt(argc, argv, "mgb:v")) != -1) 
    { 
        switch(opt) 
        { 
            case 'g':
                gdb_binary = GDB;
                break;
            case 'm':
                gdb_binary = GDB_MULTIARCH;
                break;
            case 'b':
                binary = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case '?': 
                printf("unknown option: %c\n", optopt);
                usage(argv[0]);
                exit(EXIT_FAILURE);
                break;
        } 
    } 

    if(binary == NULL){
        printf("binary is required!\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if(gdb_binary == NULL){
        printf("gdb type is required!\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, int_handler);
    gdb_start(&gdb, gdb_binary, (char *const[]){gdb_binary, binary,NULL}, &loop, cmds);
    return 0;
}
