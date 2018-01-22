#include <sys/types.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/ptrace.h>
#include <sys/reg.h>     /* For constants ORIG_EAX, etc */
#include <string.h>
#include <sys/wait.h>
#include <sys/syscall.h>    /* For SYS_write, etc */

#include <algorithm>
#include <iostream>
#include <tuple>
#include <iostream>
#include <set>
#include <experimental/optional>
#include <memory>
#include <cstddef>

#include "dettraceSystemCall.hpp"
#include "ptracer.hpp"


using namespace std;

ptracer::ptracer(pid_t pid ){
  traceePid = pid;

  int startingStatus;
  if(-1 == waitpid(pid, &startingStatus, 0)){
    throw runtime_error("Unable to start first process: " + string { strerror(errno)});
  }
}

uint64_t ptracer::arg1(){
  return regs.rdi;
}
uint64_t ptracer::arg2(){
  return regs.rsi;
}
uint64_t ptracer::arg3(){
  return regs.rdx;
}
uint64_t ptracer::arg4(){
  return regs.rcx;
}
uint64_t ptracer::arg5(){
  return regs.r8;
}
uint64_t ptracer::arg6(){
  return regs.r9;
}

uint64_t ptracer::getEventMessage(){
  long event;
  doPtrace(PTRACE_GETEVENTMSG, traceePid, nullptr, &event);

  return event;
}

bool ptracer::isPtraceEvent(int status, enum __ptrace_eventcodes event){
  return (status >> 8) == (SIGTRAP | (event << 8));
}

uint64_t ptracer::getReturnValue(){
  return regs.rax;
}

uint64_t ptracer::getSystemCallNumber(){
  return regs.orig_rax;
}


void ptracer::setReturnRegister(uint64_t retVal){
  regs.rax = retVal;
  // Please note how address is passed in data argument here. Which I guess sort of makes
  // sense? We are passing data to it?
  doPtrace(PTRACE_SETREGS, traceePid, nullptr, &regs);
}

void ptracer::updateState(pid_t newPid){
  traceePid = newPid;
  doPtrace(PTRACE_GETREGS, traceePid, NULL, & regs);

  return;
}

pid_t ptracer::getPid(){
  return traceePid;
}

void ptracer::setOptions(pid_t pid){
  doPtrace(PTRACE_SETOPTIONS, pid, NULL, (void*)
	   (PTRACE_O_EXITKILL | // If Tracer exits. Send SIGKIll signal to all tracees.
	    PTRACE_O_TRACECLONE | // Automatically enroll child of tracee.
	    // clone is called
	    PTRACE_O_TRACEEXEC |
	    PTRACE_O_TRACEFORK |
	    PTRACE_O_TRACEVFORK |
	    // PTRACE_O_TRACEEXIT | // Stop tracee when it exits.
	    PTRACE_O_TRACESYSGOOD));
  return;
}

string ptracer::readTraceeCString(char* readAddress, pid_t traceePid){
  string r;
  bool done = false;

  // Read long-sized chuncks of memory at at time.
  while (!done){
    int64_t result = doPtrace(PTRACE_PEEKDATA, traceePid, readAddress, nullptr);
    const char* p = (const char*) &result;
    const size_t bytesRead = strnlen(p, wordSize);
    if (wordSize != bytesRead) {
      done = true;
    }

    for (unsigned i = 0; i < bytesRead; i++) {
      r += p[i];
    }

    // Notice this doesn't change readAddress outside this function -> pass by value.
    readAddress += bytesRead;
  }

  return r;
}


long ptracer::doPtrace(enum __ptrace_request request, pid_t pid, void *addr, void *data){
  long val = ptrace(request, pid, addr, data);

  if(val == -1){
    throw runtime_error("Ptrace failed with error: " + string { strerror(errno) });
  }
  return val;
}


void ptracer::writeArg1(uint64_t val){
  regs.rdi = val;
  doPtrace(PTRACE_SETREGS, traceePid, nullptr, &regs);
}

void ptracer::writeArg3(uint64_t val){
  regs.rdx = val;
  doPtrace(PTRACE_SETREGS, traceePid, nullptr, &regs);
}