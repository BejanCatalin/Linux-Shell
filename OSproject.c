#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stddef.h>

// Necessary global variables
int filedesc;                              //file descriptor
static char *args[512];                    //to store the command in form of separate tokens
static char prompt[512];                   //prompts shell
int backgrnd_flag;                         //set to 1 if its a background
pid_t pid;                                 //stores process id
int tot_lines;                             //stores total lines of command
int output_redirection, input_redirection; //flags for I/O redirection
int status;                                //stores status for child process
char *input_redirection_file;              //if < occurs it means Input redirection
char *output_redirection_file;             //if > occurs it means output redirection
char *history_file;                        // contains all commands entered uptil now
char *input_buffer = NULL;                 // contains user input
size_t size_buffer = 0;                    // size of input buffer
char *cmd_toexec[512];                     //contains current command to execute
int flag, len;                             // flags and length variable
char cmddisp[1024];                        //stores message for command window
int i;
char *token;             // to contain tokens
char *array_tokens[512]; // token array_tokens

// function headers declarations
void init_global_vars();
void display_command_history();
void switch_directory();
char *delete_white_space(char *);
void gen_tokens_using_space(char *);
void redirect_IO(char *);
void redirect_input(char *);
void redirect_output(char *);
char *delete_doublequotes(char *);
static int system_cmd_handler(char *, int, int, int, int, char **);
void gen_token_using_pipe();
static int command_executor(int, int, int, char *);
void interrupt_handler(int);
void backgrnd_process_checker();
void command_handler();
void gen_tokens(char *);
int lineCounter(const char *str);
int wordCounter(const char *str);


char *list_commands[] = {

    "cd",    //a list with all the commands
    "exit",
    "help",
    "wc , with parameters -c, -w, -l,-L",
    "tee , with parameter -a"};


int num_commands()
{
    return sizeof(list_commands) / sizeof(char *);    //the number of commands
}

void help()
{
    int i;
    printf("-------------------------Welcome to my shell-------------------------\n");
    printf("The following are built-in commands\n");                             //help function

    for (i = 0; i < num_commands(); i++)
    {
        printf(" %s\n\n", list_commands[i]);
    }
}

/* This function handles the interrupt signals */
void interrupt_handler(int sig_num)
{
    signal(SIGINT, interrupt_handler);
    fflush(stdout);
    return;
}

/* This function initializes the global variables */
void init_global_vars()
{
    filedesc = 0;
    flag = 0;
    len = 0;
    tot_lines = 0;
    output_redirection = 0;
    input_redirection = 0;
    cmddisp[0] = '\0';
    prompt[0] = '\0';
    pid = 0;
}

/* This function prints the comand history when "history" command is given */
void display_command_history()
{
    register HIST_ENTRY **the_list;
    register int i;
    the_list = history_list();
    if (the_list)
        for (i = 0; the_list[i]; i++)
            printf("%d: %s\n", i + history_base, the_list[i]->line);
    return;
}

/* This function is used to displays the Shell Prompt */
void display_shell()
{
    if (getcwd(cmddisp, sizeof(cmddisp)) != NULL)
    {
        strcpy(prompt, "My_Shell: ");
        strcat(prompt, cmddisp);
        strcat(prompt, "$ ");
    }
    else
    {
        perror("Error obtaining the curent working directory: ");
    }
    return;
}

/* This function is used to delete the white spaces in the input string */
char *delete_white_space(char *str)
{
    int i = 0, j = 0;
    char *temp;
    if (NULL == (temp = (char *)malloc(sizeof(str) * sizeof(char))))
    {
        perror("Memory Error: ");
        return NULL;
    }
    while (str[i++])
    {
        if (str[i - 1] != ' ')
            temp[j++] = str[i - 1];
    }
    temp[j] = '\0';
    return temp;
}

/* This function is used to delete out the double quote characters {"} in the
input string */
char *delete_doublequotes(char *str)
{
    int i = 0, j = 0;
    char *temp;
    if (NULL == (temp = (char *)malloc(sizeof(str) * sizeof(char))))
    {
        perror("Memory Error: ");
        return NULL;
    }
    while (str[i++])
    {
        if (str[i - 1] != '"')
            temp[j++] = str[i - 1];
    }
    temp[j] = '\0';
    return temp;
}

/* This function is used to change directory when "cd" command is 
executed */
void switch_directory()
{
    char *home_dir = "/home";
    if ((args[1] == NULL) || (!(strcmp(args[1], "~") && strcmp(args[1], "~/"))))
        chdir(home_dir);
    else if (chdir(args[1]) < 0)
        perror("No such file or directory: ");
}


/* This function is used when "tee / tee -a" commands are executed*/
int tee(int argc, char *args[])
{
    FILE *file;
    int aflag = 0, c;
    size_t len = 0;
    char *line = NULL;
    while ((c = getopt(argc, args, "a")) != -1)
        switch (c)
        {

        case 'a':
            aflag = 1;

            break;
        case '?':
            printf("This option is not known\n");
            break;
        }

    while (getline(&line, &len, stdin) > 1)
    {
        printf("%s", line);
        for (int i = optind; i < argc; i++)
        {
            if (aflag)
            {
                file = fopen(args[i], "a");
            }
            else
            {
                if (i == 0)
                    file = fopen(args[i], "w");
                else
                    file = fopen(args[i], "a");
            }
            fputs(line, file);
            fclose(file);
        }
    }

    free(line);
    return 0;
}


/* This function is used when "wc / wc -c / wc -w / wc -l / wc -L" commands are executed*/
void wc(int mode, char *path)
{
    if (mode == 0)   
    {

        if (path == NULL)
        {

            int lines = 0;
            int words = 0;
            int length = 0;
            char *pipeBuf = (char *)calloc(5000, sizeof(char *));

            fread(pipeBuf, 4500, 1, stdin);

            for (int i = 0; i < 5000; i++)     //no parameter
            {
                if (pipeBuf[i] == '\0')
                {
                    words++;
                    lines++;
                    break;
                }
                if (pipeBuf[i] == ' ')
                {
                    words++;
                }
                if (pipeBuf[i] == '\n')
                {
                    words++;
                    lines++;
                }
                length++;
            }

            printf("%d    %d    %d \n", lines, words, length);

            free(pipeBuf);
        }

        else
        {
            int lines = 0;
            int words = 0;
            int length = 0;
            FILE *fp = NULL;
            fp = fopen(path, "r");
            fseek(fp, 0, SEEK_END);
            length = ftell(fp);
            rewind(fp);
            char *buffer = calloc(1, length + 1);
            fread(buffer, length, 1, fp);
            lines = lineCounter(buffer);
            words = wordCounter(buffer);
            printf("%d    %d    %d\n", lines, words, length);
            fclose(fp);
            free(buffer);
        }
    }

    else if (mode == 1)    // -l parameter
    {
        if (path == NULL)
        {

            int lines = 0;
            int words = 0;
            int length = 0;
            char *pipeBuf = (char *)calloc(5000, sizeof(char *));

            fread(pipeBuf, 4500, 1, stdin);

            for (int i = 0; i < 5000; i++)
            {
                if (pipeBuf[i] == '\0')
                {
                    words++;
                    lines++;
                    break;
                }
                if (pipeBuf[i] == ' ')
                {
                    words++;
                }
                if (pipeBuf[i] == '\n')
                {
                    words++;
                    lines++;
                }
                length++;
            }
            printf("%d   \n", lines);
            free(pipeBuf);
        }

        else
        {
            int lines = 0;
            int length = 0;
            FILE *fp = NULL;
            fp = fopen(path, "r");
            fseek(fp, 0, SEEK_END);
            length = ftell(fp);
            rewind(fp);
            char *buffer = calloc(1, length + 1);
            fread(buffer, length, 1, fp);
            lines = lineCounter(buffer);
            printf("%d\n", lines);
            fclose(fp);
            free(buffer);
        }
    }

    else if (mode == 2)    // -w parameter
    {
        if (path == NULL)
        {

            int lines = 0;
            int words = 0;
            int length = 0;
            char *pipeBuf = (char *)calloc(5000, sizeof(char *));

            fread(pipeBuf, 4500, 1, stdin);

            for (int i = 0; i < 5000; i++)
            {
                if (pipeBuf[i] == '\0')
                {
                    words++;
                    lines++;
                    break;
                }
                if (pipeBuf[i] == ' ')
                {
                    words++;
                }
                if (pipeBuf[i] == '\n')
                {
                    words++;
                    lines++;
                }
                length++;
            }
            printf("%d   \n", lines);
            free(pipeBuf);
        }

        else
        {
            int words = 0;
            int length = 0;
            FILE *fp = NULL;
            fp = fopen(path, "r");
            fseek(fp, 0, SEEK_END);
            length = ftell(fp);
            rewind(fp);
            char *buffer = calloc(1, length + 1);
            fread(buffer, length, 1, fp);
            words = wordCounter(buffer);
            printf("%d\n", words);
            fclose(fp);
            free(buffer);
        }
    }

    else if (mode == 3)    // -c parameter
    {
        if (path == NULL)
        {

            int lines = 0;
            int words = 0;
            int length = 0;
            char *pipeBuf = (char *)calloc(5000, sizeof(char *));

            fread(pipeBuf, 4500, 1, stdin);

            for (int i = 0; i < 5000; i++)
            {
                if (pipeBuf[i] == '\0')
                {
                    words++;
                    lines++;
                    break;
                }
                if (pipeBuf[i] == ' ')
                {
                    words++;
                }
                if (pipeBuf[i] == '\n')
                {
                    words++;
                    lines++;
                }
                length++;
            }
            printf("%d   \n", lines);
            free(pipeBuf);
        }

        else
        {
            int length = 0;
            FILE *fp = NULL;
            fp = fopen(path, "r");
            fseek(fp, 0, SEEK_END);
            length = ftell(fp);
            rewind(fp);
            printf("%d\n", length);
            fclose(fp);
        }
    }
    return;
}


int lineCounter(const char *str)    // count the lines 
{
    int count = 0;
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '\n')
        {
            count++;
        }
    }

    return count;
}

int wordCounter(const char *str)    // count the words
{
    int count = 0;
    int i = 0;
    while (str[i] != '\0')
    {
        if (str[i] == ' ' || str[i] == '\n')
        {
            i++;
        }
        else
        {
            break;
        }
    }
    for (int j = i; str[j] != '\0'; j++)
    {
        if (str[j] == ' ' || str[j] == '\n')
        {
            count++;
            while (str[j] == ' ' || str[j] == '\n')
            {
                j++;
            }
        }
    }
    return count;
}


/* This function is used to execute the systemcommands. It also calls 
the "command_executor" function when the command to be executed doesn't
fall under systemcommands */
static int system_cmd_handler(char *cmd_toexec, int input, int isfirst, int islast, int argc, char** args)
{
    char *new_cmd_toexec;
    new_cmd_toexec = strdup(cmd_toexec); //used to duplicate a string.
    printf(" new_cmd_toexec=%s \n ", new_cmd_toexec);
    gen_tokens_using_space(cmd_toexec);
    backgrnd_process_checker();
    if (args[0] != NULL)
    {
        if (!(strcmp(args[0], "exit") && strcmp(args[0], "quit")))
            exit(0);
        
        else if (!strcmp("cd", args[0]))
        {
            switch_directory();
            return 1;
        }
        else if(!strcmp("wc", args[0])){
            if (argc > 2){
        if (strcmp(args[1], "-l") == 0)
        {
            wc(1, args[2]);
        }
        else if (strcmp(args[1], "-w") == 0)
        {
            wc(2, args[2]);
        }
        else if (strcmp(args[1], "-c") == 0)
        {
            wc(3, args[2]);
        }
        else
        {
            printf("Invalid arguments\n");
        }
        return 1;
    }
    else if (argc == 2)
    {
        if (strcmp(args[1], "") == 0)
        {
            wc(0, NULL);
            return 0;
        }
        if (strcmp(args[1], "-l") == 0)
        {
            wc(1, NULL);
        }
        else if (strcmp(args[1], "-w") == 0)
        {
            wc(2, NULL);
        }
        else if (strcmp(args[1], "-c") == 0)
        {
            wc(3, NULL);
        }
        else
        {
            wc(0, args[1]);
        }
        return 1;
    }
    return 1;
        }
        else if (!strcmp(args[0], "history"))
        {
            display_command_history();
            return 1;
        }
        else if (!strcmp("tee",args[0])){
            tee(argc,args);
            return 1;
        }
    }
    printf(" input=%d ,isfirst=%d,islast=%d,new_cmd_exc=%s \n ", input, isfirst, islast, new_cmd_toexec);
    return (command_executor(input, isfirst, islast, new_cmd_toexec));
}

/* Execute a command */
void command_handler()
{
    int pid = fork(); // Create a new process
    if (pid != 0)
    { // If not successfully completed
        int s;
        waitpid(-1, &s, 0); // Wait for process termination
    }
    else
    {
         if (execvp(array_tokens[0], array_tokens) == -1)
        {       // -1 => something went wrong! else command successfully completed * /
                perror("Wrong command"); // Display error message
            exit(errno);
        }
    }
}

/* This function is used to parse the input when both input redirection 
"<" and output redirection ">" are present */
void redirect_IO(char *cmd_toexec)
{
    char *val[128];
    char *new_cmd_toexec, *s1, *s2;
    new_cmd_toexec = strdup(cmd_toexec);
    int m = 1;
    val[0] = strtok(new_cmd_toexec, "<");
    while ((val[m] = strtok(NULL, ">")) != NULL)
        m++;
    s1 = strdup(val[1]);
    s2 = strdup(val[2]);
    input_redirection_file = delete_white_space(s1);
    output_redirection_file = delete_white_space(s2);
    gen_tokens_using_space(val[0]);
    return;
}

/* This function is used to parse the input if input redirection
"<" occurs */
void redirect_input(char *cmd_toexec)
{
    char *val[128];
    char *new_cmd_toexec, *s1;
    new_cmd_toexec = strdup(cmd_toexec);
    int m = 1;
    val[0] = strtok(new_cmd_toexec, "<");
    while ((val[m] = strtok(NULL, "<")) != NULL)
        m++;
    s1 = strdup(val[1]);
    input_redirection_file = delete_white_space(s1);
    gen_tokens_using_space(val[0]);
    return;
}

/* This function is used to parse the input if output redirection
">" occurs */
void redirect_output(char *cmd_toexec)
{
    char *val[128];
    char *new_cmd_toexec, *s1;
    new_cmd_toexec = strdup(cmd_toexec);
    int m = 1;
    val[0] = strtok(new_cmd_toexec, ">");
    while ((val[m] = strtok(NULL, ">")) != NULL)
        m++;
    s1 = strdup(val[1]);
    output_redirection_file = delete_white_space(s1);
    gen_tokens_using_space(val[0]);
    return;
}

/* This function is used to displays pipe and execute the non-system
commands using execvp */
static int command_executor(int input, int first, int last, char *cmd_toexec)
{
    int mypipefiledesc[2], ret, input_filedesc, output_filedesc;
    if (-1 == (ret = pipe(mypipefiledesc)))
    {
        perror("pipe error: ");
        return 1;
    }
    pid = fork();
    if (pid == 0)
    {
        if (first == 1 && last == 0 && input == 0)
        {
            dup2(mypipefiledesc[1], 1); //dup2 creates a copy of file descriptor
        }
        else if (first == 0 && last == 0 && input != 0)
        {
            dup2(input, 0);
            dup2(mypipefiledesc[1], 1);
        }
        else
        {
            dup2(input, 0);
        }
        if (strchr(cmd_toexec, '<') && strchr(cmd_toexec, '>'))
        {
            input_redirection = 1;
            output_redirection = 1;
            redirect_IO(cmd_toexec);
        }
        else if (strchr(cmd_toexec, '<'))
        {
            input_redirection = 1;
            redirect_input(cmd_toexec);
        }
        else if (strchr(cmd_toexec, '>'))
        {
            output_redirection = 1;
            redirect_output(cmd_toexec);
        }
        if (output_redirection)
        {
            if ((output_filedesc = creat(output_redirection_file, 0644)) < 0)
            {
                fprintf(stderr, "Failed to open %s for writing\n", output_redirection_file);
                return (EXIT_FAILURE);
            }
            dup2(output_filedesc, 1);
            close(output_filedesc);
            output_redirection = 0;
        }
        if (input_redirection)
        {
            if ((input_filedesc = open(input_redirection_file, O_RDONLY, 0)) < 0)
            {
                fprintf(stderr, "Failed to open %s for reading\n", input_redirection_file);
                return (EXIT_FAILURE);
            }
            dup2(input_filedesc, 0);
            close(input_filedesc);
            input_redirection = 0;
        }
        backgrnd_process_checker();
        exit(0);
    }
    else
    {
        if (backgrnd_flag == 0)
            waitpid(pid, 0, 0);
    }
    if (last == 1)
        close(mypipefiledesc[0]);
    if (input != 0)
        close(input);
    close(mypipefiledesc[1]);
    return (mypipefiledesc[0]);
}

/* This function generates token using the input string based on white-space [" "] */
void gen_tokens_using_space(char *str)
{
    int m = 1;
    args[0] = strtok(str, " ");
    while ((args[m] = strtok(NULL, " ")) != NULL)
        m++;
    args[m] = NULL;
}

/* This is function is used to check whether the process is
foreground or background */
void backgrnd_process_checker()
{
    int i = 0;
    backgrnd_flag = 0;
    while (args[i] != NULL)
    {
        if (!strcmp(args[i], "&"))
        {
            backgrnd_flag = 1;
            args[i] = NULL;
            break;
        }
        i++;
    }
}

/* This function generates token from the input string using pipe ["|"] */
void gen_token_using_pipe(int argc, char** args)
{
    int i, n = 1, input = 0, first = 1;
    cmd_toexec[0] = strtok(input_buffer, "|");
    while ((cmd_toexec[n] = strtok(NULL, "|")) != NULL)
        n++;
    cmd_toexec[n] = NULL;
    //first=1 if it's the first command of pipe and last=1 if it's the last.
    for (i = 0; i < n - 1; i++)
    {
        input = system_cmd_handler(cmd_toexec[i], input, first, 0, argc, args);
        first = 0;
    }
    input = system_cmd_handler(cmd_toexec[i], input, first, 1, argc, args);
    return;
}

int main(int argc, char** args)
{
    int status;
    system("clear");
    signal(SIGINT, interrupt_handler);
    char new_line = 0;
    using_history();
    help();

    do
    {
        init_global_vars();
        display_shell();
        input_buffer = readline(prompt);
        if (strcmp(input_buffer, "\n"))
            add_history(input_buffer);
        if (!(strcmp(input_buffer, "\n") && strcmp(input_buffer, "")))
            continue;
        if (!(strncmp(input_buffer, "exit", 4) && strncmp(input_buffer, "quit", 4)))
        {
            flag = 1;
            break;
        }
        gen_token_using_pipe(argc, args);
        if (backgrnd_flag == 0)
            waitpid(pid, &status, 0);
        else
            status = 0;
    } while (!WIFEXITED(status) || !WIFSIGNALED(status));
    if (flag == 1)
    {
        printf("\nExiting Shell!!\n");
        exit(0);
    }
    return 0;
}
