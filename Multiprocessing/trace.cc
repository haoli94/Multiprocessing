/**
 * File: trace.cc
 * ----------------
 * Presents the implementation of the trace program, which traces the execution of another
 * program and prints out information about ever single system call it makes.  For each system call,
 * trace prints:
 *
 *    + the name of the system call,
 *    + the values of all of its arguments, and
 *    + the system calls return value
 */
#include <string>
#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <unistd.h> // for fork, execvp
#include <string.h> // for memchr, strerror
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include "trace-options.h"
#include "trace-error-constants.h"
#include "trace-system-calls.h"
#include "trace-exception.h"
#include <errno.h>
#include <limits.h>


using namespace std;

long registerVal(pid_t child, int num) {
    long val = ptrace(PTRACE_PEEKUSER, child, num * sizeof(long));
    assert(errno == 0);
    return val;
}

long get_syscall_arg(pid_t child, int which) {
    switch (which) {

	case 0: return registerVal(child, RDI);
	case 1: return registerVal(child, RSI);
	case 2: return registerVal(child, RDX);
	case 3: return registerVal(child, R10);
	case 4: return registerVal(child, R8);
	case 5: return registerVal(child, R9);
	default: return -1L;
    }
}
string readString(pid_t pid, long addr) {
    string str; 
    size_t read = 0;
    while (true) {
	bool end = false;
	long tmp = ptrace(PTRACE_PEEKDATA, pid, (char*) addr + read);
	char* word = reinterpret_cast<char *>(&tmp);
	for (size_t i = 0; i < sizeof(long); i++){
	    char ch = *(word+i);
	    if(ch == '\0') {
		end = true;
		break;
	    }
	    str += ch;
	}
	read += sizeof(long);
	if (end == true){
	    break;
	}

    }
    return str;
}





int main(int argc, char *argv[]) {
    bool simple = false, rebuild = false;
    int numFlags = processCommandLineFlags(simple, rebuild, argv);
    if (argc - numFlags == 1) {
	cout << "Nothing to trace... exiting." << endl;
	return 0;
    }
    if (simple){
	pid_t pid = fork();
	if (pid == 0) {
	    ptrace(PTRACE_TRACEME);
	    //kill(getpid(), SIGSTOP); Kill can cause itself and others to stop
	    raise(SIGSTOP);
	    execvp(argv[numFlags + 1], argv + numFlags + 1);
	    return 0;
	}

	int status;
	waitpid(pid, &status, 0);
	assert(WIFSTOPPED(status));
	ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);

	while (true) {
	    bool Exited = false;
	    while (true){
		ptrace(PTRACE_SYSCALL, pid, 0, 0);
		waitpid(pid, &status, 0);
		if (WIFSTOPPED(status) && WSTOPSIG(status) & 0x80){ // If the child is stopped, we can continue
		    break;
		}
		if (WIFEXITED(status)){ //If the child has finished we turn the flag to be true and break the outside loop
		    Exited = true;
		    break;
		}
	    }
	    if (Exited) break;
	    int syscall = ptrace(PTRACE_PEEKUSER, pid, ORIG_RAX * sizeof(long));
	    cout << "syscall(" << syscall << ") = " << flush;
	    //First get parameters of the System call
	    while (true){
		ptrace(PTRACE_SYSCALL, pid, 0, 0);
		waitpid(pid, &status, 0);
		if (WIFSTOPPED(status) && WSTOPSIG(status) & 0x80){ // If the child is stopped, we can continue
		    break;
		}
		if (WIFEXITED(status)){
		    Exited = true;
		    break;
		}
	    }
	    if (Exited) break;
	    long retval = ptrace(PTRACE_PEEKUSER, pid, RAX * sizeof(long));
	    cout << retval << endl;
	    //And then get the return value of the system call
	}
	if (WIFEXITED(status)){
	    int returned = WEXITSTATUS(status);
	    cout << "<no return>" << endl;
	    cout << "Program exited normally with status " << returned << endl;
	}
	kill(pid, SIGKILL);
    }else{
	// load system calls
	map<int, string> errors;
	compileSystemCallErrorStrings(errors);

	// Initialize the three provided maps with system call names, 
	//their corresponding numbers,and signatures.
	map<int, string> sysCallNums;
	map<string, int> sysCallNames;
	map<string, systemCallSignature> systemCallSignatures;

	compileSystemCallData(sysCallNums, sysCallNames, systemCallSignatures, rebuild);
	//Rebuild the map of prototype information
	pid_t pid = fork();
	if (pid == 0) {
	    ptrace(PTRACE_TRACEME);
	    //kill(getpid(), SIGSTOP); Kill can cause itself and others to stop
	    raise(SIGSTOP);
	    execvp(argv[numFlags + 1], argv + numFlags + 1);
	    return 0;
	}
	int status;
	waitpid(pid, &status, 0);
	assert(WIFSTOPPED(status));
	ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);
	while(true){
	    bool Exited = false;
	    while (true){
		ptrace(PTRACE_SYSCALL, pid, 0, 0);
		waitpid(pid, &status, 0);
		if (WIFSTOPPED(status) && WSTOPSIG(status) & 0x80){ // If the child is stopped, we can continue
		    break;
		}
		if (WIFEXITED(status)){ //If the child has finished we turn the flag to be true and break the outside lo
		    Exited = true;
		    break;
		}
	    }
	    if (Exited) break;
	    int syscall = ptrace(PTRACE_PEEKUSER, pid, ORIG_RAX * sizeof(long));
	    string sysName = sysCallNums[syscall];
	    cout << sysName << "(";	
	    systemCallSignature args = systemCallSignatures[sysName];
	    for (size_t i = 0; i < args.size(); i++) {
		if (args[i] == SYSCALL_INTEGER) {
		    printf("%ld", get_syscall_arg(pid,i));
		}else if(args[i] == SYSCALL_STRING){
		    long addr = get_syscall_arg(pid,i);
		    string str = readString(pid,addr);
		    cout << "\"" << str <<"\"";
		} else if (args[i] == SYSCALL_POINTER) {
		    long pointer = get_syscall_arg(pid,i);
		    if (pointer==0){
			printf("NULL");	
		    }else{
			printf("%#lx",pointer);
		    }
		} else {
		    cout << "SYSCALL_UNKNOWN_TYPE";
		}
		if (i != args.size() - 1) {
		    printf(", ");
		}
	    }
	    cout << ") = ";


	    while (true){
		ptrace(PTRACE_SYSCALL, pid, 0, 0);
		waitpid(pid, &status, 0);
		if (WIFSTOPPED(status) && WSTOPSIG(status) & 0x80){ // If the child is stopped, we can continue
		    break;
		}
		if (WIFEXITED(status)){
		    Exited = true;
		    break;
		}
	    }
	    if (Exited) break;
	    long retval = ptrace(PTRACE_PEEKUSER, pid, RAX * sizeof(long));
	    if (syscall == sysCallNames["brk"] || syscall == sysCallNames["mmap"]) {
	        printf("%#lx\n", retval);
	    } else if (retval < 0) {
		cout << -1 << " " << errors[-retval] << " (" << strerror(-retval) << ")" << endl;	
	    }else{
	        cout << retval << endl;
	    }
	    //And then get the return value of the system call
	} 
	if (WIFEXITED(status)){
	    int returned = WEXITSTATUS(status);
            cout << "<no return>" << endl;
            cout << "Program exited normally with status " << returned << endl;
	}
        kill(pid, SIGKILL);

    }


    return 0;
}

