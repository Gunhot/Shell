/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#define MAXARGS 128
char history_path[1024];

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

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

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    // write history before parsing
    repalce_exclam(cmdline);
    memset(argv, 0, sizeof(argv));
    int j = 0;
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
            i++;
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
    bg = parseline(buf, argv);
    write_to_history(cmdline);

    if (argv[0] == NULL)
        return; /* Ignore empty lines */
    if (!builtin_command(argv))
    { // quit -> exit(0), & -> ignore
      // other -> run
        if ((pid = Fork()) == 0)
        {
            // make the command start wtih "/bin/"
            if (execvp(argv[0], argv) < 0)
            { // ex) /bin/ls ls -al &
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }

        /* Parent waits for foreground job to terminate */
        if (!bg)
        {
            // p.42 참고
            int status;
            waitpid(pid, &status, 0);
        }
        else // when there is backgrount process!
            printf("%d %s", pid, cmdline);
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
