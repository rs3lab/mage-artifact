#include <disagg/profile_points_disagg.h>

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/percpu.h>
#include <linux/slab.h>

// ----------------------------------------
// Profile Point Operations
// ----------------------------------------

#ifdef CONFIG_PROFILING_POINTS

// These variables are created by GCC when it's linking our special
// ".profile.point" binary section. They refer to the first and last
// profile points defined _anywhere_ in the program.
extern struct profile_point __sprofilepoint[], __eprofilepoint[];

static void init_pp(struct profile_point *pp)
{
	int cpu;

	pp->sample_mode = false;
	pp->percpu = __alloc_percpu(sizeof(*pp->percpu), L1_CACHE_BYTES);
	// Prevent string overflow. Juuust in case.
	pp->pp_name[PP_MAX_NAME_LEN-1] = '\0'; 
	BUG_ON(!pp->percpu);
	for_each_possible_cpu(cpu) {
		struct profile_point_percpu *pcpu = per_cpu_ptr(pp->percpu, cpu);
		pcpu->nr = 0;
		pcpu->time_ns = 0;
		pcpu->sample_after = PP_SAMPLE_MIN_DELAY +
			(prandom_u32() % (PP_SAMPLE_MAX_DELAY - PP_SAMPLE_MIN_DELAY));
		memset(pcpu->time_samples, 0, sizeof(pcpu->time_samples));
	}
}

void init_pps(void)
{
	struct profile_point *pp;
	for (pp = __sprofilepoint; pp < __eprofilepoint; pp++)
		init_pp(pp);
}
EXPORT_SYMBOL(init_pps);

static void destroy_pp(struct profile_point *pp)
{
	if (pp->percpu)
		 free_percpu(pp->percpu);
}

void destroy_pps(void)
{
	struct profile_point *pp;
	for (pp = __sprofilepoint; pp < __eprofilepoint; pp++)
		destroy_pp(pp);
}
EXPORT_SYMBOL(destroy_pps);

static void print_pp(struct profile_point *pp)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		struct timespec ts = ns_to_timespec(per_cpu(pp->percpu->time_ns, cpu));
		pr_info("pp: %s, cpu: %d, time_sec: %lld.%09Ld, nr: %ld\n",
				pp->pp_name, cpu,
				(s64)ts.tv_sec, (s64)ts.tv_nsec,
				per_cpu(pp->percpu->nr, cpu));
	}
}

void print_pps(void)
{
	struct profile_point *pp;
	pr_info("Kernel Profile Points: Begin Statistics\n");
	for (pp = __sprofilepoint; pp < __eprofilepoint; pp++) {
		print_pp(pp);
		msleep_interruptible(10);
		cond_resched();
	}
	pr_info("Kernel Profile Points: End Statistics\n");
}
EXPORT_SYMBOL(print_pps);

static char print_buf[2048];
// This is buggy. But it works for now.
static void print_sample_buf(unsigned long *buf, int buf_size)
{
	int i;
	for (i = 0; i < buf_size; i += 8) {
		int j, bytes_in_line = 0;
		memset(print_buf, 0, 2048);

		// Try to print 8 numbers per line.
		for (j = 0; j < 8 && (i + j) < buf_size; j++) {
			bytes_in_line += snprintf(
					print_buf + bytes_in_line, // start offset
					2048 - bytes_in_line, // space left
					"%lu ", buf[i + j]);
		}
		print_buf[2048 - 1] = '\0';
		pr_info("%s\n", print_buf);
	}
}


static void print_pp_sample(struct profile_point *pp)
{
	int cpu;
	pr_info("Begin Sampled Latencies(%s):\n", pp->pp_name);
	for_each_possible_cpu(cpu) {
		struct profile_point_percpu *percpu;
		percpu = per_cpu_ptr(pp->percpu, cpu);

		if (percpu->num_samples == 0)
			 continue;

		pr_info("Sampled Latencies from CPU %d: (%d samples)\n", cpu, percpu->num_samples);
		print_sample_buf(percpu->time_samples, percpu->num_samples);
		msleep_interruptible(25);
		cond_resched();
	}
	pr_info("End Sampled Latencies(%s):\n", pp->pp_name);
}

void print_pp_samples(void)
{
	struct profile_point *pp;

	pr_info("Kernel Profile Points: Begin Sampled Latencies: (min_delay=%d max_delay=%d)\n",
			PP_SAMPLE_MIN_DELAY, PP_SAMPLE_MAX_DELAY);

	for (pp = __sprofilepoint; pp < __eprofilepoint; pp++) {
		print_pp_sample(pp);
		msleep_interruptible(50);
		cond_resched();
	}

	pr_info("Kernel Profile Points: End Sampled Latencies\n");
}
EXPORT_SYMBOL(print_pp_samples);

static void clear_pp(struct profile_point *pp)
{
	int cpu;
	pp->sample_mode = false;
	for_each_possible_cpu(cpu) {
		struct profile_point_percpu *percpu;
		percpu = per_cpu_ptr(pp->percpu, cpu);
		percpu->nr = 0;
		percpu->time_ns = 0;
		percpu->num_samples = 0; // no need to clear array now
	}
}

void clear_pps(void)
{
	struct profile_point *pp;
	for (pp = __sprofilepoint; pp < __eprofilepoint; pp++)
		clear_pp(pp);
}
EXPORT_SYMBOL(clear_pps);

void set_pp_sample_mode(struct profile_point *pp, bool mode)
{
	WRITE_ONCE(pp->sample_mode, mode);
	smp_wmb();
}
EXPORT_SYMBOL(set_pp_sample_mode);

void set_pps_sample_mode(bool mode)
{
	struct profile_point *pp;
	for (pp = __sprofilepoint; pp < __eprofilepoint; pp++)
		 set_pp_sample_mode(pp, mode);
}
EXPORT_SYMBOL(set_pps_sample_mode);

#else //  CONFIG_PROFILING_POINTS

void init_pps(void) { }
EXPORT_SYMBOL(init_pps);

void destroy_pps(void) { }
EXPORT_SYMBOL(destroy_pps);

void print_pps(void)
{
	pr_info("Kernel Profile Points: Begin Statistics\n");
	pr_info("Kernel Profile Points: End Statistics\n");
}
EXPORT_SYMBOL(print_pps);

void print_pp_samples(void)
{
	pr_info("Kernel Profile Points: Begin Sampled Latencies: (min_delay=0 max_delay=0)\n");
	pr_info("Kernel Profile Points: End Sampled Latencies\n");
}
EXPORT_SYMBOL(print_pp_samples);

void clear_pps(void) { }
EXPORT_SYMBOL(clear_pps);

void set_pp_sample_mode(struct profile_point *pp, bool mode) { }
EXPORT_SYMBOL(set_pp_sample_mode);

void set_pps_sample_mode(bool mode) { }
EXPORT_SYMBOL(set_pps_sample_mode);

#endif

/* vim: set ts=8 sw=8 tw=99 noexpandtab */
