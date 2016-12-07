#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include <fit.hpp>
#include <functional>

using namespace placeholders;

template <class TL,class TD> TD fit_fsedep(const TL &L,const TD &D){return D/(L*L*L);}

//! ansatz fit
template <class Tpars,class Tml,class Ta>
Tpars cont_chir_ansatz(const Tpars &f0,const Tpars &B0,const Tpars &C,const Tpars &K,const Tml &ml,const Ta &a,const Tpars &adep,double L,const Tpars &D,const Tpars &A2ML,const bool chir_an)
{
  Tpars
    Cf04=C/(f0*f0*f0*f0),
    M2=2*B0*ml,
    den=sqr(Tpars(4*M_PI*f0));
  // Ta
  //   Lphys=L*a;
  
  if(chir_an)
    {
      return e2*sqr(f0)*(4*Cf04-(3+16*Cf04)*M2/den*log(M2)+K*M2/den)+sqr(a)*adep+fit_fsedep(L,D)+A2ML*sqr(a)*ml;
    }
  else return e2*sqr(f0)*(4*Cf04+K*M2/den)+sqr(a)*adep+fit_fsedep(L,D)+A2ML*sqr(a)*ml;
}

void cont_chir_fit(const dbvec_t &a,const dbvec_t &z,const dboot_t &f0,const dboot_t &B0,const vector<cont_chir_fit_data_t> &ext_data,const dboot_t &ml_phys,const string &path,bool chir_an)
{
  //set_printlevel(3);
  
  //search max renormalized mass
  double ml_max=0;
  for(auto &data : ext_data)
    ml_max=max(ml_max,dboot_t(data.aml/a[data.ib]/z[data.ib]).ave());
  ml_max*=1.1;
  
  //parameters to fit
  MnUserParameters pars;
  vector<boot_fit_data_t> fit_data;
  
  //set a
  for(size_t ibeta=0;ibeta<a.size();ibeta++)
    {
      add_self_fitted_point(pars,combine("a[%zu]",ibeta),fit_data,a[ibeta]);
      add_self_fitted_point(pars,combine("z[%zu]",ibeta),fit_data,z[ibeta]);
    }
  
  //lec
  size_t if0=add_self_fitted_point(pars,"f0",fit_data,f0);
  size_t iB0=add_self_fitted_point(pars,"B0",fit_data,B0);
  size_t iC=add_fit_par(pars,"C",0,1);
  size_t iK=add_fit_par(pars,"K",0,1);
  
  size_t iadep=add_fit_par(pars,"adep",0.6,1);
  size_t iD=add_fit_par(pars,"D",0,1);
  size_t iA2ML=add_fit_par(pars,"A2ML",0,1);
  
  //set data
  for(size_t idata=0;idata<ext_data.size();idata++)
    fit_data.push_back(boot_fit_data_t(//numerical data
  				       [ext_data,idata]
  				       (vector<double> p,int iel) //dimension 2
  				       {return (ext_data[idata].y[iel]-ext_data[idata].fse[iel])/sqr(p[2*ext_data[idata].ib+0]);},
  				       //ansatz
  				       [idata,&ext_data,iA2ML,iD,iadep,iB0,if0,iC,iK,chir_an]
  				       (vector<double> p,int iel)
  				       {
  					 double a=p[2*ext_data[idata].ib+0];
  					 double z=p[2*ext_data[idata].ib+1];
  					 double ml=ext_data[idata].aml/a/z;
					 double L=ext_data[idata].L;
  					 return cont_chir_ansatz(p[if0],p[iB0],p[iC],p[iK],ml,a,p[iadep],L,p[iD],p[iA2ML],chir_an);
  				       },
  				       //error
  				       dboot_t((ext_data[idata].y-ext_data[idata].fse)/sqr(a[ext_data[idata].ib])).err()));

  //! fit
  size_t iel=0;
  boot_fit_t boot_fit(fit_data,iel);
  MnMigrad migrad(boot_fit,pars);
  
  dbvec_t fit_a(a.size()),fit_z(z.size());
  dboot_t fit_f0,fit_B0,C,K,adep,D,A2ML;
  dboot_t ch2;
  for(iel=0;iel<ml_phys.size();iel++)
    {
      //minimize and print the result
      FunctionMinimum min=migrad();
      MinimumParameters par_min=min.Parameters();
      ch2[iel]=par_min.Fval();
      
      //get back pars
      fit_f0[iel]=par_min.Vec()[if0];
      fit_B0[iel]=par_min.Vec()[iB0];
      C[iel]=par_min.Vec()[iC];
      K[iel]=par_min.Vec()[iK];
      adep[iel]=par_min.Vec()[iadep];
      D[iel]=par_min.Vec()[iD];
      A2ML[iel]=par_min.Vec()[iA2ML];
      
      for(size_t ibeta=0;ibeta<a.size();ibeta++) fit_a[ibeta][iel]=par_min.Vec()[2*ibeta+0];
      for(size_t ibeta=0;ibeta<z.size();ibeta++) fit_z[ibeta][iel]=par_min.Vec()[2*ibeta+1];
    }
  
  //write ch2
  cout<<"Ch2: "<<ch2.ave_err()<<" / "<<fit_data.size()-pars.Parameters().size()<<endl;
  
  //print a and z
  for(size_t ia=0;ia<a.size();ia++)
    cout<<"a["<<ia<<"]: "<<fit_a[ia].ave_err()<<", orig: "<<a[ia].ave_err()<<", ratio: "<<dboot_t(a[ia]/fit_a[ia]-1.0).ave_err()<<endl;
  for(size_t iz=0;iz<z.size();iz++)
    cout<<"Zp["<<iz<<"]: "<<fit_z[iz].ave_err()<<", orig: "<<z[iz].ave_err()<<", ratio: "<<dboot_t(z[iz]/fit_z[iz]-1.0).ave_err()<<endl;
  
  //print parameters
  cout<<"f0: "<<fit_f0.ave_err()<<", orig: "<<f0.ave_err()<<", ratio: "<<dboot_t(fit_f0/f0-1).ave_err()<<endl;
  cout<<"B0: "<<fit_B0.ave_err()<<", orig: "<<B0.ave_err()<<", ratio: "<<dboot_t(fit_B0/B0-1).ave_err()<<endl;
  cout<<"C: "<<C.ave_err()<<endl;
  cout<<"K: "<<K.ave_err()<<endl;
  cout<<"Adep: "<<adep.ave_err()<<endl;
  cout<<"D: "<<D.ave_err()<<endl;
  cout<<"A2ML: "<<A2ML.ave_err()<<endl;
  
  //compute physical result
  const double a_cont=0.0;
  const double inf_vol=pow(10,3);
  
  dboot_t phys_res=cont_chir_ansatz(f0,B0,C,K,ml_phys,a_cont,adep,inf_vol,D,A2ML,chir_an);
  cout<<"Physical result: "<<phys_res.ave_err()<<endl;
  const double mpi0=0.1349766;
  const double mpip=0.13957018;
  cout<<"MP+-MP0: "<<(dboot_t(phys_res/(mpi0+mpip))*1000).ave_err()<<", exp: 4.5936"<<endl;
  
  //prepare plot
  grace_file_t fit_file(path);
  fit_file.set_title("Continuum and chiral limit");
  fit_file.set_xaxis_label("$$ml^{\\overline{MS},2 GeV} [GeV]");
  fit_file.set_yaxis_label("$$(M^2_{\\pi^+}-M^2_{\\pi^0}) [GeV^2]");
  fit_file.set_xaxis_max(ml_max);

  //band of the fit to individual beta
  for(size_t ib=0;ib<a.size();ib++)
    fit_file.write_line(bind(cont_chir_ansatz<double,double,double>,f0.ave(),B0.ave(),C.ave(),K.ave(),_1,a[ib].ave(),adep.ave(),inf_vol,D.ave(),A2ML.ave(),chir_an),1e-6,ml_max);
  //band of the continuum limit
  fit_file.write_polygon(bind(cont_chir_ansatz<dboot_t,double,double>,f0,B0,C,K,_1,a_cont,adep,inf_vol,D,A2ML,chir_an),1e-6,ml_max);
  //data without and with fse
  grace::default_symbol_fill_pattern=grace::FILLED_SYMBOL;
  for(int without_with_fse=0;without_with_fse<2;without_with_fse++)
    {
      for(size_t ib=0;ib<a.size();ib++)
	{
	  fit_file.new_data_set();
	  //put data without fse to brown
	  if(without_with_fse==0) fit_file.set_all_colors(grace::BROWN);
	  
	  for(size_t idata=0;idata<ext_data.size();idata++)
	    if(ext_data[idata].ib==ib)
	      fit_file<<dboot_t(ext_data[idata].aml/z[ib]/a[ib]).ave()<<" "<<
		dboot_t((ext_data[idata].y-without_with_fse*(ext_data[idata].fse+fit_fsedep(ext_data[idata].L,D)))/sqr(a[ib])).ave_err()<<endl;
	}
      //put back colors for data with fse
      if(without_with_fse==0) fit_file.reset_cur_col();
    }
  //data of the continuum-chiral limit
  fit_file.write_ave_err(ml_phys.ave_err(),phys_res.ave_err());
}
