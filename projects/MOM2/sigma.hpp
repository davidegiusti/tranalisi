#ifndef _SIGMA_HPP
#define _SIGMA_HPP

#include <tranalisi.hpp>

#ifndef EXTERN_SIGMA
 #define EXTERN_SIGMA extern
 #define INIT_SIGMA_TO(...)
#else
 #define INIT_SIGMA_TO(...) __VA_ARGS__
#endif

namespace sigma
{
  enum ins{LO,  CR,  TM,  PH,  QED};
  EXTERN_SIGMA vector<ins>     ins_list;
  EXTERN_SIGMA vector<size_t>  iins_of_ins;
  EXTERN_SIGMA vector<string>  ins_tag INIT_SIGMA_TO({"LO","CR","TM","PH","QED"});
  
  //! set all sigma insertions
  void set_ins();
  EXTERN_SIGMA size_t nins;
  
  enum proj{SIGMA1,SIGMA2,SIGMA3,SIGMA4};
  EXTERN_SIGMA vector<proj>     proj_list;
  EXTERN_SIGMA size_t nproj;
}

#undef EXTERN_SIGMA
#undef INIT_SIGMA_TO

#endif
