# CS344-Small-Shell
Uploaded small shell portfolio project from my Spring 2023 operating systems class
**Features**
* Command prompt and execution
* Lines that start with "#" are comments
* Variable expansion of "$$" into shell process ID
* Built in comments:
-   "exit": terminates shell
-   "cd": changes directory
-   "status": prints the exit status or the terminating signal of the foreground process last ran (ingores built in command and prints 0 if none ran)
* Redirects input from standard input to file by including " < [file_name] " in line
* Redirects output from standard output to file by including " > [file_name] " in line
* Can run command in the background allowing user to run commands again while background command is running by including "&" as the last argument
* Sending the SIGTSTP signal enters and exits from foreground only mode where all commands will be ran in the foreground
* Sending the SIGINT signal kills only a foreground process
**Running the shell**
Compile the smallsh.c file with the line "gcc -o smallsh.c smallsh" then run the executable smallsh
