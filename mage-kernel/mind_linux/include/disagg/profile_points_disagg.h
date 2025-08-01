/*
 * Profiling points
 *
 * We borrowed profiling system from LegoOS.
 * LegoOS: https://github.com/WukLab/LegoOS
 * 
 * We added profiling outside of kernel itself.
 * For example, kernel module which is not compiled with 
 * the kernel can use profiling points defined in the kernel
 * and use PP_EXIT_PTR instead of PP_EXIT.
 * Those profiling points used outside of the kernel
 * should be exported in this file by using 
 * PROTO_PROFILE_WITH_EXPORT().
 * TODO(yash): not anymore. Need to document our new system...
 */

#ifndef __PROFILE_POINT_DISAGGREGATION_H__
#define __PROFILE_POINT_DISAGGREGATION_H__

#include <linux/kernel.h>
#include <linux/sched/clock.h>
#include <linux/stringify.h>
#include <linux/percpu.h>
#include <linux/types.h>
#include <linux/random.h>

#include <linux/smp.h>
#include <linux/percpu.h>
#include <linux/types.h>

#ifndef __MEMORY_NODE__
// Inherit CONFIG_PROFILING_POINTS from the main print macro file.
#include <disagg/print_disagg.h>
#else
// Do not define CONFIG_PROFILING_POINTS.
#endif


#ifdef CONFIG_PROFILING_POINTS

#define PP_SAMPLE_MIN_DELAY 256
#define PP_SAMPLE_MAX_DELAY 2048
#define PP_SAMPLE_BUF_SIZE 128
#define PP_MAX_NAME_LEN 64

// Keep this below a cache line size.
struct profile_point_percpu {
	unsigned long nr;
	unsigned long time_ns;
	int           num_samples;
	int           sample_after;
	unsigned long time_samples[PP_SAMPLE_BUF_SIZE];
};

struct profile_point {
	// Keep per-cpu critical fields at the beginning of the struct, so they're always in the
	// same cache line.
	struct profile_point_percpu __percpu *percpu;
	bool           sample_mode;
	char	       pp_name[PP_MAX_NAME_LEN];
} ____cacheline_aligned;


// Variable Name mangling.
#define _PP_NAME(name)            __profilepoint_##name
#define _PP_TIME(name)            __profilepoint_start_ns_##name
#define _PP_BIN(name, low, high)  name##_##low##_to_##high

// Note: The variable is defined here. But the per-CPU fields must be
// initialized with `init_pps()` at runtime.
#define _DEFINE_PP_VAR(name)                                          \
	struct profile_point _PP_NAME(name) __section(.profile.point) = { \
		.pp_name = __stringify(name),                                 \
		.sample_mode = false,                                         \
		.percpu = NULL,                                               \
	}

#define _DECLARE_PP_VAR(name)                         \
	extern struct profile_point _PP_NAME(name)

#define _DEFINE_PP_BINNED(name, bin1, bin2, bin3)  \
	_DEFINE_PP_VAR(name);                      \
	_DEFINE_PP_VAR(_PP_BIN(name, 0, bin1));    \
	_DEFINE_PP_VAR(_PP_BIN(name, bin1, bin2)); \
	_DEFINE_PP_VAR(_PP_BIN(name, bin2, bin3)); \
	_DEFINE_PP_VAR(_PP_BIN(name, bin3, inf));

#define _DECLARE_PP_BINNED(name, bin1, bin2, bin3)  \
	_DECLARE_PP_VAR(name);                      \
	_DECLARE_PP_VAR(_PP_BIN(name, 0, bin1));    \
	_DECLARE_PP_VAR(_PP_BIN(name, bin1, bin2)); \
	_DECLARE_PP_VAR(_PP_BIN(name, bin2, bin3)); \
	_DECLARE_PP_VAR(_PP_BIN(name, bin3, inf));

#define _PP_INCREMENT_BINNED(name, time, bin1, bin2, bin3)         \
	do {                                                     \
		if (time < bin1) {                               \
			PP_INCREMENT(_PP_BIN(name, 0, bin1));    \
		} else if (time < bin2) {                        \
			PP_INCREMENT(_PP_BIN(name, bin1, bin2)); \
		} else if (time < bin3) {                        \
			PP_INCREMENT(_PP_BIN(name, bin2, bin3)); \
		} else {                                         \
			PP_INCREMENT(_PP_BIN(name, bin3, inf));  \
		}                                                \
	} while (0)


/* ==========================================================
 * Profiling Point API
 * ==========================================================
 */

/*
 * Define a profile point. Should be done once; in a C file.
 * Once a profile point is defined, it'll automatically be printed
 * in `print_pps()` and initialized in `init_pps()`.
 */
#define DEFINE_PP(name)  \
	_DEFINE_PP_VAR(name)

#define DEFINE_AND_EXPORT_PP(name)             \
	_DEFINE_PP_VAR(name);                  \
	EXPORT_SYMBOL(_PP_NAME(name))

/*
 * Define a series of profile points, one for each bin.
 * Useful for "binning" by value.
 */
#define DEFINE_PP_BINNED(name, time1, time2, time3)   \
	_DEFINE_PP_BINNED(name, time1, time2, time3)

/*
 * Declare a profile point ("extern profile_point yee"). Should be invoked once in the files where
 * a PP is being used.
 */
#define DECLARE_PP(name) _DECLARE_PP_VAR(name)

/*
 * Define a series of profile points, one for each bin.
 * Useful for "binning" by value. Example: `DECLARE_PP_BINNED(latency, 100, 200, 400, 500);`
 */
#define DECLARE_PP_BINNED(name, time1, time2, time3) \
	_DECLARE_PP_BINNED(name, time1, time2, time3)

/*
 * Declare one of these at the head of every function where you use a PP
 */
#define PP_STATE(name) \
	unsigned long _PP_TIME(name) __maybe_unused

#define PP_ENTER(name)                                      \
	do {                                                \
		barrier();                                  \
		WRITE_ONCE(_PP_TIME(name), sched_clock());  \
	} while (0)

#define _PP_EXIT(name, percpu)                                                                  \
	do {                                                                                    \
		unsigned long __PP_end_time;                                                    \
		unsigned long __PP_diff_time;                                                   \
		                                                                                \
		if (!_PP_NAME(name).sample_mode)                                                \
			break;                                                                  \
		                                                                                \
		barrier();                                                                      \
		WRITE_ONCE(__PP_end_time, sched_clock());                                       \
		__PP_diff_time = __PP_end_time - _PP_TIME(name);                                \
		this_cpu_inc(percpu->nr);                                                       \
		this_cpu_add(percpu->time_ns, __PP_diff_time);                                  \
		                                                                                \
		if (this_cpu_read(percpu->num_samples) == PP_SAMPLE_BUF_SIZE)                   \
			break;                                                                  \
		                                                                                \
		if (this_cpu_read(percpu->sample_after) > 0) {                                  \
			this_cpu_dec(percpu->sample_after);                                     \
			break;                                                                  \
		}                                                                               \
		this_cpu_write(percpu->time_samples[this_cpu_read(percpu->num_samples)],        \
				__PP_diff_time);                                                \
		this_cpu_inc(percpu->num_samples);                                              \
		this_cpu_write(percpu->sample_after,                                            \
				PP_SAMPLE_MIN_DELAY +                                           \
				(prandom_u32() % (PP_SAMPLE_MAX_DELAY - PP_SAMPLE_MIN_DELAY))); \
	} while (0)

#define PP_EXIT(name)      _PP_EXIT(name, _PP_NAME(name).percpu)

/*
 * Same as PP exit -- except instead of automatically sampling the current time, just record
 * a sample that the user provided.
 */
#define _PP_RECORD(name, percpu, sample)                                                        \
	do {                                                                                    \
		unsigned long __PP_sample = sample;                                             \
		                                                                                \
		if (!_PP_NAME(name).sample_mode)                                                \
			break;                                                                  \
		                                                                                \
		barrier();                                                                      \
		                                                                                \
		this_cpu_inc(percpu->nr);                                                       \
		this_cpu_add(percpu->time_ns, __PP_sample);                                     \
		                                                                                \
		if (this_cpu_read(percpu->num_samples) == PP_SAMPLE_BUF_SIZE)                   \
			break;                                                                  \
		                                                                                \
		if (this_cpu_read(percpu->sample_after) > 0) {                                  \
			this_cpu_dec(percpu->sample_after);                                     \
			break;                                                                  \
		}                                                                               \
		this_cpu_write(percpu->time_samples[this_cpu_read(percpu->num_samples)],        \
				__PP_sample);                                                   \
		this_cpu_inc(percpu->num_samples);                                              \
		this_cpu_write(percpu->sample_after,                                            \
				PP_SAMPLE_MIN_DELAY +                                           \
				(prandom_u32() % (PP_SAMPLE_MAX_DELAY - PP_SAMPLE_MIN_DELAY))); \
	} while (0)

#define PP_RECORD(name, sample)   _PP_RECORD(name, _PP_NAME(name).percpu, sample)

#define PP_INCREMENT(name)                               \
	do {                                             \
		if (!_PP_NAME(name).sample_mode)         \
			break;                           \
		this_cpu_inc(_PP_NAME(name).percpu->nr); \
	} while (0)

#define PP_ADD(name, count)                                     \
	do {                                                    \
		if (!_PP_NAME(name).sample_mode)                \
			break;                                  \
		this_cpu_add(_PP_NAME(name).percpu->nr, count); \
	} while (0)

#define _PP_SAMPLE(name, percpu, sample)                                                        \
	do {                                                                                    \
		if (!_PP_NAME(name).sample_mode)                                                \
			break;                                                                  \
		                                                                                \
		this_cpu_inc(percpu->nr);                                                       \
		                                                                                \
		if (this_cpu_read(percpu->num_samples) == PP_SAMPLE_BUF_SIZE)                   \
			break;                                                                  \
		                                                                                \
		this_cpu_write(percpu->time_samples[this_cpu_read(percpu->num_samples)],        \
				sample);                                                        \
		this_cpu_inc(percpu->num_samples);                                              \
	} while (0)

/*
 * Unconditionally log a sample into our PP's sample buffer.  
 */
#define PP_SAMPLE(name, sample) _PP_SAMPLE(name, _PP_NAME(name).percpu, sample)

/*
 * Suppose there's a series of profile points, one for each bin.
 * This will increment the correct bin, based on the passed value.
 * Example: `PP_INCREMENT_BINNED(LatencyProfilePoint, time, 100, 200, 400, 500);`
 */
#define PP_EXIT_BINNED(name, bin1, bin2, bin3)                                \
	do {                                                                  \
		unsigned long __PP_end_time;                                  \
		unsigned long __PP_diff_time;                                 \
                                                                              \
		if (!_PP_NAME(name).sample_mode)                              \
			break;                                                \
		__PP_end_time = sched_clock();                                \
		__PP_diff_time = __PP_end_time - _PP_TIME(name);              \
		this_cpu_inc(_PP_NAME(name).percpu->nr);                      \
		this_cpu_add(_PP_NAME(name).percpu->time_ns, __PP_diff_time); \
		_PP_INCREMENT_BINNED(name, __PP_diff_time, bin1, bin2, bin3); \
	} while (0)

#define PP_READ_PER_CPU(name, field)                        \
	this_cpu_read(_PP_NAME(name).percpu->field)

#else // CONFIG_PROFILING_POINTS

struct profile_point_percpu { };
struct profile_point { };
// Variable Name mangling.
#define _PP_NAME(name)            __profilepoint_##name
#define _PP_TIME(name)            __profilepoint_start_ns_##name
#define _PP_BIN(name, low, high)  name##_##low##_to_##high

/* ==========================================================
 * Profiling Point API
 * ==========================================================
 */

#define DEFINE_PP(name)
// #define DEFINE_AND_EXPORT_PP(name)
#define DEFINE_PP_BINNED(name, time1, time2, time3)
#define DECLARE_PP(name)
#define DECLARE_PP_BINNED(name, time1, time2, time3)

#define PP_STATE(name) unsigned long _PP_TIME(name) __maybe_unused
#define PP_ENTER(name)           do { } while(0)
#define PP_EXIT(name)            do { } while(0)
#define PP_RECORD(name, sample)  do { } while(0)
#define PP_INCREMENT(name)       do { } while (0)
#define PP_EXIT_BINNED(name, bin1, bin2, bin3) do { } while(0)
#define PP_READ_PER_CPU(name, field) (-1L)

#endif

void init_pps(void);
void destroy_pps(void);
void print_pps(void);
void print_pp_samples(void);
void clear_pps(void);
void set_pp_sample_mode(struct profile_point *pp, bool mode);
void set_pps_sample_mode(bool mode);

#endif /* __PROFILE_POINT_DISAGGREGATION_H__ */
