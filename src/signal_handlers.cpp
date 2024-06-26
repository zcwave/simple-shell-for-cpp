#include <iostream>
#include <sys/wait.h>
#include <cassert>

#include "tsh.h"
#include "jobs_types.h"
#include "Signal.hpp"
#include "Jobs.hpp"

using std::cout, std::endl;

#define BLOCK(set, old_set) sigprocmask(SIG_BLOCK, &(set), &(old_set))
#define BLOCK_NOT_SAVE_OLD_SET(set) sigprocmask(SIG_BLOCK, &(set), NULL)
#define UNBLOCK(old_set) sigprocmask(SIG_SETMASK, &(old_set), NULL)

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) {
    int old_errno = errno;
    pid_t pid;
    int status;
    sigset_t mask_all, prev_all; 
        
    sigfillset(&mask_all);  
 
    while((pid = waitpid(-1, 
                         &status, 
                         WNOHANG | WUNTRACED)) > 0) {
        BLOCK(mask_all, prev_all);
        auto &jobs = Jobs::getInstance();
        auto job = jobs.getJobByPid(pid);

        if (!job.has_value()) {
            if (verbose)
                cout << "hello,world" << endl;
            //! 打印错误日志！！
            return;
        }
 
        auto _job = job.value();

        if (pid == jobs.getFgPid()) {
            FG_PID_GLOBALS = pid;
        }


        if (WIFEXITED(status)) {  
        // Normal: delete job
            jobs.deleteJob(pid); 
        } else if (WIFSIGNALED(status)) { 
        // C^C: print,and delete job
            printf("Job [%d] (%d) terminated by signal %d\n", 
                                                        _job.jid, 
                                                        _job.pid, 
                                                        WTERMSIG(status));
            Jobs::getInstance().deleteJob(pid); 
        } else if (WIFSTOPPED(status)) {
        // C^Z: print,and moditfy job state.
            printf("Job [%d] (%d) stopped by signal %d\n", 
                                                    _job.jid, 
                                                    _job.pid, 
                                                    WSTOPSIG(status));
            job.value().state = ST;
        } else {
        //? happen?
            if (verbose) 
                cout << "BUG!! sigchld_handler" << endl;
        }
 
        UNBLOCK(prev_all);
   }
 
    errno = old_errno;

}


/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) {

    int old_errno = errno;
    auto pid = Jobs::getInstance().getFgPid();

    if (pid.has_value())
        kill(-pid.value(), sig);
    else {
        if (verbose) 
            cout << __func__ << " : " << "Not found fgpid."<< endl;
    }

    errno = old_errno;

}


/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) {

    int old_errno = errno;
    auto pid = Jobs::getInstance().getFgPid();

    if (pid.has_value())
        kill(-pid.value(), sig);
    else {
        if (verbose) 
            cout << endl
                 << __func__ 
                 << " : " 
                 << "Not found fgpid." 
                 << endl;
    }

    errno = old_errno;

}


/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) {

    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);

}
