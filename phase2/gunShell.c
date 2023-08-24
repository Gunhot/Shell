/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#define MAXARGS 128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
char history_path[1024];

int main()
{
    char cmdline[MAXLINE]; /* Command line */
    char *ret;
    ret = getcwd(history_path, sizeof(history_path));
    strcat(history_path, "/history.txt");
    while (1)
    {
        /* Read */
        printf("> ");
        ret = fgets(cmdline, MAXLINE, stdin);
        //
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
    int status;
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

void repalce_exclam(char *cmdline)
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
        repalce_exclam(cmdline);
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
    repalce_exclam(cmdline);
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

    if (ins_n > 1)
    {
        if ((pid = Fork()) == 0)
        {
            pipe_recursive(argv, ins_s_idx, 0, ins_n);
        }
    }
    else
    {
        if (!builtin_command(argv))
        { // quit -> exit(0), & -> ignore
            // other -> run
            if ((pid = Fork()) == 0)
            {
                if (execvp(argv[0], argv) < 0)
                { // ex) /bin/ls ls -al &
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            }
        }
    }
    /* Parent waits for foreground job to terminate */
    if (!bg)
    {
        if (pid)
        {
            int status;
            waitpid(pid, &status, 0);
        }
    }
    else // when there is backgrount process!
    {
        if (pid)
        {
            printf("%d %s", pid, cmdline);
        }
    }

    return;
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
