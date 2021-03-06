#ifndef _TIMINGS_HPP
#define _TIMINGS_HPP

#include <tranalisi.hpp>

#ifndef EXTERN_TIMINGS
 #define EXTERN_TIMINGS extern
 #define INIT_TO(...)
#else
 #define INIT_TO(...) = __VA_ARGS__
#endif

const size_t ntimers=10;

EXTERN_TIMINGS time_stats_t ts INIT_TO(ntimers);

//compute the partial times
EXTERN_TIMINGS stopwatch_t &read_time INIT_TO(ts.add("read propagators"));
EXTERN_TIMINGS stopwatch_t &build_props_time INIT_TO(ts.add("build props"));
EXTERN_TIMINGS stopwatch_t &build_verts_time INIT_TO(ts.add("build verts"));
EXTERN_TIMINGS stopwatch_t &build_meslep_verts_time INIT_TO(ts.add("build meslep_verts"));
EXTERN_TIMINGS stopwatch_t &clust_time INIT_TO(ts.add("clusterize"));
EXTERN_TIMINGS stopwatch_t &invert_time INIT_TO(ts.add("invert the props"));
EXTERN_TIMINGS stopwatch_t &proj_time INIT_TO(ts.add("project bilinears"));
EXTERN_TIMINGS stopwatch_t &Zq_time INIT_TO(ts.add("compute Zq"));
EXTERN_TIMINGS stopwatch_t &deltam_cr_time INIT_TO(ts.add("compute deltam_cr"));

#undef EXTERN_TIMINGS
#undef INIT_TO

#endif
