#ifdef HAVE_CONFIG_H
 #include <config.hpp>
#endif

#ifdef USE_OMP
 #include <omp.h>
#endif

#define EXTERN_GEOMETRY
 #include <geometry.hpp>

#include <tools.hpp>
#include <fstream>
#include <iostream>

using namespace std;

double ph_mom[NDIM]={0,0,0,0};

void set_comp_list_of_moms(const string &path,double thresh)
{
  //slightly increment thresh to include border
  thresh*=1+1e-10;
  
  //open the file to read momentum
  ifstream mom_file(path);
  if(not mom_file.good()) CRASH("Unable to open %s",path.c_str());
  
  //open the file to write filtered momentumm
  const string filt_path="filt_mom.txt";
  ofstream filt_file(filt_path);
  if(not filt_file.good()) CRASH("Unable to open %s",filt_path.c_str());
  
  do
    {
      //temporary read the coords
      imom_t c;
      for(auto &ci : c) mom_file>>ci;
      //if coords good, store them
      if(mom_file.good())
	{
	  //store in any case
	  all_moms.push_back(c);
	  
	  //mark if filtered or not
	  const double discr=c.p(L).tilde().p4_fr_p22();
	  const bool filt=(discr<thresh);
	  filt_moms.push_back(filt);
	  
	  filt_file<<c<<" = "<<c.p(L)<<" , discr: "<<discr<<" , filt: "<<filt<<endl;
	}
    }
  while(mom_file.good());
  
  //count the computed momenta
  ncomp_moms=all_moms.size();
  
  //count the number of momenta that passed the filter
  const size_t nthresh=count_if(filt_moms.begin(),filt_moms.end(),[](const bool &i){return i;});
  
  //print stats
  cout<<"Read "<<filt_moms.size()<<" momenta"<<endl;
  cout<<"NFiltered moms (p4/p2^2<"<<thresh<<"): "<<nthresh<<endl;
}

size_t get_mir_mom(size_t imom,size_t imir)
{
  coords_t cm=all_moms[imom];
  
  if(imir&1) cm[0]=-cm[0]-1;
  for(size_t mu=1;mu<NDIM;mu++) cm[mu]*=(1-2*((imir>>mu)&1));
  auto ret=find(all_moms.begin(),all_moms.end(),cm);
  if(ret==all_moms.end()) CRASH("searching imir=%zu of %zu",imom,imom);
  
  return distance(all_moms.begin(),ret);
}
