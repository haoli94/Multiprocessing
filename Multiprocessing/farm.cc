#include <cassert>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include "subprocess.h"

using namespace std;

struct worker {
	worker() {}
	worker(char *argv[]) : sp(subprocess(argv, true, false)), available(false) {}
	subprocess_t sp;
	bool available;
};

static const size_t kNumCPUs = sysconf(_SC_NPROCESSORS_ONLN);
// restore static keyword once you start using these, commented out to suppress compiler warning
/* static */ vector<worker> workers(kNumCPUs);
/* static */ size_t numWorkersAvailable = 0;

static void markWorkersAsAvailable(int sig) {
	while(true){
		pid_t pid = waitpid(-1,NULL,WNOHANG|WUNTRACED);
		if (pid <= 0){
			break;
		}
		for (size_t i = 0; i < kNumCPUs; i++){
			if (workers[i].sp.pid == pid){
				workers[i].available = true;
				numWorkersAvailable++;
				break;
			}
		}
	}
}

// restore static keyword once you start using it, commented out to suppress compiler warning
static const char *kWorkerArguments[] = {"./factor.py", "--self-halting", NULL};
static void spawnAllWorkers() {
	cout << "There are this many CPUs: " << kNumCPUs << ", numbered 0 through " << kNumCPUs - 1 << "." << endl;
	sigset_t sigMask;
	sigemptyset(&sigMask);
	sigaddset(&sigMask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &sigMask, NULL);

	for (size_t i = 0; i < kNumCPUs; i++) {
		struct worker w(const_cast<char **>(kWorkerArguments));
		workers[i] = w;
		cpu_set_t cpu_set;
		CPU_ZERO(&cpu_set);
		CPU_SET(i, &cpu_set);
		sched_setaffinity(workers[i].sp.pid, sizeof(cpu_set_t), &cpu_set);
		cout << "Worker " << workers[i].sp.pid << " is set to run on CPU " << i << "." << endl;
	}
	sigprocmask(SIG_UNBLOCK, &sigMask, NULL);
}

// restore static keyword once you start using it, commented out to suppress compiler warning
static size_t getAvailableWorker() {
	sigset_t additions, existingmask;
	sigemptyset(&additions);
	sigaddset(&additions, SIGCHLD);
	sigprocmask(SIG_BLOCK, &additions, &existingmask); 
	while (numWorkersAvailable == 0) {
		sigsuspend(&existingmask);
	}
	for (size_t i = 0; i < workers.size(); i++) {
		if (workers[i].available) {
			workers[i].available = false;
			numWorkersAvailable--;
			sigprocmask(SIG_UNBLOCK, &additions, NULL);
			return i;
		}
	}
	throw;
}

static void broadcastNumbersToWorkers() {
	while (true) {
		string line;
		getline(cin, line);
		if (cin.fail()) break;
		size_t endpos;
		/* long long num = */ stoll(line, &endpos);
		if (endpos != line.size()) break;
		size_t w = getAvailableWorker();
		write(workers[w].sp.supplyfd, line.c_str(), line.size());
		write(workers[w].sp.supplyfd, "\n", 1);
		kill(workers[w].sp.pid, SIGCONT);
		// you shouldn't need all that many lines of additional code
	}
}

static void waitForAllWorkers() {
	sigset_t additions,existing;
	sigemptyset(&additions);
	sigaddset(&additions, SIGCHLD);
	sigprocmask(SIG_BLOCK, &additions, &existing);
	while (numWorkersAvailable != workers.size()) {
		sigsuspend(&existing);
	}
	sigprocmask(SIG_UNBLOCK, &additions, NULL);
}

static void closeAllWorkers() {
	signal(SIGCHLD, SIG_DFL);
	for (const worker& w : workers) {
		close(w.sp.supplyfd);
		kill(w.sp.pid, SIGCONT);
	}
	while (true) {
    	pid_t pid = waitpid(-1, NULL, 0);
    	if (pid == -1) break;
	}
}

int main(int argc, char *argv[]) {
	signal(SIGCHLD, markWorkersAsAvailable);
	spawnAllWorkers();
	broadcastNumbersToWorkers();
	waitForAllWorkers();
	closeAllWorkers();
	return 0;
}
