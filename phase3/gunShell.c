/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#define MAXARGS 128
#define RUNNING 0
#define SUSPENDED 1

// job들을 저장할 자료구조와 함수
typedef struct process
{
    char name[100];
    int num;
    pid_t pid;
    int status;
    struct process *link;
} job;
job *head;
job *fg_job;
void add_job(char **argv, int pid, int status);
job *make_job(char **argv, int num, int pid, int status);
int find_job_num();
// foreground job을 가리키고 있다.

// SIGhandler Set
job *find_job_from_pid(int pid);
void delete_job_from_pid(int pid);
void INT_handler(int sig);
void TSTP_handler(int sig);
void CHLD_handler(int sig);

// pipe Set
int pipe_flag;
char pipe_job_command[100];
void pipe_recursive(char **argv, int *ins_s_idx, int current_idx, int ins_n);

// phase 1
void write_to_history(char *cmdline);
void replace_exclam(char *cmdline);
char history_path[1024];

/* Given Function*/
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

job *make_job(char **argv, int num, int pid, int status)
{
    job *temp = (job *)malloc(sizeof(struct process));
    temp->name[0] = '\0';
    // 이름처리만 파이프와 파이프가 아닌 경우를 나눠서 처리한다.
    // 파이프 명령어일 경우
    if (pipe_flag == 1)
    {
        pipe_job_command[strlen(pipe_job_command) - 1] = '\0';
        pipe_job_command[strcspn(pipe_job_command, "&")] = '\0';
        strcpy(temp->name, pipe_job_command);
    }
    else
    {
        for (int i = 0; argv[i]; i++)
            strcat(temp->name, argv[i]);
    }
    temp->pid = pid;
    temp->num = num;
    temp->status = status;
    temp->link = NULL;
    return (temp);
}

int find_job_num()
{
    // 추가되는 job의 num을 구해준다.
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
    // job을 자료구조에 추가한다.
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
    // 주어진 pid를 갖는 job을 return한다.
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
    // 주어진 pid를 갖는 job을 삭제한다.
    job *temp = head;
    job *prev = NULL;

    // 아무것도 없을 경우
    if (temp == NULL)
        return;

    // 삭제하는 것이 head일 경우
    if (temp->pid == pid)
    {
        head = temp->link;
        free(temp);
        return;
    }

    // pid를 가지는 node를 찾는다
    while (temp != NULL && temp->pid != pid)
    {
        prev = temp;
        temp = temp->link;
    }

    // 해당 pid를 가지는 노드가 없을 경우
    if (temp == NULL)
        return;

    // 삭제
    prev->link = temp->link;
    free(temp);
}

void CHLD_handler(int sig)
{
    sigset_t mask_all, prev_all;
    pid_t pid;
    job *temp;
    Sigfillset(&mask_all);
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    { /* Reap child */
        // 처리할 동안에는 signal을 block해놓는다.
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        if (fg_job)
        {
            // 삭제된 것이 foreground process일 경우
            if (fg_job->pid == pid)
            {
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
    sigset_t mask_one, prev_one;
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGINT);
    //아래 명령어를 처리 중에 signal을 놓칠 수 있기 때문에 block 해놓는다.
    Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);
    if (fg_job)
    {
        kill(fg_job->pid, SIGKILL);
        Sio_puts("\n");
    }
    else
    {
        Sio_puts("\n> ");
    }
    Sigprocmask(SIG_SETMASK, &prev_one, NULL);
}

void TSTP_handler(int sig)
{
    sigset_t mask_one, prev_one;
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGTSTP);
    //아래 명령어를 처리 중에 signal을 놓칠 수 있기 때문에 block 해놓는다.
    Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);
    if (fg_job != 0)
    {
        kill(fg_job->pid, SIGTSTP);
        fg_job->status = SUSPENDED;
        // CHLD_handler에서 처리해주지 못하기 때문에 while문의 탈출조건을 직접 설정한다.
        fg_job = 0;
    }
    Sigprocmask(SIG_SETMASK, &prev_one, NULL);
}

int main()
{
    char cmdline[MAXLINE]; /* Command line */
    char *ret;
    ret = getcwd(history_path, sizeof(history_path));
    strcat(history_path, "/history.txt");
    // Signal Handler들
    signal(SIGINT, INT_handler);
    signal(SIGTSTP, TSTP_handler);
    signal(SIGCHLD, CHLD_handler);

    while (1)
    {
        /* Read */
        printf("> ");
        ret = fgets(cmdline, MAXLINE, stdin);
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
    sigset_t mask_all, mask_one, prev_one;
    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);
    if (current_idx < ins_n - 1)
    {
        int ret;
        ret = pipe(fd);
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one); /* Block SIGCHLD 너무 빨리 죽을까봐 */
        if ((pid = Fork()) == 0)
        {
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);//막아놓은 SIGCHLD 해제

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
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);

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
        //마지막 recursive는 child process 생성 없이 바로 실행 후 종료되게 한다. 
        //이로 인해 마지막 프로세스부터 안정적으로 reaping이 가능해진다.
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
    //엔터만 입력됐을 경우
    if(cmdline[0] == '\n')
        return ;
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
            //마지막 줄
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
                //n번째 줄
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
        //찾아온 명령어로 치환한다.
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
    // my parsing
    // 따옴표 처리 + 파이프, &의 공백처리
    for (int i = 0; i < strlen(cmdline); i++)
    {
        if (cmdline[i] == '\'' || cmdline[i] == '\"')
        {
            char c = cmdline[i];
            i++;
            while (cmdline[i] && (cmdline[i] != c)) /* Ignore spaces */
            {
                buf[j++] = cmdline[i++];
            }
        }
        else
        {
            if (cmdline[i] == '|' || cmdline[i] == '&')
            {
                buf[j++] = ' ';
                buf[j++] = cmdline[i];
                buf[j++] = ' ';
            }
            else
                buf[j++] = cmdline[i];
        }
    }
    buf[j] = '\0';
    strcpy(pipe_job_command, buf);
    bg = parseline(buf, argv);

    int ins_n = 1;
    int ins_s_idx[MAXARGS];
    // get each instruction start point
    ins_s_idx[0] = 0;
    // pipeline counting
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
    pipe_flag = 0;
    sigset_t mask_all, mask_one, prev_one;
    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD); //너무 빨리 죽는 것 방지
    // pipe
    if (ins_n > 1)
    {
        pipe_flag = 1;
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one); /* Block SIGCHLD */
        if ((pid = Fork()) == 0)
        {
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            // 별도의 pgid 생성
            setpgid(0, 0);
            pipe_recursive(argv, ins_s_idx, 0, ins_n);
        }
        Sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Parent process */
        add_job(argv, pid, RUNNING);             /* Add the child to the job list */
        fg_job = find_job_from_pid(pid);
        Sigprocmask(SIG_SETMASK, &prev_one, NULL);
    }
    // non-pipe
    else
    {
        if (!builtin_command(argv))
        {                                                 // quit -> exit(0), & -> ignore
                                                          // other -> run
            Sigprocmask(SIG_BLOCK, &mask_one, &prev_one); /* Block SIGCHLD */
            if ((pid = Fork()) == 0)
            {
                Sigprocmask(SIG_SETMASK, &prev_one, NULL);
                // 별도의 pgid 생성
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
    // foreground process
    if (!bg)
    {
        // child_process가 생성됐을 경우에만
        if (pid)
        {
            // foreground process가 종료되기를 기다린다.
            sigset_t mask, prev;
            Sigemptyset(&mask);
            Sigaddset(&mask, SIGCHLD);
            Sigprocmask(SIG_BLOCK, &mask, &prev);
            while (fg_job)
                Sigsuspend(&prev);
            Sigprocmask(SIG_SETMASK, &prev, NULL);
        }
    }
    // background process
    else
    {
        // child_process가 생성됐을 경우에만
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
            // 해당 번호의 job 찾기
            if (cur->num == n)
            {
                kill(cur->pid, SIGCONT);
                //상태 최신화
                cur->status = RUNNING;
                printf("[%d]", cur->num);
                printf("       running");
                printf("        %s\n", cur->name);
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
            // 해당 번호의 job 찾기
            if (cur->num == n)
            {
                kill(cur->pid, SIGCONT);
                //fg_job 최신화
                fg_job = cur;
                //상태 최신화
                cur->status = RUNNING;
                printf("[%d]", cur->num);
                printf("       running");
                printf("        %s\n", cur->name);

                // foreground process가 종료되기를 기다린다.
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
            // 해당 번호의 job 찾기
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
    //비어있을 경우 아무것도 출력하지 않는다
    if (head == NULL)
    {
        return;
    }
    else
    {
        job *cur = head;
        //형식에 맞게 출력
        while (cur)
        {
            printf("[%d]", cur->num);
            if (cur->status == RUNNING)
                printf("       running");
            if (cur->status == SUSPENDED)
                printf("     suspended");
            printf("        %s\n", cur->name);
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
            // home directory
            if (strcmp(argv[1], "~") == 0)
            {
                // home directory로 갈 수 있게 한다.
                argv[1] = '\0';
            }
            // 특정 directory
            else
            {
                char *path = (char *)malloc(strlen("./") + strlen(argv[1]) + 1);
                strcat(path, "./");
                strcat(path, argv[1]);
                ret = chdir(path);
            }
        }
        // home directory
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
        // 파일 한줄 씩 읽기
        while (fgets(buffer, MAXLINE, frp) != NULL)
        {
            printf("%5d  %s", ++i, buffer);
        }
        fclose(frp);
        return (1);
    }
    return 0; /* Not a builtin command */
}
/* $end eval */

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
