#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <unistd.h>



using namespace std;


#define _PRINT_GOT_CTRLC      std::cout << "smash: got ctrl-C" << std::endl;
#define _PRINT_GOT_CTRLZ      std::cout << "smash: got ctrl-Z" << std::endl;
#define _PRINT_GOT_ALRM       std::cout << "smash: got an alarm" << std::endl;
#define _PRINT_CTRLC_EXEC(ID) std::cout << "smash: process " << ID << " was killed" << std::endl;
#define _PRINT_CTRLZ_EXEC(ID) std::cout << "smash: process " << ID << " was stopped" << std::endl;
#define _PRINT_ALRM_EXEC(CMD) std::cout << "smash: " << CMD << " timed out!" << std::endl;



void ctrlZHandler(int sig_num) {
  _PRINT_GOT_CTRLZ
  SmallShell& smash = SmallShell::getInstance();
  JobsList::JobEntry* je = smash.curr_job;
  if (je == nullptr) {
    return;
  }
  int job_pid = je->getJobPid();
  je->setStopped(true);
  kill(job_pid, SIGSTOP);
  smash.curr_job = nullptr;
  _PRINT_CTRLZ_EXEC(job_pid)

}

void ctrlCHandler(int sig_num) {
  _PRINT_GOT_CTRLC
  SmallShell& smash = SmallShell::getInstance();
  JobsList::JobEntry* je = smash.curr_job;
  if (je == nullptr) {
    return;
  }
  int job_id = je->getJobId();
  int job_pid = je->getJobPid();
  smash.jl.removeJobById(job_id, false);
  smash.timeoutlist.removeJobById(job_id, true);
  kill(job_pid, SIGKILL);
  smash.curr_job = nullptr;
  _PRINT_CTRLC_EXEC(job_pid)
}

void alarmHandler(int sig_num) {
  _PRINT_GOT_ALRM
  SmallShell& smash = SmallShell::getInstance();
  smash.timeoutlist.removeFinishedJobs(false);
  smash.jl.removeFinishedJobs(true);
  JobsList::JobEntry* timeout_job = smash.timeoutlist.getFirst();
  if (timeout_job == nullptr) {
    return;
  }
  int job_id = timeout_job->getJobId();
  if (smash.curr_job != nullptr && smash.curr_job->getJobId() == job_id) {
    smash.curr_job = nullptr;
  }
  if(timeout_job->getJobPid() != smash.get_pid()) {
    kill(timeout_job->getJobPid(), SIGKILL);
  }
  _PRINT_ALRM_EXEC(timeout_job->getOldCmdLine());
  smash.timeoutlist.removeJobById(job_id, false);
  smash.jl.removeJobById(job_id);
  smash.timeoutlist.sortTimeout();
  smash.timeoutlist.setAlarm();
}


