#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#define EXTERN_MINIMIZER
#include <minimizer.hpp>

#if MINIMIZER_TYPE == MINUIT

//! c wrapper
void fcn(int &npar,double *fuf,double &ch,double *p,int flag)
{
  //! put the parameters into a c++ vector
  vector<double> pars(fun_npars);
  pars.assign(p,p+fun_npars);
  //call and return
  ch=(*fun_ptr)(pars);
}

#endif
