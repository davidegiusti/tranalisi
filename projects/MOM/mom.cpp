#ifdef HAVE_CONFIG_H
 #include <config.hpp>
#endif

#ifdef USE_OMP
 #include <omp.h>
#endif

#include <tranalisi.hpp>

#include <corrections.hpp>
#include <geometry.hpp>

using vprop_t=vector<prop_t>;
using jverts_t=array<jprop_t,nGamma>;
using vjprop_t=vector<jprop_t>;

int main(int narg,char **arg)
{
  //read input file
  string input_path="input.txt";
  if(narg>=2) input_path=arg[1];
  
  //open input
  raw_file_t input(input_path,"r");
  
  size_t Ls=input.read<size_t>("L"); //!< lattice spatial size
  L[0]=input.read<size_t>("T");
  
  const string act_str=input.read<string>("Action"); //!< action name
  auto act_key=gaz_decr.find(act_str); //!< key in the map of act
  if(act_key==gaz_decr.end()) CRASH("Unable to decript %s",act_str.c_str());
  gaz_t act=act_key->second; //!< action
  
  const double beta=input.read<double>("Beta"); //!< beta
  const double plaq=input.read<double>("Plaq"); //!< plaquette
  
  const string mom_list_path=input.read<string>("MomList"); //!< list of momenta
  const size_t ext_njacks=input.read<size_t>("NJacks"); //!< number of jacks
  
  //! conf range
  range_t conf_range;
  input.expect("ConfRange");
  conf_range.start=input.read<size_t>();
  conf_range.each=input.read<size_t>();
  conf_range.end=input.read<size_t>();
  
  //! template path
  string template_path=input.read<string>("TemplatePath");
  
  //////////////////////////////////////////////////
  
  //set the number of jackknives
  set_njacks(ext_njacks);
  
  //set spatial sizes
  V=L[0]*pow(Ls,NDIM-1);
  for(size_t mu=1;mu<NDIM;mu++) L[mu]=Ls;
  
  //initialize momenta
  get_list_of_moms(mom_list_path);
  get_class_of_equiv_moms();
  //list_all_smom_pairs();
  
  //! list of existing confs
  vector<size_t> conf_list=get_existing_paths_in_range(template_path,conf_range);
  size_t clust_size=trim_to_njacks_multiple(conf_list,true);
  
  //! jackkniffed propagator
  vjprop_t jprop(imoms.size());
  
  //! jackkniffed vertex
  vector<jverts_t> jverts(imoms.size());
  
#pragma omp parallel for
  for(size_t ijack=0;ijack<njacks;ijack++)
    for(size_t iconf=ijack*clust_size;iconf<(ijack+1)*clust_size;iconf++)
      {
	string path=combine(template_path.c_str(),conf_list[iconf]);
#ifdef USE_OMP
	printf("Thread %d/%d reading file %zu/%zu\n",omp_get_thread_num()+1,omp_get_num_threads(),iconf+1,conf_list.size());
#else
	printf("Reading file %zu/%zu\n",iconf+1,conf_list.size());
#endif
	
	coords_t vitL={4,3,3,3};
	size_t nvit_mom=1;
	for(size_t mu=0;mu<NDIM;mu++) nvit_mom*=vitL[mu];
	vector<prop_t> vit_prop(nvit_mom); //!< converted prop
	
	//! source file
	raw_file_t file(path,"r");
	
	//! propagator on a given conf
	vprop_t prop(imoms.size());
	for(size_t is_so=0;is_so<NSPIN;is_so++)
	  for(size_t ic_so=0;ic_so<NCOL;ic_so++)
	    for(size_t imom=0;imom<imoms.size();imom++)
	      for(size_t is_si=0;is_si<NSPIN;is_si++)
		for(size_t ic_si=0;ic_si<NCOL;ic_si++)
		  file.bin_read(prop[imom](isc(is_si,ic_si),isc(is_so,ic_so)));
	
	for(size_t imom=0;imom<imoms.size();imom++)
	  {
	    // build the jackkniffed propagator
	    put_into_cluster(jprop[imom],prop[imom],ijack);
	    
	    //! compute all 16 vertices
	    for(size_t iG=0;iG<nGamma;iG++)
	      put_into_cluster(jverts[imom][iG],prop[imom]*Gamma[iG]*Gamma[5]*prop[imom].adjoint()*Gamma[5],ijack);
	  }
	
	//! print formatted
	raw_file_t converted_file(path+"_converted.txt","w");
	for(size_t ivit_mom=0;ivit_mom<nvit_mom;ivit_mom++)
	  for(size_t isc1=0;isc1<NSPINCOL;isc1++)
	    for(size_t isc2=0;isc2<NSPINCOL;isc2++)
	      for(size_t ri=0;ri<2;ri++)
		converted_file.printf("%.16lg\n",get_re_or_im(vit_prop[ivit_mom](isc1,isc2),ri));
      }
  
  //////////////////////////////////////////// clusterize .///////////////////////////////////////
  
#pragma omp parallel for
  for(size_t imom=0;imom<imoms.size();imom++)
    {
      clusterize(jprop[imom],clust_size);
      for(size_t iG=0;iG<nGamma;iG++) clusterize(jverts[imom][iG],clust_size);
    }
  
  //////////////////////////////// compute the inverse propagator ////////////////////////////////
  
  //! inverse prop
  vjprop_t jprop_inv(jprop.size());
#pragma omp parallel for
  for(size_t imom=0;imom<imoms.size();imom++)
    for(size_t ijack=0;ijack<=njacks;ijack++)
      put_into_jackknife(jprop_inv[imom],get_from_jackknife(jprop[imom],ijack).inverse(),ijack);
  
  //////////////////////////////////// compute Z by amputating //////////////////////////////////
  
  djvec_t Zq(equiv_imoms.size()); //!< Z of quark field
  djvec_t Zq_sig1(equiv_imoms.size()); //!< Z of quark field, alternative definition
  
  //! Z
  enum{iZS,iZA,iZP,iZV,iZT};
  const size_t nZ=5,iZ_of_iG[nGamma]={0,1,1,1,1,2,3,3,3,3,4,4,4,4,4,4};
  const double Zdeg[nZ]={1,4,1,4,6};
  vector<djvec_t> Z(nZ,djvec_t(equiv_imoms.size()));
  
#pragma omp parallel for
  for(size_t ind_mom=0;ind_mom<equiv_imoms.size();ind_mom++)
    {
#ifdef USE_OMP
      printf("Thread %d/%d analyzing mom %zu/%zu\n",omp_get_thread_num()+1,omp_get_num_threads(),ind_mom+1,equiv_imoms.size());
#else
      printf("Analyzing mom %zu/%zu\n",ind_mom+1,equiv_imoms.size());
#endif
      
      //reset Z
      Zq[ind_mom]=Zq_sig1[ind_mom]=0.0;
      for(auto &Zi : Z) Zi[ind_mom]=0.0;
      
      //loop on equivalent moms
      auto &imom_class=equiv_imoms[ind_mom];
      size_t irep_mom=imom_class.first;
      double pt2=imoms[irep_mom].p(L).tilde().norm2();
      for(size_t imom : imom_class.second)
	{
	  p_t ptilde=imoms[imom].p(L).tilde();
	  prop_t pslash=slash(ptilde);
	  
	  for(size_t ijack=0;ijack<=njacks;ijack++)
	    {
	      prop_t prop_inv=get_from_jackknife(jprop_inv[imom],ijack);
	      //zq
	      double Zq_cl=(prop_inv*pslash).trace().imag()/(12.0*pt2*V);
	      Zq[ind_mom][ijack]+=Zq_cl;
	      //sigma1
	      for(size_t mu=0;mu<NDIM;mu++)
		if(fabs(ptilde[mu])>1e-10)
		  {
		    double Zq_sig1_cl=(prop_inv*Gamma[igmu[mu]]).trace().imag()/(12.0*ptilde[mu]*V);
		    Zq_sig1[ind_mom][ijack]+=Zq_sig1_cl;
		  }
	      
	      //project all Z
	      vector<double> pr(nZ,0.0);
	      for(size_t iG=0;iG<nGamma;iG++)
		{
		  prop_t vert=get_from_jackknife(jverts[imom][iG],ijack) ;
		  prop_t amp_vert=prop_inv*vert*Gamma[5]*prop_inv.adjoint()*Gamma[5];
		  pr[iZ_of_iG[iG]]+=(amp_vert*Gamma[iG].adjoint()).trace().real()/12.0;
		}
	      
	      for(size_t iZ=0;iZ<nZ;iZ++) Z[iZ][ind_mom][ijack]+=Zq_cl/pr[iZ];
	    }
	}
      
      //normalize Z
      Zq[ind_mom]/=imom_class.second.size();
      Zq_sig1[ind_mom]/=imoms[irep_mom].Np()*imom_class.second.size();
      for(size_t iZ=0;iZ<nZ;iZ++)
	Z[iZ][ind_mom]/=imom_class.second.size()/Zdeg[iZ];
    }
  
  double g2=6.0/beta;
  double g2tilde=g2/plaq;
  
  //correct Zq
  djvec_t Zq_sub(equiv_imoms.size());
  djvec_t Zq_sig1_sub(equiv_imoms.size());
  for(size_t imom=0;imom<equiv_imoms.size();imom++)
    {
      auto &e=equiv_imoms[imom];
      imom_t irep=imoms[e.first];
      Zq_sub[imom]=Zq[imom]-g2tilde*sig1_a2(act,irep,L);
      Zq_sig1_sub[imom]=Zq_sig1[imom]-g2tilde*sig1_a2(act,irep,L);
    }
  
  //write all Z
  for(auto &p : vector<pair<djvec_t,string>>
    {
      {Zq,"Zq"},
      {Zq_sig1,"Zq_sig1"},
      {Zq_sub,"Zq_sub"},
      {Zq_sig1_sub,"Zq_sig1_sub"},
      {Z[iZS],"ZS"},
      {Z[iZA],"ZA"},
      {Z[iZP],"ZP"},
      {Z[iZV],"ZV"},
      {Z[iZT],"ZT"}
    })
    {
      grace_file_t outf("plots/"+p.second+".xmg");
      outf<<fixed;
      outf.precision(8);
      outf.write_vec_ave_err(get_indep_pt2(),p.first.ave_err());
    }
  
  return 0;
}
