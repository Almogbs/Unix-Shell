#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

/***
 *  TODO:
 *      V Support Redirections
 *      V Support Pipes
 *      V Implement the Signal Calls
 *      V Implement Head Command
 *      V Test All expect Timeout
 *      - Implement Timeout Command & SIG_ALRM
 *      - Test Timeout
 ***/

int main(int argc, char* argv[]) {
    if(signal(SIGTSTP , ctrlZHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }
    /*
    if(signal(SIGALRM , alarmHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }*/
    struct sigaction a;
    a.sa_handler = alarmHandler;
    sigemptyset(&a.sa_mask);
    a.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &a, nullptr) == -1) {
        perror("smash error: failed to set alarm handler");
    }
    SmallShell& smash = SmallShell::getInstance();
    while(true) {
        setbuf(stdout, NULL);
        //setbuf(stderr, NULL);
        std::cout << smash.getPrompt() << "> ";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}