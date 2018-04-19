#ifdef HAVE_CONFIG_H
 #include <config.hpp>
#endif

#ifdef USE_OMP
 #include <omp.h>
#endif

#define EXTERN_PROP
 #include <prop.hpp>

#include <oper.hpp>
#include <timings.hpp>

#include <corrections.hpp>
#include <geometry.hpp>
#include <types.hpp>

const char m_r_mom_conf_qprops_t::tag[NPROP_WITH_QED][3]={"0","FF","F","T","S","P"};
const char mom_conf_lprops_t::tag[NPROP_WITH_QED][3]={"0","F"};

template <typename T>
T get_rotator(const vector<T> &gamma,const int r)
{
  switch(r)
    {
    case 0:
    case 1:
      return (gamma[0]+dcompl_t(0,1)*tau3[r]*gamma[5])/sqrt(2);
      break;
    default:
      return gamma[0];
    }
}

void read_qprop(qprop_t *prop,raw_file_t &file,const dcompl_t &fact,const size_t imom,const int r_si,const int r_so)
{
  //cout<<"Seeking file "<<file.get_path()<<" to mom "<<imom<<" position "<<imom*sizeof(dcompl_t)*NSPIN*NSPIN*NCOL*NCOL<<" from "<<file.get_pos()<<endl;
  file.set_pos(imom*sizeof(dcompl_t)*NSPIN*NSPIN*NCOL*NCOL);
  
  qprop_t temp;
  for(size_t is_so=0;is_so<NSPIN;is_so++)
    for(size_t ic_so=0;ic_so<NCOL;ic_so++)
      for(size_t is_si=0;is_si<NSPIN;is_si++)
	for(size_t ic_si=0;ic_si<NCOL;ic_si++)
	  {
	    dcompl_t c;
	    file.bin_read(c);
	    temp(isc(is_si,ic_si),isc(is_so,ic_so))=c*fact;
	  }
  
  auto rot_si=get_rotator(quaGamma,r_si);
  auto rot_so=get_rotator(quaGamma,r_so);
  *prop=rot_si*temp*rot_so;
}

void read_lprop(lprop_t *prop,raw_file_t &file,const dcompl_t &fact,const size_t imom,const int r_si,const int r_so)
{
  //cout<<"Seeking file "<<file.get_path()<<" to mom "<<imom<<" position "<<imom*sizeof(dcompl_t)*NSPIN*NSPIN*NCOL*NCOL<<" from "<<file.get_pos()<<endl;
  file.set_pos(imom*sizeof(dcompl_t)*NSPIN*NSPIN*NCOL*NCOL);
  
  lprop_t temp;
  for(size_t is_so=0;is_so<NSPIN;is_so++)
    for(size_t ic_so=0;ic_so<NCOL;ic_so++)
      for(size_t is_si=0;is_si<NSPIN;is_si++)
	for(size_t ic_si=0;ic_si<NCOL;ic_si++)
	  {
	    dcompl_t c;
	    file.bin_read(c);
	    temp(is_si,is_so)=c*fact;
	  }
  
  auto rot_si=get_rotator(lepGamma,r_si);
  auto rot_so=get_rotator(lepGamma,r_so);
  *prop=rot_si*temp*rot_so;
}

string get_qprop_tag(const size_t im,const size_t ir,const size_t ikind)
{
  return combine("S_M%zu_R%zu_%s",im,ir,m_r_mom_conf_qprops_t::tag[ikind]);
}

string get_lprop_tag(const size_t ikind)
{
  return combine("L_%s",mom_conf_lprops_t::tag[ikind]);
}

void incorporate_charge(vector<m_r_mom_conf_qprops_t> &props,const double ch)
{
  const double ch2=ch*ch;
  
  for(auto &p : props)
    {
      p.F*=ch;
      p.FF*=ch2;
      p.T*=ch2;
      p.P*=ch2;
      p.S*=ch2;
    }
}

void incorporate_charge(vector<mom_conf_lprops_t> &props,const double ch)
{
  for(auto &p : props)
    p.F*=ch;
}

void build_all_mr_jackkniffed_qprops(vector<jm_r_mom_qprops_t> &jprops,const vector<m_r_mom_conf_qprops_t> &props,bool use_QED,const index_t &im_r_ind)
{
  const index_t im_r_ijack_ind=im_r_ind*index_t({{"ijack",njacks}});
  
#pragma omp parallel for
  for(size_t i_im_r_ijack=0;i_im_r_ijack<im_r_ijack_ind.max();i_im_r_ijack++)
    {
      vector<size_t> im_r_ijack=im_r_ijack_ind(i_im_r_ijack);
      size_t im=im_r_ijack[0],r=im_r_ijack[1],ijack=im_r_ijack[2];
      size_t im_r=im_r_ind({im,r});
      //cout<<"Building jack prop im="<<im<<", r="<<r<<", ijack="<<ijack<<endl;
      
      jm_r_mom_qprops_t &j=jprops[im_r];
      const m_r_mom_conf_qprops_t &p=props[i_im_r_ijack];
      
      j.LO[ijack]+=p.LO;
      if(use_QED)
	for(auto &jp_p : vector<tuple<jqprop_t*,const qprop_t*>>({
	      {&j.PH,&p.FF},
	      {&j.PH,&p.T},
	      {&j.CT,&p.P},
	      {&j.S,&p.S}}))
	  (*get<0>(jp_p))[ijack]+=*get<1>(jp_p);
    }
}

void clusterize_all_mr_jackkniffed_qprops(vector<jm_r_mom_qprops_t> &jprops,bool use_QED,size_t clust_size,const index_t &im_r_ind,const djvec_t &deltam_cr)
{
#pragma omp parallel for
  for(size_t iprop=0;iprop<jprops.size();iprop++)
    jprops[iprop].clusterize_all_mr_props(use_QED,clust_size);
  
  if(use_QED)
#pragma omp parallel for
    for(size_t i=0;i<im_r_ind.max();i++)
      for(size_t ijack=0;ijack<=njacks;ijack++)
	jprops[i].QED[ijack]=
	  jprops[i].PH[ijack]-
 // #warning excluding counterterm on prop
 // 	  0.0*
	  deltam_cr[im_r_ind(i)[0]][ijack]*jprops[i].CT[ijack];
}

void get_inverse_propagators(vector<jqprop_t> &jprop_inv,vector<jqprop_t> &jprop_QED_inv,
			     const vector<jm_r_mom_qprops_t> &jprops,
			     const index_t &im_r_ijackp1_ind)
{
  jprop_inv.resize(glb::im_r_ind.max());
  jprop_QED_inv.resize(glb::im_r_ind.max());
  
#pragma omp parallel for reduction(+:invert_time)
  for(size_t im_r_ijack=0;im_r_ijack<im_r_ijackp1_ind.max();im_r_ijack++)
    {
      //decript indices
      const vector<size_t> im_r_ijack_comps=im_r_ijackp1_ind(im_r_ijack);
      const size_t im=im_r_ijack_comps[0],r=im_r_ijack_comps[1],ijack=im_r_ijack_comps[2];
      const size_t im_r=glb::im_r_ind({im,r});
      
      //compute inverse
      invert_time.start();
      qprop_t prop_inv=jprop_inv[im_r][ijack]=jprops[im_r].LO[ijack].inverse();
      invert_time.stop();
      
      //do the same with QED
      if(use_QED)
	{
	  invert_time.start(); //This misses a sign -1 coming from the original inverse
	  jprop_QED_inv[im_r][ijack]=prop_inv*jprops[im_r].QED[ijack]*prop_inv;
	  invert_time.stop();
	}
    }
}

double m0_of_kappa(double kappa)
{
  return 1.0/(2.0*kappa)-4;
}

double M_of_p(const p_t &p,double kappa)
{
  return m0_of_kappa(kappa)+p.hat().norm2()/2.0;
}

lprop_t free_prop(const imom_t &pi,double mu,double kappa,size_t r)
{
  const p_t p=pi.p(L);
  const p_t ptilde=p.tilde();
  
  double M=M_of_p(p,kappa);
  dcompl_t I=dcompl_t(0.0,1.0);
  
  lprop_t out=-I*lep_slash(ptilde)+mu*lepGamma[0]+I*M*lepGamma[5]*tau3[r];
  out/=sqr(M)+sqr(mu)+ptilde.norm2();
  out/=V;
  
  return out;
}

vector<m_r_mom_conf_qprops_t> read_all_qprops_mom(vector<raw_file_t> &files,const index_t &im_r_ijack_ind,const size_t i_in_clust_ihit,const size_t imom)
{
  //! output
  vector<m_r_mom_conf_qprops_t> props(im_r_ijack_ind.max());
  
  //! getting the correct file
  const index_t im_r_iconf_ihit_ikind_ind=get_im_r_iconf_ihit_iqkind_ind();
  
  //decompose the outer index
  const vector<size_t> i_in_clust_ihit_comp=i_in_clust_ihit_ind(i_in_clust_ihit);
  const size_t i_in_clust=i_in_clust_ihit_comp[0],ihit=i_in_clust_ihit_comp[1];
  
  //! index of all that must be read
  const index_t im_r_ijack_ikind_ind=im_r_ijack_ind*index_t({{"ikind",m_r_mom_conf_qprops_t::nprop_kind()}});
#pragma omp parallel for
  for(size_t im_r_ijack_ikind=0;im_r_ijack_ikind<im_r_ijack_ikind_ind.max();im_r_ijack_ikind++)
    {
      const vector<size_t> im_r_ijack_ikind_comps=im_r_ijack_ikind_ind(im_r_ijack_ikind);
      const size_t im=im_r_ijack_ikind_comps[0];
      const size_t r=im_r_ijack_ikind_comps[1];
      const size_t ijack=im_r_ijack_ikind_comps[2];
      const size_t ikind=im_r_ijack_ikind_comps[3];
      
      //! index of the conf built from ijack and i_in_clust
      const size_t iconf=i_in_clust+clust_size*ijack;
      
      //! index of the file to use
      //cout<<im<<" "<<r<<" "<<iconf<<" "<<ihit<<" "<<ikind<<endl;
      const size_t im_r_iconf_ihit_ikind=im_r_iconf_ihit_ikind_ind({im,r,iconf,ihit,ikind});
      
      //! index of the output propagator
      const size_t im_r_ijack=im_r_ijack_ind({im,r,ijack});
      
      read_qprop(props[im_r_ijack].kind[ikind],files[im_r_iconf_ihit_ikind],m_r_mom_conf_qprops_t::coeff_to_read(ikind,r),imom,r,r);
    }
  
  return props;
}

vector<mom_conf_lprops_t> perens_t::read_all_lprops_mom(vector<raw_file_t> &files,const size_t i_in_clust_ihit,const size_t imom)
{
  //! output
  vector<mom_conf_lprops_t> props(njacks);
  
  //decompose the outer index
  const vector<size_t> i_in_clust_ihit_comp=i_in_clust_ihit_ind(i_in_clust_ihit);
  const size_t i_in_clust=i_in_clust_ihit_comp[0],ihit=i_in_clust_ihit_comp[1];
  
  //! index of all that must be read
  const index_t ijack_ikind_ind=index_t({{"ijack",njacks},{"ikind",mom_conf_lprops_t::nprop_kind()}});
#pragma omp parallel for
  for(size_t ijack_ikind=0;ijack_ikind<ijack_ikind_ind.max();ijack_ikind++)
    {
      const vector<size_t> ijack_ikind_comps=ijack_ikind_ind(ijack_ikind);
      const size_t ijack=ijack_ikind_comps[0];
      const size_t ikind=ijack_ikind_comps[1];
      
      //! index of the conf built from ijack and i_in_clust
      const size_t iconf=i_in_clust+clust_size*ijack;
      
      //! index of the file to use
      const size_t iconf_ihit_ikind=iconf_ihit_ikind_ind({iconf,ihit,ikind});
      
      read_lprop(props[ijack].kind[ikind],files[iconf_ihit_ikind],1.0,imom,0,0); //r of lprop is always 0
    }
  
  return props;
}
