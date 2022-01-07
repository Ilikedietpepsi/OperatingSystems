#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>

int main(void);
static void sighandler(int signum);

pid_t job_list[50] = {0};
int jobs = 0;
int pid;
int output_redirection = 0;
int piping = 0;

int getcmd(char *prompt, char *args[], int *background)
{
    int length = 0;
    int i = 0;
    char *line = NULL;
    char *token = NULL;
    char *input = NULL;
    char *loc = NULL;
    int index = 0;
    size_t linecap = 0;
    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    if (length <= 0)
    {
        exit(-1);
    }
    loc = strrchr(line, '&');
    if (loc != NULL)
    {
        index = loc - line;
        //check if '&' is at the end
        if (index == length - 2)
        {
            *background = 1;
        }

        else
        {

            *background = 0;
        }
    }

    else
    {
        *background = 0;
    }

    while ((token = strsep(&line, " \t\n")) != NULL)
    {
        // if (strcmp("&", token) == 0 && *background == 1)
        // {
        //     continue;
        // }
        for (int j = 0; j < strlen(token); j++)
        {
            if (token[j] <= 32)
            {
                token[j] = '\0';
            }
        }

        if (strlen(token) > 0)
        {
            if (strcmp(token, ">") == 0)
            {
                output_redirection = 1;
            }
            else if (strcmp(token, "|") == 0)
            {
                piping = 1;
            }
            args[i] = token;
            i++;
        }
    }
    if (i != 0 && strcmp(args[i - 1], "&") == 0)
    {
        args[i - 1] = NULL;
        i--;
    }
    args[i] = NULL;

    return i;
}

static void sighandler(int signum)
{
    kill(pid, SIGTERM);
}



static void chldhandler(int signum)
{
    pid_t p;
    p = waitpid(-1, NULL, WNOHANG);
    
    for (int i = 0; i < jobs; i++)
    {
        if (job_list[i] == p)
        {
            job_list[i] = -1;
        }
    }
}

int main(void)
{
    int bg = 0;
    int cnt;
    char *args[20] = {NULL};
    char *path;
    char *args_1[20] = {NULL};
    char *args_2[20] = {NULL};
    int fd[2];
    signal(SIGTSTP, SIG_IGN);
    while (1)
    {

        cnt = getcmd("\n>> ", args, &bg);

        //empty input
        if (cnt == 0)
        {
            continue;
        }
        //output redirection '>'
        else if (output_redirection)
        {
            pid = fork();
            if (pid == 0)
            {
                close(1);
                fopen(args[cnt - 1], "w");
                for (int i = 0; i < cnt - 2; i++)
                {
                    args_1[i] = args[i];
                }
                execvp(args_1[0], args_1);
            }
            else
            {
            
                signal(SIGINT, sighandler);
                waitpid(pid, NULL, 0);
                output_redirection = 0;
            }
        }
        //piping '|'
        else if (piping)
        {
            pipe(fd);
            int i = 0;
            int flag = 1;

            pid = fork();
            if (pid == 0)
            {
                close(1);
                dup(fd[1]);
                close(fd[0]);
                close(fd[1]);
                while (strcmp(args[i], "|") != 0)
                {
                    args_1[i] = args[i];
                    i++;
                }

                execvp(args_1[0], args_1);
            }

            int pid1 = fork();
            if (pid1 == 0)
            {
                close(0);
                dup(fd[0]);
                close(fd[0]);
                close(fd[1]);
                for (int j = 0; j < cnt; j++)
                {
                    if (strcmp(args[j], "|") == 0)
                    {
                        flag = 0;
                    }
                    else if (flag)
                    {
                        continue;
                    }
                    else
                    {
                        args_2[i] = args[j];
                        i++;
                    }
                }

                execvp(args_2[0], args_2);
            }
            close(fd[0]);
            close(fd[1]);
            piping = 0;
            waitpid(pid, NULL, 0);
            waitpid(pid1, NULL, 0);
        }

        //command "exit"
        else if (strcmp(args[0], "exit") == 0)
        {
            for (int i = 0; i < jobs; i++)
            {
                if (job_list[i] != -1 && job_list[i] != 0)
                {
                    kill(job_list[i], SIGKILL);
                }
            }
            return 0;
        }
        //command 'cd'
        else if (strcmp(args[0], "cd") == 0)
        {
            if (chdir(args[1]) != 0)
            {
                perror("No such directory.");
            }
        }
        //command 'pwd'
        else if (strcmp(args[0], "pwd") == 0)
        {
            path = (char *)malloc(MAXPATHLEN);
            getcwd(path, MAXPATHLEN);
            printf("%s\n", path);
            free(path);
        }
        //command 'fg'
        else if (strcmp(args[0], "fg") == 0)
        {
            signal(SIGINT, sighandler);

            if (cnt == 1)
            {
                for (int i = 0; i < jobs; i++)
                {
                    if (job_list[i] != -1 && job_list[i] != 0)
                    {
                        waitpid(job_list[i], NULL, 0);
                        job_list[i] = -1;
                        break;
                    }
                }
            }

            else
            {
                int i = atoi(args[1]);
                int j = 0;
                do
                {
                    if (job_list[j] != -1 && job_list[j] != 0)
                    {
                        i--;
                    }
                    j++;
                } while (i > 0);
                pid = job_list[j - 1];
                
                waitpid(pid, NULL, 0);
                job_list[j - 1] = -1;
            }
        }

        //command 'jobs'
        else if (strcmp(args[0], "jobs") == 0)
        {
            for (int i = 0; i < jobs; i++)
            {
                if (job_list[i] != -1 && job_list[i] != 0)
                {
                    printf("%d\n", job_list[i]);
                }
            }
        }
        //run in background
        else if (bg == 1)
        {
            signal(SIGCHLD, chldhandler);
            signal(SIGINT, SIG_IGN);
            pid = fork();
            if (pid == 0)
            {
                execvp(args[0], args);
        
            }
            else
            {
                job_list[jobs] = pid;
                jobs++;
            }
    
        }
        //run in foreground
        else if (bg == 0)
        {

            pid = fork();
            if (pid == 0)
            {

                execvp(args[0], args);
              
            }
            else
            {
                
                signal(SIGINT, sighandler);
                waitpid(pid, NULL, 0);
            }
        }
    }
    free(args);
    free(path);
    free(args_1);
    free(args_2);

    return 0;
}