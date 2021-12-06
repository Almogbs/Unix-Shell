#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <limits.h>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <time.h>
#include <algorithm>  
#include <signal.h>
#include <fcntl.h>



using namespace std;

// Error messages
#define _PRINT_ERROR(ERR, CMD) std::cerr << ERR(CMD) << std::endl;
#define _PRINT_PERROR(ERR, CMD) perror(ERR(CMD));
#define _PRINT_ERROR_JOB_ID(ERR_S, ERR_E, CMD, ID) std::cerr <<  ERR_S(CMD) << ID << ERR_E << std::endl;
#define SYSCALL_ERROR(CMD)                  "smash error: " CMD " failed"
#define TOO_MANY_ARGS_ERROR(CMD)            "smash error: " CMD ": too many arguments"
#define NOT_ENOUGH_ARGS_ERROR(CMD)          "smash error: " CMD ": not enough arguments"
#define NO_OLDPWD_ERROR(CMD)                "smash error: " CMD ": OLDPWD not set"
#define SYSCALL_ERROR(CMD)                  "smash error: " CMD " failed"
#define INVALID_ARGS_ERROR(CMD)             "smash error: " CMD ": invalid arguments"
#define JOB_NOT_EXIST_ERROR_START(CMD)      "smash error: " CMD ": job-id "
#define JOB_NOT_EXIST_ERROR_END             " does not exist"
#define JOB_ALREADY_IN_BG_ERROR_START(CMD)  "smash error: " CMD ": job-id "
#define JOB_ALREADY_IN_BG_ERROR__END        " is already running in the background"
#define JOB_LIST_EMPTY_ERROR(CMD)           "smash error: " CMD ": jobs list is empty"
#define JOBS_NO_STOPPED_ERROR(CMD)          "smash error: " CMD ": there is no stopped jobs to resume"
#define EXECV    "execv"
#define TIMEOUT    "timeout"
#define READ    "read"
#define HEAD     "head"
#define PIPE      "pipe"
#define DUP       "dup"
#define CLOSE     "close"
#define CD        "cd"
#define OPEN      "open"
#define FG        "fg"
#define BG        "bg"
#define FORK      "fork"
#define KILL      "kill"
#define CHDIR     "chdir"
#define BASH_PATH "/bin/bash"

// Constants
const char* WHITESPACE =     " \n\r\t\f\v";
const char* DEFAULT_PROMPT = "smash";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif



string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for (std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}


int argToInt(char* arg) {
  int ret = -1;
  try {
    ret = std::atoi(arg);
  }
  catch (std::invalid_argument&) {
    return -1;
  }
  return ret;
}

/*** SmallShell class ***/
SmallShell::SmallShell() : oldwd_exist(false), curr_job(nullptr) {
  this->oldwd = (char*)malloc(PATH_MAX);
  this->prompt = (char*)malloc(COMMAND_ARGS_MAX_LENGTH);
  strcpy(this->prompt, DEFAULT_PROMPT);
  this->pid = getpid();
}

SmallShell::~SmallShell() {
  free(oldwd);
  free(prompt);
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord[firstWord.size() - 1] == '&') {
    firstWord = firstWord.substr(0, firstWord.size() - 1);
  }
  if (cmd_s.find('>') != std::string::npos) {
    return new RedirectionCommand(cmd_line, &jl);
  }
  else if (cmd_s.find('|') != std::string::npos) {
    return new PipeCommand(cmd_line);
  }
  else if (firstWord.compare("timeout") == 0) {
    return new TimeoutCommand(cmd_line);
  }
  else if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("chprompt") == 0) {
    return new ChangePromptCommand(cmd_line, &this->prompt);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line, &this->oldwd, &this->oldwd_exist);
  }
  else if (firstWord.compare("jobs") == 0) {
    return new JobsCommand(cmd_line, &this->jl);
  }
  else if (firstWord.compare("kill") == 0) {
    return new KillCommand(cmd_line, &this->jl);
  }
  else if (firstWord.compare("fg") == 0) {
    return new ForegroundCommand(cmd_line, &this->jl);
  }
  else if (firstWord.compare("bg") == 0) {
    return new BackgroundCommand(cmd_line, &this->jl);
  }
  else if (firstWord.compare("quit") == 0) {
    return new QuitCommand(cmd_line, &this->jl);
  }
  else if (firstWord.compare("head") == 0) {
    return new HeadCommand(cmd_line);
  }
  else {
    return new ExternalCommand(cmd_line);
  }

  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // clear finished jobs before executing a new one
  //setbuf(stdout, NULL);
  timeoutlist.removeFinishedJobs(false);
  jl.removeFinishedJobs();

  if (string(cmd_line).empty()) {
    return;
  }
  Command* cmd;
  try {
    cmd = CreateCommand(cmd_line);
  } catch (std::bad_alloc&) {
    return;
  }

  if(cmd == nullptr) return;

  // check if Built-In Command
  if (dynamic_cast<BuiltInCommand*>(cmd) != nullptr) {
    bool isQuitCmd = (dynamic_cast<QuitCommand*>(cmd) != nullptr);
    cmd->execute();
    delete cmd;
    if (isQuitCmd) {
      exit(0);
    }
  }
  else if (dynamic_cast<ExternalCommand*>(cmd) != nullptr) {
    cmd->execute();
  }
  else if (dynamic_cast<TimeoutCommand*>(cmd) != nullptr) {
    cmd->execute();
  }
  else if (dynamic_cast<PipeCommand*>(cmd) != nullptr) {
    cmd->execute();
    if(dynamic_cast<PipeCommand*>(cmd)->isSon) {
      delete cmd;
      exit(0);
    }
    delete cmd;
  }
  else if (dynamic_cast<RedirectionCommand*>(cmd) != nullptr) {
    cmd->execute();
    bool isQuitCmd = (string(cmd->getArgs()[0]).compare("quit") == 0);
    delete cmd;
    if (isQuitCmd) {
      exit(0);
    }
  }
}

const char* SmallShell::getPrompt() {
  return this->prompt;
}

pid_t SmallShell::get_pid() {
  return this->pid;
}
/*** SmallShell class END ***/




/*** JobList class ***/
JobsList::JobsList() {
  
}

JobsList::~JobsList() {

}

void  JobsList::addJob(Command* cmd, bool isStopped, pid_t pid) {
  this->removeFinishedJobs();
  int job_id = getHighestJobID() + 1;
  JobEntry* je;
  try {
    je = new JobEntry(cmd, job_id, isStopped, pid);
  }
  catch (std::bad_alloc&) {
    return;
  }
  jobs.push_back(je);
}

int JobsList::getHighestJobID() {
  int max_job_id = 0;
  for (auto x: jobs) {
    max_job_id = max(max_job_id, x->getJobId());
  }
  return max_job_id;
}

int JobsList::size() {
  return jobs.size();
}

void JobsList::printKillJobs() {
  for (auto x: jobs) {
    std::cout << x->getJobPid() << ": " << x->getOldCmdLine() << std::endl;
  }
}

void  JobsList::printJobsList() {
  sort(jobs.begin(), jobs.end(), JobsList::JobIsBigger());
  time_t curr_time;
  time(&curr_time);
  for (size_t i = 0; i < jobs.size(); i++) {
    fprintf(stdout, "[%d] %s : %d %d secs", jobs[i]->getJobId(), jobs[i]->getOldCmdLine(), jobs[i]->getJobPid(), (int)difftime(curr_time, jobs[i]->getJobInsertTime()));
    if (jobs[i]->JobIsStopped()) fprintf(stdout, " (stopped)\n");
    else fprintf(stdout, "\n");
  }
}

void  JobsList::killAllJobs() {
  while (jobs.size() != 0) {
    kill(jobs.back()->getJobPid(), SIGKILL);
    delete jobs.back();
    jobs.pop_back();
  }
}

void  JobsList::removeFinishedJobs(bool del) {
  auto iter = jobs.begin();
  while (iter != jobs.end())
  {
    int status;       
    if (waitpid((*iter)->getJobPid(), &status, WNOHANG) > 0 || kill((*iter)->getJobPid(), 0) == -1) {
      if (del) {
        delete *iter;
      }
      iter = jobs.erase(iter);
    }
    else {
      ++iter;
    }
  }
}

JobsList::JobEntry*  JobsList::getJobById(int jobId) {
  for (size_t i = 0; i < jobs.size(); i++) {
    if (jobs[i]->getJobId() == jobId) {
      return jobs[i];
    }
  }
  return nullptr;
}

void  JobsList::removeJobById(int jobId, bool del) {
  for (size_t i = 0; i < jobs.size(); i++) {
    if (jobs[i]->getJobId() == jobId) {
      if(del) delete *(jobs.begin() + i);
      jobs.erase(jobs.begin() + i);
      return;
    }
  }
}

void JobsList::removeJobByPid(int jobPid, bool del) {
  for (size_t i = 0; i < jobs.size(); i++) {
    if (jobs[i]->getJobPid() == jobPid) {
      if(del) delete *(jobs.begin() + i);
      jobs.erase(jobs.begin() + i);
      return;
    }
  }
}

Command* JobsList::JobEntry::getCmd() {
  return this->cmd;
}

JobsList::JobEntry*  JobsList::getLastJob(int* lastJobId) {
  time_t lj_time = LONG_MAX;
  int idx = -1;
  for (size_t i = 0; i < jobs.size(); i++) {
    time_t t = LONG_MAX;
    time(&t);
    time_t k = abs(t - jobs[i]->getJobInsertTime() - argToInt(jobs[i]->getCmd()->getArgs()[1])) ;
    if ( k < lj_time) {
      idx = i;
      lj_time = k;
    }
  }

  if (idx != -1) {
    *lastJobId = jobs[idx]->getJobId();
    return jobs[idx];
  } 

  return nullptr;
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int *jobId) {
  int max_job_id = 0;
  JobsList::JobEntry* max_job = nullptr;
  for (auto x: jobs) {
    if (x->getJobId() > max_job_id && x->JobIsStopped()) {
      max_job_id = x->getJobId();
      max_job = x;
    }
  }
  *jobId = max_job_id;
  return max_job;
}

  
  
bool JobsList::JobIsBigger::operator()(const JobsList::JobEntry* j1, const JobsList::JobEntry* j2){
  return j1->getJobId() < j2->getJobId();
}

bool JobsList::JobIsTimeout::operator()(JobsList::JobEntry* j1, JobsList::JobEntry* j2){
  return j1->getJobInsertTime() + argToInt(j1->getCmd()->getArgs()[1]) < j2->getJobInsertTime() + argToInt(j2->getCmd()->getArgs()[1]);
}

void JobsList::sortTimeout() {
  sort(jobs.begin(), jobs.end(), JobsList::JobIsTimeout());
}

void JobsList::setAlarm() {
  time_t t;
  time(&t);
  if(this->size() != 0) alarm(this->jobs[0]->getJobInsertTime() + argToInt(this->jobs[0]->getCmd()->getArgs()[1]) - t);
}

JobsList::JobEntry* JobsList::getFirst() {
  if(this->size() == 0) return nullptr;
  return this->jobs[0];
}
/*** JobList class END ***/




/*** JobEntry class ***/
JobsList::JobEntry::JobEntry(Command* cmd, int job_id, bool isStopped, pid_t pid) : job_id(job_id), pid(pid),  isStopped(isStopped), cmd(cmd) {
  time(&this->insert_time);
}

bool JobsList::JobEntry::operator<(const JobEntry &rhs) {
  return this->job_id < rhs.job_id;
}

bool JobsList::JobEntry::operator>(const JobEntry &rhs) {
  return this->job_id > rhs.job_id;
}

bool JobsList::JobEntry::operator==(const JobEntry &rhs) {
  return this->job_id == rhs.job_id;
}

int JobsList::JobEntry::getJobId() const{
  return this->job_id;
}

void JobsList::JobEntry::setStopped(bool val) {
  this->isStopped = val;
}

char* JobsList::JobEntry::getJobCmd() {
  return this->cmd->getCmdLine();
}

const char* JobsList::JobEntry::getOldCmdLine() {
  return this->cmd->getOldCmdLine();
}

time_t JobsList::JobEntry::getJobInsertTime() {
  return this->insert_time;
}

bool JobsList::JobEntry::JobIsStopped() {
  return this->isStopped;
}

int JobsList::JobEntry::getJobPid() {
  return this->pid;
}

JobsList::JobEntry::~JobEntry() {
  if(cmd) {
    delete cmd;
  }
}
/*** JobEntry class END ***/





/*** Command class ***/
Command::Command(const char* cmd_line_t) {
  cmd_line = (char*)malloc(COMMAND_ARGS_MAX_LENGTH);
  org_cmd_line = (char*)malloc(COMMAND_ARGS_MAX_LENGTH);
  strcpy(cmd_line, cmd_line_t);
  strcpy(org_cmd_line, cmd_line_t);
  _removeBackgroundSign(cmd_line);
  args = new char*[COMMAND_MAX_ARGS + 1];
  argc = _parseCommandLine(cmd_line, args);
  isBgCmd = _isBackgroundComamnd(cmd_line_t);
};

bool Command::getIsBgCmd() {
  return isBgCmd;
};

char** Command::getArgs() {
  return args;
};

Command::~Command() {
  if(args) {
    for(int i = 0; i < argc; i++) {
      free(args[i]);
    }
    delete[] args;
  }
  free(org_cmd_line);
  free(cmd_line);
};

char* Command::getCmdLine() {
  return this->cmd_line;
};

const char* Command::getOldCmdLine() {
  return this->org_cmd_line;
};
/*** Command class END ***/




/*** BuiltInCommand class ***/
BuiltInCommand::BuiltInCommand(const char* cmd_line_t) : Command(cmd_line_t) {
  isBgCmd = false;
};
/*** BuiltInCommand class END ***/




/*** ExternalCommand class ***/
ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {};

void ExternalCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    pid_t pid = fork();
    if (pid == 0) {
      setpgrp();

      char cmd_line_t[PATH_MAX];
      strcpy(cmd_line_t, getCmdLine());
      char bash_path[PATH_MAX];
      strcpy(bash_path, BASH_PATH);
      char bash_arg[10];
      strcpy(bash_arg, "-c");
      char* args[] = {bash_path, bash_arg, cmd_line_t, NULL};
      if (execv(BASH_PATH, args) == -1) {
        _PRINT_PERROR(SYSCALL_ERROR, EXECV)
      }
      exit(0);
    } 
    else {
      smash.jl.addJob(this, false, pid);
      int job_id = smash.jl.getHighestJobID();
      if (!getIsBgCmd()) {
        smash.curr_job = smash.jl.getJobById(job_id);
        int status;
        waitpid(pid, &status, WUNTRACED);
        if (!WIFSTOPPED(status)) {
          smash.jl.removeJobById(job_id);
          smash.curr_job = nullptr;
        }
      }
    }

};
/*** ExternalCommand class END ***/




/*** RedirectionCommand class ***/
RedirectionCommand::RedirectionCommand(const char* cmd_line, JobsList* jl) : Command(cmd_line), jl(jl) {
  this->isAppend = (string(cmd_line).find(">>") != std::string::npos);
  this->cmd = string(cmd_line).substr(0, string(cmd_line).find(">"));
  this->file = _trim(string(cmd_line).substr(string(cmd_line).find_last_of(">") + 1));
};

void RedirectionCommand::execute() {
  const char* c_cmd = cmd.c_str();
  const char* c_file = file.c_str();
  int old_out = dup(1);
  if(old_out == -1) {
    _PRINT_PERROR(SYSCALL_ERROR, DUP)
    return;
  }

  int new_out;
  if (isAppend) {
    new_out = open(c_file, O_CREAT | O_APPEND | O_WRONLY, 0666);
  }
  else {
    new_out = open(c_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
  }

  if (new_out == -1) {
    _PRINT_PERROR(SYSCALL_ERROR, OPEN)
    close(old_out);
    return;
  }
  if (dup2(new_out,1) == -1) {
    _PRINT_PERROR(SYSCALL_ERROR, DUP)
    return;
  }
  if (close(new_out) == -1) {
    _PRINT_PERROR(SYSCALL_ERROR, CLOSE)
    return;
  } 
  SmallShell& smash = SmallShell::getInstance();
  smash.executeCommand(c_cmd);

  if (dup2(old_out, 1) == -1) {
    _PRINT_PERROR(SYSCALL_ERROR, DUP)
    return;
  }
  if (close(old_out) == -1) {
    _PRINT_PERROR(SYSCALL_ERROR, CLOSE)
    return;
  }    
};
/*** RedirectionCommand class END ***/



/*** PipeCommand class ***/
PipeCommand::PipeCommand(const char* cmd_line) : Command(cmd_line) {
  this->isStderr = (string(cmd_line).find("|&") != std::string::npos);
  this->cmd1 = string(cmd_line).substr(0, string(cmd_line).find("|"));
  this->cmd2 = string(cmd_line).substr(string(cmd_line).find("|") + (!this->isStderr ? 1 : 2));
};

void PipeCommand::execute() {
  int pipe_args[2];
  if(pipe(pipe_args) != 0 ) {
    _PRINT_PERROR(SYSCALL_ERROR, PIPE)
    return;
  }
  int pid = fork();
  if (pid == 0) {
    (this->isSon) = true;
    setpgrp();
    close(pipe_args[0]);
    if (dup2(pipe_args[1], (this->isStderr ? 2 : 1)) == -1) {
      _PRINT_PERROR(SYSCALL_ERROR, DUP)
      exit(0);
    }
    SmallShell& smash = SmallShell::getInstance();
    smash.executeCommand(cmd1.c_str());
    if (close(pipe_args[1]) == -1) {
      _PRINT_PERROR(SYSCALL_ERROR, CLOSE)
      exit(0);
    }
    exit(0);
  }
  else {
    (this->isSon) = false;
    close(pipe_args[1]);
    int old_out = dup(0);
    if (old_out == -1) {
      _PRINT_PERROR(SYSCALL_ERROR, DUP)
      return;
    }
    if (dup2(pipe_args[0], 0) == -1) {
      _PRINT_PERROR(SYSCALL_ERROR, DUP)
      return;
    }
    SmallShell& smash = SmallShell::getInstance();
    smash.executeCommand(cmd2.c_str());
    if (close(pipe_args[0]) == -1) {
      _PRINT_PERROR(SYSCALL_ERROR, CLOSE)
    }
    if (dup2(old_out, 0) == -1) {
      _PRINT_PERROR(SYSCALL_ERROR, DUP)
      return;
    }
    close(old_out);
  }
};
/*** PipeCommand class END ***/





/*** ChangeDirCommand class ***/
ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd, bool* pwd_exist) :
   BuiltInCommand(cmd_line), oldpwd(plastPwd), pwd_exist(pwd_exist) {};

void ChangeDirCommand::execute() {
  if (argc == 1) {
    return;
  }
  else if (argc > 2) {
    _PRINT_ERROR(TOO_MANY_ARGS_ERROR, CD)
  }
  else if (std::string(args[1]).compare("-") == 0) {
      char currwd[PATH_MAX];
      getcwd(currwd, PATH_MAX);
      if (!(*pwd_exist)){
        _PRINT_ERROR(NO_OLDPWD_ERROR, CD)
        return;
      }

      if (chdir(*oldpwd) != 0) {
        _PRINT_PERROR(SYSCALL_ERROR, CHDIR)
        return;
      }  
      strcpy(*oldpwd, currwd);
  }
  else {
    char currwd[PATH_MAX];
    getcwd(currwd, PATH_MAX);
    if (chdir(args[1]) != 0) {
      _PRINT_PERROR(SYSCALL_ERROR, CHDIR)
      return;
    }
    *pwd_exist = true;
    strcpy(*oldpwd, currwd);
  }
};
/*** ChangeDirCommand class END ***/




/*** GetCurrDirCommand class ***/
GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};

void GetCurrDirCommand::execute() {
  char cwd[PATH_MAX];
  std::cout << std::string(getcwd(cwd, sizeof(cwd))) << std::endl; 
};
/*** GetCurrDirCommand class END ***/




/*** ShowPidCommand class ***/
ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};

void ShowPidCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  std::cout << "smash pid is " << smash.get_pid() << std::endl; 
};
/*** ShowPidCommand class END ***/




/*** ChangePromptCommand class ***/
ChangePromptCommand::ChangePromptCommand(const char* cmd_line, char** curr_prompt) : BuiltInCommand(cmd_line), prompt_ptr(curr_prompt) {};

void ChangePromptCommand::execute() {
  if (argc > 1) {
    strcpy(*prompt_ptr, string(args[1]).c_str());
  }
  else {
    strcpy(*prompt_ptr, string(DEFAULT_PROMPT).c_str());
  }
};
/*** ChangePromptCommand class END ***/




/*** JobsCommand class ***/
JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void JobsCommand::execute() {
  jobs->printJobsList();
};
/*** JobsCommand class END ***/




/*** KillCommand class ***/
KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void KillCommand::execute() {
  if (argc != 3) {
    _PRINT_ERROR(INVALID_ARGS_ERROR, KILL)
    return;
  }
  int job_id = argToInt(args[2]);
  int sig_num = argToInt(args[1] + 1);
  if(args[1][0] != '-') {
    _PRINT_ERROR(INVALID_ARGS_ERROR, KILL)
    return;
  }

  if (sig_num >= 0 && job_id != 0) {
    if(strlen(args[1] + 1) == 0) {
      _PRINT_ERROR(INVALID_ARGS_ERROR, KILL)
      return;
    }
    if(sig_num == 0 && *(args[1] + 1) != '0') {
      _PRINT_ERROR(INVALID_ARGS_ERROR, KILL)
      return;
    }
    JobsList::JobEntry* je = jobs->getJobById(job_id);
    if (je == nullptr) {
      _PRINT_ERROR_JOB_ID(JOB_NOT_EXIST_ERROR_START, JOB_NOT_EXIST_ERROR_END, KILL, job_id)
      return;
    }

    pid_t job_pid = je->getJobPid();
    if (kill(job_pid, sig_num) != 0){
      _PRINT_PERROR(SYSCALL_ERROR, KILL)
      return;
    }
    fprintf(stdout, "signal number %d was sent to pid %d\n", sig_num, job_pid);
  }
  else {
      _PRINT_ERROR(INVALID_ARGS_ERROR, KILL)
  }
};
/*** KillCommand class END ***/




/*** ForegroundCommand class ***/
ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void ForegroundCommand::execute() {
  if (argc > 2) {
    _PRINT_ERROR(INVALID_ARGS_ERROR, FG)
    return;
  }
  int job_id = (argc == 2) ? argToInt(args[1]) : jobs->getHighestJobID();
  if (job_id == -1) {
    _PRINT_ERROR(INVALID_ARGS_ERROR, FG)
    return;
  }
  if (argc == 1 && jobs->getHighestJobID() == 0) {
    _PRINT_ERROR(JOB_LIST_EMPTY_ERROR, FG)
    return;
  }
  JobsList::JobEntry* je = jobs->getJobById(job_id);
  if (je == nullptr) {
    _PRINT_ERROR_JOB_ID(JOB_NOT_EXIST_ERROR_START, JOB_NOT_EXIST_ERROR_END, FG, job_id)
    return;
  }


  pid_t pid = je->getJobPid();
  std::cout << je->getOldCmdLine() << " : " << je->getJobPid() << std::endl;
  if (kill(je->getJobPid(), SIGCONT) != 0) {
    _PRINT_PERROR(SYSCALL_ERROR, FG)
    return;
  }
  int status;
  SmallShell& smash = SmallShell::getInstance();
  smash.curr_job = je;
  waitpid(pid, &status, WUNTRACED);
  if (!WIFSTOPPED(status)) {
    jobs->removeJobById(job_id, false);
    smash.curr_job = nullptr;
    return;
  }
  je->setStopped(true);
};
/*** ForegroundCommand class END ***/




/*** BackgroundCommand class ***/
BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void BackgroundCommand::execute() {
  if (argc > 2) {
    _PRINT_ERROR(INVALID_ARGS_ERROR, BG)
    return;
  }
  int tmp_id;
  jobs->getLastStoppedJob(&tmp_id);
  int job_id = (argc == 2) ? argToInt(args[1]) : tmp_id;
  if(argc == 2 && args[1][0] != '0' && job_id == 0) {
      _PRINT_ERROR(INVALID_ARGS_ERROR, BG)
      return;
    }

  if (argc == 1 && tmp_id == 0) {
    _PRINT_ERROR(JOBS_NO_STOPPED_ERROR, BG)
    return;
  }
  JobsList::JobEntry* je = jobs->getJobById(job_id);
  if (je == nullptr) {
    _PRINT_ERROR_JOB_ID(JOB_NOT_EXIST_ERROR_START, JOB_NOT_EXIST_ERROR_END, BG, job_id)
    return;
  }
  if (!je->JobIsStopped()) {
    _PRINT_ERROR_JOB_ID(JOB_ALREADY_IN_BG_ERROR_START, JOB_ALREADY_IN_BG_ERROR__END, BG, job_id)
    return;

  }
  if (kill(je->getJobPid(), SIGCONT) != 0) {
    _PRINT_PERROR(SYSCALL_ERROR, BG)
    return;
  }
  je->setStopped(false);
  std::cout << je->getOldCmdLine() << " : " << je->getJobPid() << std::endl;
};
/*** BackgroundCommand class END ***/




/*** QuitCommand class ***/
QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void QuitCommand::execute() {
  if ((argc > 1 && strcmp(args[1], "kill") == 0)) {  
    std::cout << "smash: sending SIGKILL signal to " << jobs->size() << " jobs:" << std::endl;
    jobs->printKillJobs();
    jobs->killAllJobs();
  }
};
/*** QuitCommand class END ***/



/*** TimeoutCommand class ***/
TimeoutCommand::TimeoutCommand(const char* cmd_line) : Command(cmd_line){}

void TimeoutCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  if(argc <= 2) {
    _PRINT_ERROR(INVALID_ARGS_ERROR, TIMEOUT)
    return;
  }
  int secs = argToInt(args[1]);
  string str_cmd = string(cmd_line);
  string str_cmd_temp = str_cmd.substr(str_cmd.find_first_of(" ") + 1);
  string new_cmd = str_cmd_temp.substr(str_cmd_temp.find_first_of(" ") + 1);
  
  if(secs < 0) {
    _PRINT_ERROR(INVALID_ARGS_ERROR, TIMEOUT)
    return;
  }
  pid_t pid = fork();
  if (pid == 0) {
    setpgrp();
    char cmd_line_t[PATH_MAX];
    strcpy(cmd_line_t, new_cmd.c_str());
    char bash_path[PATH_MAX];
    strcpy(bash_path, BASH_PATH);
    char bash_arg[10];
    strcpy(bash_arg, "-c");
    char* args[] = {bash_path, bash_arg, cmd_line_t, NULL};
    if (execv(BASH_PATH, args) == -1) {
      _PRINT_PERROR(SYSCALL_ERROR, EXECV)
    }
    exit(0);
  } 
  else {
    smash.timeoutlist.addJob(this, false, pid);
    smash.jl.addJob(this, false, pid);
    smash.timeoutlist.sortTimeout();
    smash.timeoutlist.setAlarm();
    int job_id = smash.jl.getHighestJobID();
    if (!getIsBgCmd()) {
      smash.curr_job = smash.jl.getJobById(job_id);
      int status;
      waitpid(pid, &status, WUNTRACED);
      if (!WIFSTOPPED(status)) {
        smash.jl.removeJobById(job_id, false);
        smash.timeoutlist.removeJobById(job_id);
        smash.curr_job = nullptr;
      }
    }
  }
}
/*** TimeoutCommand class END ***/


/*** HeadCommand class ***/
HeadCommand::HeadCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void HeadCommand::execute() {
  if (argc != 2 && argc != 3)  {
    _PRINT_ERROR(NOT_ENOUGH_ARGS_ERROR, HEAD)
    return;
  }
  int lines = (argc == 3) ? argToInt(args[1] + 1) : 10;
  char* file = (argc == 3) ? args[2] : args[1];
  int fd = open(file, O_RDONLY);
  if (fd == -1) {
    _PRINT_PERROR(SYSCALL_ERROR, OPEN)
    return;
  }
  
  char curr_char[1];
  int status;
  while((status = read(fd, curr_char, 1)) > 0 && lines > 0) {
    std::cout << curr_char;
    if(strcmp(curr_char, "\n") == 0) {
        lines--;
    }
  } 

  if(status == -1) {
    _PRINT_PERROR(SYSCALL_ERROR, READ)
  }
};
/*** HeadCommand class END ***/
