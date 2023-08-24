/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#define MAXARGS 128
#define RUNNING 0
#define SUSPENDED 1

typedef struct process
{
    char name[100];
    int num;
    pid_t pid;
    int status;
    struct process *link;
} job;
job *fg_job;
job *head;
int homepid;
/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

job *make_job(char **argv, int num, int pid, int status);
int find_job_num();
void add_job(char **argv, int pid, int status);
job *find_job_from_pid(int pid);
void kill_manager(char *num);

void INT_handler(int sig);
void TSTP_handler(int sig);
void CHLD_handler(int sig);
char history_path[1024];

job *make_job(char **argv, int num, int pid, int status)
{
    // printf("make_job");
    job *temp = (job *)malloc(sizeof(struct process));
    temp->name[0] = '\0';
    for (int i = 0; argv[i]; i++)
        strcat(temp->name, argv[i]);
    temp->pid = pid;
    temp->num = num;
    temp->status = status;
    temp->link = NULL;
    return (temp);
}

int find_job_num()
{
    // printf("find_job_num\n");
    if (head == NULL)
    {
        return (1);
    }
    else
    {
        job *cur = head;
        while (cur->link)
        {
            cur = cur->link;
        }
        return (cur->num + 1);
    }
}

void add_job(char **argv, int pid, int status)
{
    // printf("add_job\n");
    int num = find_job_num();
    job *new_process = make_job(argv, num, pid, status);
    if (head == NULL)
    {
        head = new_process;
    }
    else
    {
        job *cur = head;
        while (cur->link)
        {
            cur = cur->link;
        }
        cur->link = new_process;
    }
}

job *find_job_from_pid(int pid)
{
    // printf("find_job_from_pid\n");
    job *cur = head;
    while (cur)
    {
        if (cur->pid == pid)
            return (cur);
        cur = cur->link;
    }
    return (0);
}

void delete_job_from_pid(int pid)
{
    // printf("delete_job_from_pid\n");
    job *temp = head;
    job *prev = NULL;

    if (temp == NULL)
        return;

    if (temp->pid == pid)
    {
        head = temp->link;
        free(temp);
        return;
    }

    while (temp != NULL && temp->pid != pid)
    {
        prev = temp;
        temp = temp->link;
    }

    if (temp == NULL)
        return;

    prev->link = temp->link;
    free(temp);
}

void CHLD_handler(int sig)
{
    // Sio_puts("CHLD_handler\n");
    sigset_t mask_all, prev_all;
    pid_t pid;
    job *temp;
    Sigfillset(&mask_all);
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    { /* Reap child */
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        if (fg_job)
        {
            if (fg_job->pid == pid)
            {
                // Sio_puts("CHLD_handler killing\n");
                delete_job_from_pid(pid);
                fg_job = 0;
            }
        }
        delete_job_from_pid(pid);
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
}

void INT_handler(int sig)
{
    if (fg_job)
    {
        // Sio_puts("INT_handler killing\n");
        kill(fg_job->pid, SIGKILL);
    }
    Sio_puts("\n");
}

void TSTP_handler(int sig)
{
    if (fg_job != 0)
    {
        // Sio_puts("TSTP_handler killing\n");
        kill(fg_job->pid, SIGTSTP);
        fg_job->status = SUSPENDED;
        fg_job = 0;
    }
    Sio_puts("\n");
}

int main()
{
    char cmdline[MAXLINE]; /* Command line */
    char *ret;
    ret = getcwd(history_path, sizeof(history_path));
    strcat(history_path, "/history.txt");

    signal(SIGINT, INT_handler); // Signal Handler들
    signal(SIGTSTP, TSTP_handler);
    signal(SIGCHLD, CHLD_handler);
    while (1)
    {
        /* Read */
        // signal masking
        printf("> ");
        ret = fgets(cmdline, MAXLINE, stdin);
        // 원복
        if (feof(stdin))
            exit(0);

        /* Evaluate */
        eval(cmdline);
    }
    return (0);
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void pipe_recursive(char **argv, int *ins_s_idx, int current_idx, int ins_n)
{
    int fd[2];
    pid_t pid;

    if (current_idx < ins_n - 1)
    {
        int ret;
        ret = pipe(fd);
        if ((pid = Fork()) == 0)
        {
            close(STDOUT_FILENO);
            dup2(fd[1], STDOUT_FILENO);
            close(fd[1]);
            close(fd[0]);
            if (!builtin_command(&argv[ins_s_idx[current_idx]]))
            {
                if (execvp(argv[ins_s_idx[current_idx]], &argv[ins_s_idx[current_idx]]) < 0)
                {
                    printf("Command not found\n");
                    exit(0);
                }
            }
            exit(0);
        }
        else
        {
            close(STDIN_FILENO);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            close(fd[1]);
            pipe_recursive(argv, ins_s_idx, current_idx + 1, ins_n);
            int status;
            waitpid(pid, &status, 0);
            exit(0);
        }
    }
    else
    {
        if (!builtin_command(&argv[ins_s_idx[current_idx]]))
        {
            if (execvp(argv[ins_s_idx[current_idx]], &argv[ins_s_idx[current_idx]]) < 0)
            {
                printf("Command not found\n");
                exit(0);
            }
        }
        exit(0);
    }
}

void write_to_history(char *cmdline)
{
    FILE *fwp = fopen(history_path, "a");
    FILE *frp = fopen(history_path, "r");

    char lastline[MAXLINE];
    while (fgets(lastline, MAXLINE, frp))
        ;
    lastline[strcspn(lastline, "\n")] = '\0';
    cmdline[strcspn(cmdline, "\n")] = '\0';
    if (strcmp(lastline, cmdline) != 0)
        fprintf(fwp, "%s\n", cmdline);
    fclose(fwp);
    fclose(frp);
}

void replace_exclam(char *cmdline)
{
    int exclam_idx;
    int n;
    if ((exclam_idx = strcspn(cmdline, "!")) != strlen(cmdline))
    {
        FILE *frp = fopen(history_path, "r");
        char buffer[MAXLINE];
        char after[MAXLINE];

        int digit = 0;
        if (cmdline[exclam_idx + 1] == '!')
        {
            while (fgets(buffer, MAXLINE, frp))
                ;
            digit = 1;
        }
        else
        {
            n = atoi(cmdline + exclam_idx + 1); // get the line number from the command string
            int i = 0;
            while (fgets(buffer, MAXLINE, frp))
            {
                i++;
                if (i == n)
                    break; // found the nth line
            }
            int num = n;
            if (num == 0)
                return;
            while (num != 0)
            {
                num /= 10;
                ++digit;
            }
        }
        buffer[strcspn(buffer, "\n")] = '\0';
        strcpy(after, cmdline + exclam_idx + 1 + digit);
        cmdline[exclam_idx] = '\0';
        strcat(cmdline, buffer);
        strcat(cmdline, after);
        fclose(frp);
        replace_exclam(cmdline);
    }
}

void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    // write history before parsing
    int status;
    replace_exclam(cmdline);
    memset(argv, 0, sizeof(argv));
    int j = 0;
    for (int i = 0; i < strlen(cmdline); i++)
    {
        if (cmdline[i] == '|' || cmdline[i] == '&')
        {
            buf[j++] = ' ';
            buf[j++] = cmdline[i];
            buf[j++] = ' ';
        }
        else if (cmdline[i] != '\"' && cmdline[i] != '\'')
        {
            buf[j++] = cmdline[i];
        }
    }
    buf[j] = '\0';
    bg = parseline(buf, argv);

    int ins_n = 1;
    int ins_s_idx[MAXARGS];
    // get each instruction start point
    ins_s_idx[0] = 0;
    for (int i = 0; argv[i]; i++)
    {
        if (strcmp(argv[i], "|") == 0)
        {
            argv[i] = NULL;
            ins_s_idx[ins_n++] = i + 1;
        }
    }

    write_to_history(cmdline);

    if (argv[0] == NULL)
        return; /* Ignore empty lines */

    sigset_t mask_all, mask_one, prev_one;
    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);
    // evaluation
    if (ins_n > 1)
    {
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one); /* Block SIGCHLD */
        if ((pid = Fork()) == 0)
        {
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            setpgid(0, 0);
            pipe_recursive(argv, ins_s_idx, 0, ins_n);
        }
        Sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Parent process */
        add_job(argv, pid, RUNNING);             /* Add the child to the job list */
        fg_job = find_job_from_pid(pid);
        Sigprocmask(SIG_SETMASK, &prev_one, NULL);
    }
    else
    {
        if (!builtin_command(argv))
        {                                                 // quit -> exit(0), & -> ignore
                                                          // other -> run
            Sigprocmask(SIG_BLOCK, &mask_one, &prev_one); /* Block SIGCHLD */
            if ((pid = Fork()) == 0)
            {
                Sigprocmask(SIG_SETMASK, &prev_one, NULL);
                setpgid(0, 0);
                if (execvp(argv[0], argv) < 0)
                { // ex) /bin/ls ls -al &
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            }
            Sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Parent process */
            add_job(argv, pid, RUNNING);             /* Add the child to the job list */
            fg_job = find_job_from_pid(pid);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL); /* Unblock SIGCHLD */
        }
    }
    /* Parent waits for foreground job to terminate */

    if (!bg)
    {
        if (pid)
        {
            sigset_t mask, prev;
            Sigemptyset(&mask);
            Sigaddset(&mask, SIGCHLD);
            Sigprocmask(SIG_BLOCK, &mask, &prev);
            while (fg_job)
                Sigsuspend(&prev);
            Sigprocmask(SIG_SETMASK, &prev, NULL);
        }
    }
    else
    {
        if (pid)
        {
            fg_job = 0;
            job *temp = find_job_from_pid(pid);
            printf("[%d] %s\n", temp->num, temp->name);
        }
    }
    return;
}

void bg_manager(char *num)
{
    if (num[0] == '%')
    {
        int n = atoi(++num);
        job *cur = head;
        while (cur)
        {
            if (cur->num == n)
            {
                kill(cur->pid, SIGCONT);
                cur->status = RUNNING;
                return;
            }
            cur = cur->link;
        }
    }
    printf("no such job\n");
}

void fg_manager(char *num)
{
    if (num[0] == '%')
    {
        int n = atoi(++num);
        job *cur = head;
        while (cur)
        {
            if (cur->num == n)
            {
                kill(cur->pid, SIGCONT);
                fg_job = cur;
                cur->status = RUNNING;
                sigset_t mask, prev;
                Sigemptyset(&mask);
                Sigaddset(&mask, SIGCHLD);
                Sigprocmask(SIG_BLOCK, &mask, &prev);
                while (fg_job)
                    Sigsuspend(&prev);
                Sigprocmask(SIG_SETMASK, &prev, NULL);
                return;
            }
            cur = cur->link;
        }
    }
    printf("no such job\n");
}

void kill_manager(char *num)
{
    if (num[0] == '%')
    {
        int n = atoi(++num);
        job *cur = head;
        while (cur)
        {
            if (cur->num == n)
            {
                kill(cur->pid, SIGKILL);
                return;
            }
            cur = cur->link;
        }
    }
    printf("no such job\n");
}

void jobs_manager(char **argv)
{
    if (head == NULL)
    {
        return;
    }
    else
    {
        printf("num\t   status\t command\n");
        job *cur = head;
        while (cur)
        {
            printf("[%d]", cur->num);
            if (cur->status == RUNNING)
                printf("       running");
            if (cur->status == SUSPENDED)
                printf("     suspended");
            printf("      %10s\n", cur->name);
            cur = cur->link;
        }
    }
}
/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv)
{
    pid_t pid;
    int status;

    if (!strcmp(argv[0], "exit")) /* quit command */
        exit(0);
    if (!strcmp(argv[0], "&")) /* Ignore singleton & */
        return 1;

    if (strcmp(argv[0], "jobs") == 0)
    {
        jobs_manager(argv);
        return (1);
    }

    if (strcmp(argv[0], "bg") == 0)
    {
        bg_manager(argv[1]);
        return (1);
    }

    if (strcmp(argv[0], "fg") == 0)
    {
        fg_manager(argv[1]);
        return (1);
    }

    if (strcmp(argv[0], "kill") == 0)
    {
        kill_manager(argv[1]);
        return (1);
    }

    if (strcmp(argv[0], "cd") == 0)
    {
        int ret;
        if (argv[1])
        {
            if (strcmp(argv[1], "~") == 0)
            {
                argv[1] = '\0';
            }
            else
            {
                char *path = (char *)malloc(strlen("./") + strlen(argv[1]) + 1);
                strcat(path, "./");
                strcat(path, argv[1]);
                ret = chdir(path);
            }
        }
        if (argv[1] == 0)
        {
            ret = chdir(getenv("HOME"));
        }
        return (1);
    }

    if (strcmp(argv[0], "history") == 0)
    {
        FILE *frp = fopen(history_path, "r");
        char buffer[MAXLINE];
        int i = 0;
        while (fgets(buffer, MAXLINE, frp) != NULL)
        {
            if (buffer[0] == '\n') // 빈 줄일 경우 무시
                continue;
            printf("%5d  %s", ++i, buffer);
        }
        fclose(frp);
        return (1);
    }
    return 0; /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim; /* Points to first space delimiter */
    int argc;    /* Number of args */
    int bg;      /* Background job? */

    buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' ')))
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0) /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}
/* $end parseline */
