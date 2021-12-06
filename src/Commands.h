#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define SHELL_MAX_PROCESSES (100)


class Command {
protected:
  char* cmd_line;
  char* org_cmd_line;
  int argc;
  char** args;
  bool isBgCmd;
public:
  Command(const char* cmd_line);
  Command() {};
  virtual ~Command();
  virtual void execute() = 0;
  //virtual void prepare();
  //virtual void cleanup();
  char* getCmdLine();
  char** getArgs();
  const char* getOldCmdLine();
  bool getIsBgCmd();
};

class BuiltInCommand : public Command {
public:
  BuiltInCommand(const char* cmd_line);
  virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
public:
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() {}
  void execute() override;
};


class TimeoutCommand : public Command {
public:
  TimeoutCommand(const char* cmd_line);
  virtual ~TimeoutCommand() {}
  void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
private:
  char** oldpwd;
  bool* pwd_exist;
public:
  ChangeDirCommand(const char* cmd_line, char** plastPwd, bool* pwd_exist);
  virtual ~ChangeDirCommand() {}
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class ChangePromptCommand : public BuiltInCommand {
private:
  char** prompt_ptr;
public:
  ChangePromptCommand(const char* cmd_line, char** prompt);
  virtual ~ChangePromptCommand() {}
  void execute() override;
};

class JobsList;

class JobsList {  
public:
  // JobEntry Class Declaration
  class JobEntry {
  private:
    int job_id;
    time_t insert_time;
    pid_t pid;
    bool isStopped;
    Command* cmd;
  public:
    JobEntry(Command* cmd, int job_id, bool isStopped, pid_t pid);
    bool operator<(const JobEntry &rhs);
    bool operator>(const JobEntry &rhs);
    bool operator==(const JobEntry &rhs);
    int getJobId() const;
    void setStopped(bool val);
    char* getJobCmd();
    Command* getCmd();
    const char* getOldCmdLine();
    int getJobPid();
    time_t getJobInsertTime();
    bool JobIsStopped();
    ~JobEntry();
  };

private:
  std::vector<JobEntry*> jobs;
public:
  JobsList();
  ~JobsList();
  class JobIsBigger { 
  public:
    bool operator()(const JobEntry *j1, const JobEntry *j2);
  };
  class JobIsTimeout { 
  public:
    bool operator()(JobEntry *j1, JobEntry *j2);
  };

  void addJob(Command* cmd, bool isStopped, pid_t pid);
  int size();
  void printKillJobs();
  void sortTimeout();
  void setAlarm();
  int getHighestJobID();
  void printJobsList();
  void killAllJobs();
  void removeFinishedJobs(bool del = true);
  JobEntry * getJobById(int jobId);
  JobEntry * getFirst();
  void removeJobById(int jobId, bool del = true);
  void removeJobByPid(int jobPid, bool del = true);
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
};



class PipeCommand : public Command {
  bool isStderr;
  std::string cmd1;
  std::string cmd2;
public:
  bool isSon;
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
private:
  JobsList* jl;
  std::string cmd;
  std::string file;
  bool isAppend;
public:
  explicit RedirectionCommand(const char* cmd_line, JobsList* jl);
  virtual ~RedirectionCommand() {}
  void execute() override;
};

class JobsCommand : public BuiltInCommand {
private:
  JobsList* jobs;
public:
  JobsCommand(const char* cmd_line, JobsList* jobs);
  virtual ~JobsCommand() {}
  void execute() override;
};

class QuitCommand : public BuiltInCommand {
private:
  JobsList* jobs;
public:
  QuitCommand(const char* cmd_line, JobsList* jobs);
  virtual ~QuitCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
private:
  JobsList* jobs;
public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
private:
  JobsList* jobs;
public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
private:
  JobsList* jobs;
public:
  BackgroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class HeadCommand : public BuiltInCommand {
public:
  HeadCommand(const char* cmd_line);
  virtual ~HeadCommand() {}
  void execute() override;
};


class SmallShell {
private:
  char* prompt;
  bool oldwd_exist;
  char* oldwd;
  pid_t pid;
  SmallShell();
public:
  JobsList::JobEntry* curr_job;
  JobsList jl;
  JobsList timeoutlist;
  Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance()             // make SmallShell singleton
  {
    static SmallShell instance;                // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  pid_t get_pid();
  const char* getPrompt();
  ~SmallShell();
  void executeCommand(const char* cmd_line);
};

#endif //SMASH_COMMAND_H_
