#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

int fgmode = 0; // is foreground-only mode active

void catchSIGTSTP (int signo) { // Signal handling function for SIGTSTP, changes in and out of foreground only mode
    if (fgmode) { // if currently in foreground-only mode
        fgmode = 0;
        char* exitMessage = "\nExiting foreground-only mode\n: ";
        write(STDOUT_FILENO, exitMessage, 30);
    } else { // if not in foreground-only mode
        fgmode = 1;
        char* enterMessage = "\nEntering foreground-only mode (& is now ignored)\n: ";
        write(STDOUT_FILENO, enterMessage, 50);
    }
}

int main () {

    struct sigaction SIGTSTP_action = {0}, SIGINT_child_action = {0}, ignore_action = {0}, default_action = {0};

    // set up sigaction struct for SIGTSTP
    SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;

    // set up sigaction structs for ingoring and returning to defualt action 
    ignore_action.sa_handler = SIG_IGN;
    default_action.sa_handler = SIG_DFL;

    sigaction(SIGINT, &ignore_action, NULL); // ingore SIGINT

    sigaction(SIGTSTP, &SIGTSTP_action, NULL); // change mode for SIGTSTP

    // global main vars
    int i = 0; // looping vars
    int j = 0;
    int exitStatus = -50; // storing child exit values
    int termSignal = -50; // storing signal child terminated by
    int deathBySignal = -1; // if killed by signal
    pid_t childs[1000000]; // large array for background pids
    int numChildren = 0; // number of background pids
    int childExitMethod = -5; // int for seeing how a child exited
    while (1) { // always run until exit command is recived
        // check all of background children to see if finished running
        pid_t childPID_actual = -5;
        for (i = 0; i < numChildren; i++) { // for every background child
            childPID_actual = waitpid(childs[i], &childExitMethod, WNOHANG); // check if process is done but do not wait for it as it is in background
            if (childPID_actual != 0) { // if exited
                if (WIFEXITED(childExitMethod) != 0) { // if exited normally, save method and status and let user know
                    exitStatus = WEXITSTATUS(childExitMethod);
                    printf("background pid %d is done: exit value %d\n", childPID_actual, exitStatus);
                    fflush(stdout);
                } else if (WIFSIGNALED(childExitMethod) != 0) { // if killed by signal, save method and signal and let user know
                    termSignal = WTERMSIG(childExitMethod);
                    printf("background pid %d is done: terminated by signal %d\n", childPID_actual, termSignal);
                    fflush(stdout);
                }
                childs[i] = childs[numChildren - 1]; // remove child from array
                numChildren--;
            }
        }

        printf(": "); // print prompt for user
        fflush(stdout);
        int numCharsEntered = -1;
        size_t bufferSize = 1;
        char* input = NULL;
        numCharsEntered = getline(&input, &bufferSize, stdin); // get user input
        if (numCharsEntered - 1 > 2048) { // test for max chars
            printf("exceeded maximum of 2048 characters\n");
            fflush(stdout);
            char blank[] = "\n";
            input = blank;
            numCharsEntered = 1;
        }

        char* inputArray[numCharsEntered / 2]; // array of arguments entered for easy execution
        char* token = strtok(input, " "); // token by space as that is what all arguments should be seperated by
        inputArray[0] = token; // get command
        int inputArrayLen = 1;
        while (token[strlen(token) - 1] != '\n') { // until the token has a \n at end
            token = strtok(NULL, " "); // get parameters
            inputArray[inputArrayLen] = token;
            inputArrayLen++;
        }
        if (inputArrayLen > 512) { // test for max arguments
            printf("exceeded maximum of 512 arguments\n");
            fflush(stdout);
            inputArray[0] = ""; // set command blank
        }
        
        // makes last string end with '\0' instead of '\n'
        char lastString[strlen(inputArray[inputArrayLen - 1])];
        strcpy(lastString, inputArray[inputArrayLen - 1]);
        lastString[strlen(lastString) - 1] = '\0';
        inputArray[inputArrayLen - 1] = lastString;

        // create file name strings for input redirection
        char inFile[numCharsEntered];
        memset(inFile, '\0', sizeof(inFile));
        char outFile[numCharsEntered];
        memset(outFile, '\0', sizeof(outFile));

        int bg = 0;
        if (strcmp(inputArray[inputArrayLen - 1], "&") == 0) { // if last argument is "&" put in background and remove from parameters
            inputArrayLen--;
            bg = 1;
        }
        
        // search arguments for special symbols
        for (i = 0; i < inputArrayLen; i++) {
            if (strcmp(inputArray[i], "<") == 0) { // if < exists, process file to read from
                strcpy(inFile, inputArray[i + 1]); // save name
                for (j = i; j < inputArrayLen - 1; j++) { // remove from arguments
                    inputArray[j] = inputArray[j + 1];
                    inputArray[j + 1] = inputArray[j + 2];
                }
                inputArrayLen -= 2;

            } else if (strcmp(inputArray[i], ">") == 0) { // if > exists, process file to write from
                strcpy(outFile, inputArray[i + 1]); // save name
                for (j = i; j < inputArrayLen - 1; j++) { // remove from arguments
                    inputArray[j] = inputArray[j + 1];
                    inputArray[j + 1] = inputArray[j + 2];
                }
                inputArrayLen -= 2;

            } else { // expand $$ to process id
                // get id number to expand to
                pid_t pid = getpid();
                char pidStr[pid];
                memset(pidStr, '\0', sizeof(pidStr));
                sprintf(pidStr, "%d", pid);

                // create replacement argument
                char argument[strlen(inputArray[i]) + strlen(pidStr) + 1000000];
                memset(argument, '\0', sizeof(argument));
                strcpy(argument, inputArray[i]);

                // look for "$$"
                for (j = 0; j < strlen(argument); j++) {
                    if (argument[j] == '$' && argument[j + 1] == '$') {
                        if (strlen(argument) == 2) { // if string only $$
                            inputArray[i] = pidStr;
                        } else {
                            argument[j] = ' '; // chagne $$ into spaces
                            argument[j + 1] = ' ';
                            if (j == 0) { // if $$ at front of string
                                char* last = strtok(argument, "  ");
                                char fullLine[strlen(pidStr) + strlen(last) + 1];
                                memset(fullLine, '\0', sizeof(fullLine));
                                sprintf(fullLine, "%s%s", pidStr, last); // concatenate strings
                                strcpy(argument, fullLine);
                                inputArray[i] = argument; // set new argument
                            } else if (j == strlen(argument) - 2) { // if $$ at end of string
                                char* first = strtok(argument, "  ");
                                char fullLine[strlen(pidStr) + strlen(first) + 1];
                                memset(fullLine, '\0', sizeof(fullLine));
                                sprintf(fullLine, "%s%s", first, pidStr); // concatenate strings
                                strcpy(argument, fullLine);
                                inputArray[i] = argument; // set new argument
                            } else { // if $$ in middle of string
                                char* first = strtok(argument, "  ");
                                char* last = strtok(NULL, "  ");
                                char fullLine[strlen(first) + strlen(pidStr) + strlen(last) + 1];
                                memset(fullLine, '\0', sizeof(fullLine));
                                sprintf(fullLine, "%s%s%s", first, pidStr, last); // concatenate strings
                                printf("%s\n", fullLine);
                                strcpy(argument, fullLine);
                                inputArray[i] = argument; // set new argument
                            }
                        }
                    }
                }
            }
        }
        
        // run command
        char firstWord[strlen(inputArray[0]) + 1];
        strcpy(firstWord, inputArray[0]);
        if (firstWord[0] != '#') { // ensure line is not a comment
            if (strcmp(inputArray[0], "exit") == 0) { // command is exit
                // kill all children and exit
                for (i = 0; i < numChildren; i++) {
                    kill(childs[i], SIGKILL);
                }
                exit(0);
            } else if (strcmp(inputArray[0], "cd") == 0) { // command is cd
                int err = -5;
                if (inputArrayLen == 1) { // if no parameters change to home
                    char* home = getenv("HOME");
                    err = chdir(home);
                } else { // else change to first parameter which should be users inputed path
                    err = chdir(inputArray[1]);
                }
                if (err == -1) { // if error finding directory
                    printf("bash: %s: No such file or directory\n", inputArray[i]);
                    fflush(stdout);
                }
            } else if (strcmp(inputArray[0], "status") == 0) { // command is status
                switch (deathBySignal) {
                    case -1: // var is unchanged
                        printf("no commands have been ran\n");
                        fflush(stdout);
                        break;
                    case 0: // not killed by signal
                        printf("exit value %d\n", exitStatus);
                        fflush(stdout);
                        break;
                    case 1: // killed by signal
                        printf("terminated by signal %d\n", termSignal);
                        fflush(stdout);
                        break;
                }
            } else if (strcmp(inputArray[0], "") != 0) { // if not blank, fork and execute
                // create new argument array with NULL at the end
                char* parameters[inputArrayLen + 1];
                for (i = 0; i < inputArrayLen; i++) {
                    parameters[i] = inputArray[i];
                }
                parameters[inputArrayLen] = NULL;

                // put in background if in foreground-only mode
                if (fgmode) {
                    bg = 0;
                }
                pid_t spawnpid = -5;
                spawnpid = fork();
                switch (spawnpid) {
                    case -1: // fork error
                        perror("Hull Breach!");
                        exit(1);
                        break;
                    case 0: // child process
                        sigaction(SIGTSTP, &ignore_action, NULL); // ignore ctrl-Z
                        // input redirection
                        if (strlen(inFile) != 0) {
                            char* newFile = inFile;
                            int target = open(newFile, O_RDONLY); // get file
                            if (target == -1) { // tell user we can't find their file
                                printf("bash: %s: No such file or directory\n", newFile);
                                fflush(stdout);
                                exit(1);
                            } else {
                                dup2(target, 0); // change stdin to file
                            }
                        // no input redirection and in background
                        } else if (bg) {
                            int target = open("/dev/null", O_RDONLY);
                            if (target == -1) {
                                printf("bash: /dev/null: No such file or directory\n");
                                fflush(stdout);
                                exit(1);
                            } else {
                                dup2(target, 0); // change stdin /dev/null
                            }
                        }
                        // output redirection
                        if (strlen(outFile) != 0) {
                            char* newFile = outFile;
                            int target = open(newFile, O_WRONLY | O_CREAT | O_TRUNC, 0644); // get file
                            if (target == -1) { // tell user we can't find their file
                                printf("bash: %s: No such file or directory\n", newFile);
                                fflush(stdout);
                                exit(1);
                            } else {
                                dup2(target, 1); // change stdout to file
                            }
                        // no output redirection and in background
                        } else if (bg) {
                            int target = open("/dev/null", O_WRONLY);
                            if (target == -1) {
                                printf("bash: /dev/null: No such file or directory\n\n");
                                fflush(stdout);
                                exit(1);
                            } else {
                                dup2(target, 1); // change stdout /dev/null
                            }
                        }
                        if (!bg) { // if in foreground allow ctrl-C
                            sigaction(SIGINT, &default_action, NULL);
                        }
                        execvp(inputArray[0], parameters); // execute command
                        printf("bash: %s: command not found\n", inputArray[0]); // tell user command failed
                        fflush(stdout);
                        exit(1);
                        break;
                    default: // main parent process
                        if (!bg) { // if run in foreground stop shell until finished
                            waitpid(spawnpid, &childExitMethod, 0); // wait for child to finish
                            if (WIFEXITED(childExitMethod) != 0) { // if exited normally, save method and status for status command
                                exitStatus = WEXITSTATUS(childExitMethod);
                                deathBySignal = 0;
                            } else if (WIFSIGNALED(childExitMethod) != 0) { // if killed by signal, save method and signal for status command
                                termSignal = WTERMSIG(childExitMethod);
                                printf("terminated by signal %d\n", termSignal);
                                fflush(stdout);
                                deathBySignal = 1;
                            }
                        } else { // if run in background tell user pid and do not stop program
                            printf("background pid is %d\n", spawnpid);
                            fflush(stdout);
                            childs[numChildren] = spawnpid; // add child pid to array to easily check when ended
                            numChildren++;
                        }
                        break;
                    }
                }
            }
        }
}