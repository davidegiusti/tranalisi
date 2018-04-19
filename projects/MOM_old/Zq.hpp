#ifndef _ZQ_HPP
#define _ZQ_HPP

#include <meas_vec.hpp>

#include <geometry.hpp>
#include <prop.hpp>

//! compute for a single jack
double compute_Zq(const qprop_t &prop_inv,const size_t glb_imom);

//! compute for a djack_t
djack_t compute_Zq(const jqprop_t &jprop_inv,const size_t glb_imom);

#endif
