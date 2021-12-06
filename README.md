# Unix-Shell
# Smash - Small (Unix-Like) Shell
## Implemented in C++
### - Support most of the modern linux shell commands, e.g. cd, ls, cat, head, pwd, chpromt and more
### - Support Redirctions (> / >>). e.g. "ls -ll > newfile"
### - Support Pipes (| / |&). e.g. "ls -ll | grep newfile"
### - Support Jobs Commands. e.g. jobs, bg, fg and kill
### - Support keyboard interrupts (ctrlZ / ctrlC to stop/kill job running in the foreground)
### - Support Timeout Commands. e.g. "timeout 5 sleep 10"

#### Download:
    git clone https://github.com/Almogbs/Unix-Shell.git
#### Build (using make):
    cd Unix-Shell/src
    make smash
#### Build (using g++):
    cd Unix-Shell/src
    g++ --std=c++11 -Wall Commands.cpp signals.cpp smash.cpp -o ../release/smash
#### Run:
    cd release
    ./smash
    
