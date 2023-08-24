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
                execvp(argv[ins_s_idx[current_idx]], &argv[ins_s_idx[current_idx]]);
                printf("execvp error from child\n");
                exit(1);
            }
        }
        else
        {
            close(STDIN_FILENO);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            close(fd[1]);
            pipe_recursive(argv, ins_s_idx, current_idx + 1, ins_n);
            waitpid(pid, NULL, 0);
            exit(0);
        }
    }
    else
    {
        if (!builtin_command(&argv[ins_s_idx[current_idx]]))
        {
            execvp(argv[ins_s_idx[current_idx]], &argv[ins_s_idx[current_idx]]);
            printf("execvp error from last child\n");
            exit(1);
        }
    }
}
