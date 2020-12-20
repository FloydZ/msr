#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <stdio.h>
#include <ctype.h>

static inline int dir_filter(const struct dirent *dirp) {
	return isdigit(dirp->d_name[0]) ? 1 : 0;
}

uint64_t get_masked_value(uint64_t old_value, uint64_t new_value, uint64_t mask) {
	return (new_value & mask) | (old_value & ~mask);
}

// 
// return `true` if success. 
bool rdmsr_on_cpu(uint32_t reg, uint32_t cpu, uint64_t *value) {
    char msr_file_name[64];

    sprintf(msr_file_name, "/dev/cpu/%u/msr", cpu);
    int fd = open(msr_file_name, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    const bool success = pread(fd, value, sizeof(value), reg) == sizeof(value);
    close(fd);

    return success;
}

bool wrmsr_on_cpu(uint32_t reg, uint32_t cpu, uint64_t value, uint64_t mask) {
    // If a bit in mask is set to 1, use new value, otherwise use old value
    if (mask != ((uint64_t)(-1))) {
        uint64_t old_value;
        if (rdmsr_on_cpu(reg, cpu, &old_value)) {
            value = get_masked_value(old_value, value, mask);
        }
    }

    char msr_file_name[64];

    sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
    printf("TT: %s\n", msr_file_name);
    int fd = open(msr_file_name, O_WRONLY);
    if (fd < 0) {
        return false;
    }

    const bool success = pwrite(fd, &value, sizeof(value), reg) == sizeof(value);

    close(fd);

    return success;
}

bool wrmsr_on_all_cpus(uint32_t reg, uint64_t value, uint64_t mask) {
    struct dirent **namelist;
    int dir_entries = scandir("/dev/cpu", &namelist, dir_filter, 0);
    int errors      = 0;

    while (dir_entries--) {
        if (!wrmsr_on_cpu(reg, strtoul(namelist[dir_entries]->d_name, NULL, 10), value, mask)) {
            ++errors;
        }

        free(namelist[dir_entries]);
    }

    free(namelist);

    if (errors) {
        // LOG_WARN(CLEAR "%s" YELLOW_BOLD_S "cannot set MSR 0x%08" PRIx32 " to 0x%08" PRIx64, tag, reg, value);
        printf("warn wrmsr_on_all_cpus\n");
    }

    return errors == 0;
}

bool wrmsr(uint32_t reg, uint32_t cpu, uint64_t value, uint64_t mask) {
	/*if (cacheQoSDisabled || (cpu >= cacheEnabled.size()) || cacheEnabled[cpu]) {
		// Assign Class Of Service 0 to current CPU core (default, full L3 cache available)
		if (!wrmsr_on_cpu(0xC8F, cpu, 0, -1)) {
			return false;
		}
	} else {*/
	// Disable L3 cache for Class Of Service 1
	if (!wrmsr_on_cpu(0xC91, cpu, 0, -1)) {
		// Some CPUs don't let set it to all zeros
		if (!wrmsr_on_cpu(0xC91, cpu, 1, -1)) {
			return false;
		}
	}

	// Assign Class Of Service 1 to current CPU core
	if (!wrmsr_on_cpu(0xC8F, cpu, 1ULL << 32, -1)) {
		return false;
	}
	//}
	return true;
}

int main() {
	bool result = wrmsr_on_all_cpus(0xC8F, 0, -1);
	printf("Return: %d\n", result);
	return result;
}