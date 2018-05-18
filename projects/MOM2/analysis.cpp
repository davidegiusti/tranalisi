#ifdef HAVE_CONFIG_H
 #include <config.hpp>
#endif

#define EXTERN_ANALYSIS
 #include <MOM2/analysis.hpp>

 #include <MOM2/Zq.hpp>

#define ASSERT_COMPATIBLE_MEMBER(MEMBER)			\
  if(data(in1,ASSERT_PRESENT).MEMBER!=				\
     data(in2,ASSERT_PRESENT).MEMBER)				\
    CRASH("Impossible to combine, different member " #MEMBER)

void assert_compatible(const string in1,const string in2)
{
  for(size_t mu=0;mu<NDIM;mu++) ASSERT_COMPATIBLE_MEMBER(L[mu]);
  ASSERT_COMPATIBLE_MEMBER(nm);
  ASSERT_COMPATIBLE_MEMBER(nr);
  ASSERT_COMPATIBLE_MEMBER(linmoms);
  ASSERT_COMPATIBLE_MEMBER(bilmoms);
}

void plot_all_Z(const string &suffix)
{
  needs_to_read_Z();
  
  for(auto &path : pars::ens)
    data(path,ASSERT_PRESENT)
      .plot_Z(suffix);
}

void average_Z(const string out,const string in1,const string in2)
{
  invalidate_ingredients();
  
  needs_to_read_Z();
  
  assert_compatible(in1,in2);
  
  cout<<"Averaging Z for"<<in1<<" and "<<in2<<" into "<<out<<endl;
  
  perens_t temp=data(in1,ASSERT_PRESENT);
  
  for(auto &p : temp.get_all_Ztasks({&data(in2,ASSERT_PRESENT)}))
    {
      cout<<" "<<p.tag<<endl;
      
      djvec_t &out=*p.out;
      const djvec_t &in=*p.in.front();
      
      out+=in;
      out/=2.0;
    }
  
  //remove from the list
  for(auto in : {in1,in2})
    data_erase(in);
  
  //add to the list
  pars::ens.push_back(out);
  data(out,PRESENCE_NOT_NEEDED)=temp;
  data(out,PRESENCE_NOT_NEEDED).dir_path=out;
}

void average_ingredients(const string out_name,const string in1,const string in2)
{
  invalidate_Z();
  
  needs_to_read_ingredients();
  
  assert_compatible(in1,in2);
  
  cout<<"Averaging ingredients for "<<in1<<" and "<<in2<<" into "<<out_name<<endl;
  
  perens_t temp=data(in1,ASSERT_PRESENT);
  
  for(auto &p : temp.get_all_ingredients({&data(in2,ASSERT_PRESENT)}))
    {
      cout<<" "<<p.tag<<endl;
      
      djvec_t &out=*p.out;
      const djvec_t &in=*p.in.front();
      
      out+=in;
      out/=2.0;
    }
  auto &dout=temp;
  const auto &din1=data(in1,PRESENCE_NOT_NEEDED);
  const auto &din2=data(in2,PRESENCE_NOT_NEEDED);
  dout.meson_mass_sea=(din1.meson_mass_sea+din2.meson_mass_sea)/2.0;
  for(size_t im=0;im<dout.nm;im++)
    for(size_t r=0;r<dout.nr;r++)
      {
	size_t imr=dout.im_r_ind({im,r});
	cout<<"deltam_cr[m="<<im<<",r="<<r<<"]: "<<
	  smart_print(dout.deltam_cr[imr].ave_err())<<" = [ "<<
	  smart_print(din1.deltam_cr[imr].ave_err())<<" + "<<
	  smart_print(din2.deltam_cr[imr].ave_err())<<" )/2.0"<<endl;
	cout<<"deltam_tm[m="<<im<<",r="<<r<<"]: "<<
	  smart_print(dout.deltam_tm[imr].ave_err())<<" = [ "<<
	  smart_print(din1.deltam_tm[imr].ave_err())<<" + "<<
	  smart_print(din2.deltam_tm[imr].ave_err())<<" ]/2.0"<<endl;
      }
  
  for(size_t im1=0;im1<dout.nm;im1++)
    for(size_t im2=0;im2<dout.nm;im2++)
      {
	size_t i=dout.im_im_ind({im1,im2});
	cout<<"meson_mass[m1="<<im1<<",m2="<<im2<<"]: "<<
	  smart_print(dout.meson_mass[i].ave_err())<<" = ["<<
	  smart_print(din1.meson_mass[i].ave_err())<<" + "<<
	  smart_print(din2.meson_mass[i].ave_err())<<"] /2.0"<<endl;
      }
  cout<<"meson_mass_sea: "<<
    dout.meson_mass_sea.ave_err()<<" = "<<din1.meson_mass_sea.ave_err()<<" + "<<din2.meson_mass_sea.ave_err()<<endl;
  
  //remove from the list
  for(auto in : {in1,in2})
    data_erase(in);
  
  //add to the list
  pars::ens.push_back(out_name);
  data(out_name,PRESENCE_NOT_NEEDED)=temp;
  data(out_name,PRESENCE_NOT_NEEDED).dir_path=out_name;
}

void ratio_Z_minus_one(const string out,const string in1,const string in2)
{
  needs_to_read_Z();
  
  invalidate_ingredients();
  
  assert_compatible(in1,in2);
  
  cout<<"Taking ratio minus one of "<<in1<<" and "<<in2<<" into "<<out<<endl;
  
  perens_t temp=data(in1,ASSERT_PRESENT);
  
  for(auto &p : temp.get_all_Ztasks({&data(in2,ASSERT_PRESENT)}))
    {
      cout<<" "<<p.tag<<endl;
      
      djvec_t &out=*p.out;
      const djvec_t &in=*p.in.front();
      
      out/=in;
      out-=1.0;
    }
  
  //remove from the list
  for(auto in : {in1,in2})
    data_erase(in);
  
  //add to the list
  pars::ens.push_back(out);
  data(out,PRESENCE_NOT_NEEDED)=temp;
  data(out,PRESENCE_NOT_NEEDED).dir_path=out;
}

void sea_chir_extrap(const string out_name,const vector<string> &ens_list)
{
  needs_to_read_Z();
  
  invalidate_ingredients();
  
  if(ens_list.size()<2) CRASH("Need at least 2 ensembles, %zu passed",ens_list.size());
  cout<<"Chirally extrapolating "<<out_name<<" in the sea mass, list of ensembles:"<<endl;
  
  //take x and ensembles
  vector<double> x;
  vector<const perens_t*> in;
  for(auto &en_name : ens_list)
    {
      //check and take the data, add it to the list
      assert_compatible(ens_list.front(),en_name);
      const perens_t& en=data(en_name,ASSERT_PRESENT);
      in.push_back(&en);
      
      //compute the x
      double t;
      if(pars::chir_extr_method==chir_extr::MQUARK) t=en.am[en.im_sea];
      else                                          t=sqr(en.meson_mass_sea.ave());
      x.push_back(t);
      
      //print the name
      cout<<" "<<en_name<<endl;
    }
  
  auto xminmax=minmax_element(x.begin(),x.end());
  double xmin=*xminmax.first;
  xmin=0.0;
  double xmax=*xminmax.second*1.1;
  
  //prepare output
  perens_t out=*in.front();
  out.meson_mass_sea=0.0;
  out.dir_path=out_name;
  
  for(auto &v : out.get_all_Ztasks(in))
    {
      cout<<" "<<v.tag<<endl;
      
      for(size_t icombo=0;icombo<v.out->size();icombo++)
	{
	  djvec_t y(ens_list.size());
	  for(size_t iens=0;iens<v.in.size();iens++) y[iens]=(*(v.in[iens]))[icombo];
	  
	  string plot_path="";
	  if(icombo==100) plot_path=out_name+"/plots/sea_chirextr_"+v.tag+"_"+v.ind.descr(icombo)+".xmg";
	  djvec_t coeffs=poly_fit(x,y,1);
	  if(std::isnan(coeffs[0][0])) coeffs=0.0;
	  (*v.out)[icombo]=coeffs[0];
	  
	  if(plot_path!="")
	    {
	      grace_file_t plot(plot_path);
	      write_fit_plot(plot,xmin,xmax,bind(poly_eval<djvec_t>,coeffs,_1),x,y);
	      plot.set_title(v.tag+", "+v.ind.descr(icombo));
	      plot.write_ave_err(0,coeffs[0].ave_err());
	    }
	}
    }
  
  //remove from the list
  for(auto in : ens_list)
    data_erase(in);
  
  //add to the list
  pars::ens.push_back(out_name);
  data(out_name,PRESENCE_NOT_NEEDED)=out;
  data(out_name,PRESENCE_NOT_NEEDED).dir_path=out_name;
}

auto assert_ens_present(const string &key)
{
  auto f=_data.find(key);
  
  if(f==_data.end())
    {
      cout<<"Available keys: "<<endl;
      for(auto d : _data) cout<<" "<<d.first<<endl;
      CRASH("Unable to find the key %s",key.c_str());
    }
  
  return f;
}

void add_ens(const string &name)
{
  freeze_pars();
  
  pars::ens.push_back(name);
  
  data(name,PRESENCE_NOT_NEEDED)
    .read_pars(name)
    .set_pars_for_scratch()
    .set_indices()
    .allocate();
}

void data_erase(const string &key)
{
  auto f=_data.find(key);
  auto e=find(pars::ens.begin(),pars::ens.end(),key);
  
  if(f!=_data.end() and e!=pars::ens.end())
    {
      _data.erase(key);
      pars::ens.erase(e);
    }
  else
    cout<<"Warning, key \""<<key<<"\" not present"<<endl;
}

perens_t& data(const string &key,const bool assert_present_flag)
{
  if(assert_present_flag)
    return assert_ens_present(key)->second;
  else
    return _data[key];
}

void list_ensembles()
{
  cout<<"Ensembles:"<<endl;
   for(auto &path : pars::ens)
     cout<<" "<<path<<endl;
}

void print_discr()
{
  for(auto &path : pars::ens) data(path,ASSERT_PRESENT).print_discr();
}

void compute_or_load_all_ingredients()
{
  cout<<"Going to rotate propagators: "<<(pars::twisted_run and pars::phys_basis)<<endl;
  
  for(auto &name : pars::ens)
    data(name,ASSERT_PRESENT)
      .read_or_compute_ingredients()
      .get_deltam()
      .get_mPCAC()
      .get_meson_mass();
  
  validate_ingredients();
}

void combined_sea_chir_extrap(const vector<comb_extr_t> &list)
{
  // needs_to_read_Z();
  
  // invalidate_ingredients();
  
  // const size_t ngroups=list.size();
  // if(ngroups<2) CRASH("Need at least 2 set of ensembles, %zu passed",ngroups);
  // djvec_t extr(ngroups);
  // djvec_t mslope(ngroups);
  // djvec_t mslope_a2dep(ngroups);
  
  // jack_fit_t jack_fit;
  
  // //define parameters
  // vector<size_t> iextr(ngroups);
  // vector<size_t> imslope(ngroups);
  // vector<size_t> imslope_a2dep(ngroups);
  // for(size_t igroup=0;igroup<ngroups;igroup++)
  //   {
  //     iextr[igroup]=jack_fit.add_fit_par(extr[igroup],"extr_"+to_string(igroup),0.0,0.1);
  //     imslope[igroup]=jack_fit.add_fit_par(mslope[igroup],"mslope_"+to_string(igroup),0.0,0.1);
  //     imslope_a2dep[igroup]=jack_fit.add_fit_par(mslope_a2dep[igroup],"mslope_a2dep"+to_string(igroup),0.0,0.1);
  //   }
  
  // //insert new
  
  // for(auto &p : temp.get_all_Ztasks({&data(in2,ASSERT_PRESENT)}))
  //   {
  //     cout<<" "<<p.tag<<endl;
  // get_al
  
  //   for(size_t idata=0;idata<ext_data.size();idata++)
  //   boot_fit.add_point(//numerical data
  // 		       [&ext_data,&pars,idata,apow,zpow]
  // 		       (const vector<double> &p,int iel) //dimension 2
  // 		       {return ext_data[idata].wfse[iel]*pow(pars.get_z(p,ext_data[idata].ib,iel),zpow)/pow(pars.get_a(p,ext_data[idata].ib,iel),apow);},
  // 		       //ansatz
  // 		       [idata,&pars,&ext_data,&cont_chir_ansatz]
  // 		       (const vector<double> &p,int iel)
  // 		       {
  // 			 size_t ib=ext_data[idata].ib;
  // 			 double ac=pars.get_a(p,ib,iel);
  // 			 double zc=pars.get_z(p,ib,iel);
  // 			 double ml=ext_data[idata].aml/ac/zc;
  // 			 double ms=ext_data[idata].ams/ac/zc;
  // 			 double Maux=ext_data[idata].aMaux[iel]/ac;
  // 			 double L=ext_data[idata].L;
  // 			 return cont_chir_ansatz(p,pars,ml,ms,Maux,ac,L);
  // 		       },
  // 		       //for covariance/error
  // 		       dboot_t(ext_data[idata].wfse*pow(pars.ori_z[ext_data[idata].ib],zpow)/pow(pars.ori_a[ext_data[idata].ib],apow)),1/*correlate*/);
  
  
  CRASH("Can be done only after p2 extrapolation, how we deal with that?");
}
