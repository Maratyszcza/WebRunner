#include <stddef.h>
#include <stdio.h>

#include <errno.h>
#include <sys/prctl.h>
#include <sys/resource.h>

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <linux/unistd.h>
#include <sys/ptrace.h>
#include <unistd.h>

#define SyscallArg(n) (offsetof(struct seccomp_data, args[n]))
#define SyscallArch (offsetof(struct seccomp_data, arch))
#define SyscallNr (offsetof(struct seccomp_data, nr))

void enable_sandbox(int connection_socket) {
	const struct rlimit cpu_limit = {
		.rlim_cur = 3,
		.rlim_max = 3
	};
	if (setrlimit(RLIMIT_CPU, &cpu_limit) == -1) {
		fprintf(stderr, "Error: could not set CPU resource limit (error code %d)\n", errno);
		_exit(1);
	}

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) {
		fprintf(stderr, "Error: failed to set PR_SET_NO_NEW_PRIVS (error code %d)\n", errno);
		_exit(1);
	}

	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD + BPF_W + BPF_ABS, SyscallArch),
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, AUDIT_ARCH_X86_64, 1, 0),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_KILL),

		BPF_STMT(BPF_LD + BPF_W + BPF_ABS, SyscallNr),

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_rt_sigreturn, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_exit_group, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_exit, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

		/* Allow write only on the connection socket */
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_write, 0, 4),
		BPF_STMT(BPF_LD + BPF_W + BPF_ABS, SyscallArg(0)),
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, connection_socket, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_KILL),

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_read, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

		/* Allow only anonymous mappings (file descriptor == -1) */
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_mmap, 0, 4),
		BPF_STMT(BPF_LD + BPF_W + BPF_ABS, SyscallArg(4)),
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, -1, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_KILL),

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_munmap, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

		/* Allow lseek only on the connection socket (used internally by glibc dprintf) */
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_lseek, 0, 4),
		BPF_STMT(BPF_LD + BPF_W + BPF_ABS, SyscallArg(0)),
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, connection_socket, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_KILL),

		/* Allow fstat only on the connection socket (used internally by glibc dprintf) */
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_fstat, 0, 4),
		BPF_STMT(BPF_LD + BPF_W + BPF_ABS, SyscallArg(0)),
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, connection_socket, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_KILL),

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_ioctl, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_clock_gettime, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_futex, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_sched_yield, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_TRAP),
	};
	struct sock_fprog prog = {
		.len = sizeof(filter) / sizeof(filter[0]),
		.filter = filter,
	};
	if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) == -1) {
		fprintf(stderr, "Error: could not enter seccomp mode (error code %d)\n", errno);
		_exit(1);
	}
}
