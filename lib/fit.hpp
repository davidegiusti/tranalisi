#ifndef _FIT_HPP
#define _FIT_HPP

#include <Eigen/Dense>
#include <functions.hpp>
#include <grace.hpp>
#include <meas_vec.hpp>
#include <minimizer.hpp>
#include <oper.hpp>

#ifndef EXTERN_FIT
 #define EXTERN_FIT extern
 #define INIT_TO(A)
#else
 #define INIT_TO(A) =A
#endif

using namespace std;
using namespace Eigen;

typedef Matrix<double,Dynamic,Dynamic> matr_t;

#define DO_NOT_CORRELATE -1

///////////////////////////////////////////////////////////////// fake fits /////////////////////////////////////////////////////

//! perform a fit to constant (uncorrelated)
template <class TV,class T=typename TV::base_type> T constant_fit(const TV &data,size_t xmin,size_t xmax,string path="")
{
  //fix max and min, check order
  //xmin=max(xmin,0ul);
  xmax=min(xmax,data.size()-1);
  check_ordered({xmin,xmax,data.size()});
  
  //result of the fit
  T res(init_nel(data[0]));
  
  //take weighted average
  double norm=0;
  for(size_t iel=xmin;iel<=xmax;iel++)
    {
      auto ele=data[iel];
      double err=data[iel].err();
      double weight=1/(err*err);
      if(!std::isnan(err) and err!=0)
        {
          res+=ele*weight;
          norm+=weight;
        }
    }
  
  //take simply average if error was zero
  if(norm==0)
    for(size_t iel=/*max(xmin,0ul)*/ xmin;iel<=min(xmax,data.size()-1);iel++)
      {
        norm++;
        res+=data[iel];
      }
  
  //normalize
  res/=norm;
  
  //write a plot if asked to
  if(path!="") write_constant_fit_plot(path,xmin,xmax,res,data);
  
  return res;
}

//! fit the mass and the matrix element
template <class TV,class T=typename TV::base_type> void two_pts_fit(T &Z2,T &M,const TV &corr,size_t TH,size_t tmin,size_t tmax,const string &path_mass="",const string &path_Z2="",int par=1)
{
  //fit to constant the effective mass
  M=constant_fit(effective_mass(corr,TH,par),tmin,tmax,path_mass);
  
  //prepare the reduced corr
  TV temp=corr;
  for(size_t t=0;t<=TH;t++) temp[t]/=two_pts_corr_fun(M*0+1,M,TH,t,par);
  Z2=constant_fit(temp,tmin,tmax,path_Z2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//! perform a simple fit using x, a function and data
template <class TV,class TS=typename TV::base_type>
class simple_ch2_t : public minimizer_fun_t
{
  //! type of function to be passed
  using fun_t=function<double(const vector<double> &p,double x)>;
  
  //! ascissas to be fitted
  vector<double> x;
  //! range to be used
  size_t xmin,xmax;
  //! y to be fitted
  TV data;
  //! function
  fun_t fun;
  //! error
  vector<double> err;
  //! covariance matrix
  vector<double> inv_cov_matr;
  //! element of the data-dstribution
  size_t &iel;
  
public:
  //! constructor
  simple_ch2_t(const vector<double> &x,size_t xmin,size_t xmax,const TV &data,fun_t fun,size_t &iel) :
    x(x),xmin(xmin),xmax(xmax),data(data),fun(fun),iel(iel)
  {
    check_ordered({xmin,xmax,data.size()});
    
    err.resize(data.size());
    for(size_t i=0;i<data.size();i++) err[i]=data[i].err();
  }
  
  //! compute the function
  double operator()(const vector<double> &p) const
  {
    double ch2=0;
    //cout<<"Range: "<<xmin<<" "<<xmax<<endl;
    for(size_t ix=xmin;ix<xmax;ix++)
      {
	double n=data[ix][iel];
	double t=fun(p,x[ix]);
	double e=err[ix];
	double contr=sqr((n-t)/e);
	ch2+=contr;
	//cout<<contr<<" = [("<<n<<"-f("<<x[ix]<<")="<<t<<")/"<<e<<"]^2]"<<endl;
      }
    return ch2;
  }
  
  double Up() const {return 1;}
};

//! perform a fit to the usual 2pts ansatz
template <class TV,class TS=typename TV::base_type> void two_pts_true_fit(TS &Z2,TS &M,const TV &corr,size_t TH,size_t tmin,size_t tmax,string path="",int par=1)
{
  //perform a preliminary fit
  two_pts_fit(Z2,M,corr,TH,tmin,tmax);
  
  //! fit a two point function
  size_t iel;
  simple_ch2_t<TV> two_pts_fit_obj(vector_up_to<double>(corr.size()),tmin,tmax,corr,two_pts_corr_fun_t(TH,par),iel);
  
  //parameters to fit
  minimizer_pars_t pars;
  pars.add("Z2",Z2[0],Z2.err());
  pars.add("M",M[0],M.err());
  
  minimizer_t minimizer(two_pts_fit_obj,pars);
  for(iel=0;iel<corr[0].size();iel++)
    {
      //minimize and print the result
      vector<double> pars=minimizer.minimize();
      Z2[iel]=pars[0];
      M[iel]=pars[1];
    }
  
  if(path!="") write_constant_fit_plot(path,tmin,tmax,M,effective_mass(corr,TH,par));
}

//! perform a fit using multiple x, functions and data
template <class TV,class TS=typename TV::base_type>
class multi_ch2_t : public minimizer_fun_t
{
  //! type of function to be passed
  using fun_t=function<double(const vector<double> &p,double x)>;
  using fun_shuf_t=function<vector<double>(const vector<double> &p,size_t icontr)>;
  
  //! individual contributions
  vector<simple_ch2_t<TV>> contrs;
  
  //! shuffle parameters to individual contribtuions
  fun_shuf_t fun_shuf;
public:
  //! constructor
  multi_ch2_t(const initializer_list<vector<double>> &xs,initializer_list<size_t> xmins,initializer_list<size_t> xmaxs,const initializer_list<TV> &datas,initializer_list<fun_t> funs,const fun_shuf_t &fun_shuf,size_t &iel) : fun_shuf(fun_shuf)
  {
    //init all subch2
    auto x=xs.begin();auto xmin=xmins.begin();auto xmax=xmaxs.begin();auto data=datas.begin();auto fun=funs.begin();
    while(x!=xs.end()) contrs.push_back(simple_ch2_t<TV>(*(x++),*(xmin++),*(xmax++),*(data++),*(fun++),iel));
  }
  
  //! compute the sum of all ch2
  double operator()(const vector<double> &p) const
  {
    double ch2=0;
      for(size_t icontr=0;icontr<contrs.size();icontr++)
	ch2+=contrs[icontr](fun_shuf(p,icontr));
    
    return ch2;
  }
  
  double Up() const {return 1;}
};

//////////////////////////////////////////////////////////// slope /////////////////////////////////////////////////////

//! perform a fit to determine the slope
template <class TV,class TS=typename TV::base_type> void two_pts_with_ins_ratio_fit(TS &Z2,TS &M,TS &DZ2_fr_Z2,TS &SL,const TV &corr,const TV &corr_ins,size_t TH,size_t tmin,size_t tmax,string path="",string path_ins="",int par=1)
{
  //perform a preliminary fit
  TV eff_mass=effective_mass(corr,TH,par);
  M=constant_fit(eff_mass,tmin,tmax,"/tmp/test_mass.xmg");
  TV eff_sq_coupling=effective_squared_coupling(corr,eff_mass,TH,par);
  Z2=constant_fit(eff_sq_coupling,tmin,tmax,"/tmp/test_sq_coupling.xmg");
  TV eff_slope=effective_slope(TV(corr_ins/corr),eff_mass,TH);
  SL=constant_fit(eff_slope,tmin,tmax,"/tmp/test_slope.xmg");
  TV eff_slope_offset=effective_squared_coupling_rel_corr(TV(corr_ins/corr),eff_mass,eff_slope,TH);
  DZ2_fr_Z2=constant_fit(eff_slope_offset,tmin,tmax,"/tmp/test_sq_coupling_rel_corr.xmg");
  
  if(SL.err()!=0.0 and DZ2_fr_Z2.ave()!=0.0)
    {
      //! fit for real
      size_t iel=0;
      auto x=vector_up_to<double>(corr.size());
      multi_ch2_t<TV> two_pts_fit_obj({x,x},{tmin,tmin},{tmax,tmax},{corr,corr_ins/corr},
				      {two_pts_corr_fun_t(TH,par),two_pts_corr_with_ins_fun_t(TH,par)},
				      [](const vector<double> &p,size_t icontr)
				      {
					switch(icontr)
					  {
					  case 0:return vector<double>({p[0],p[1]});break;
					  case 1:return vector<double>({p[1],p[2],p[3]});break;
					  default: CRASH("Unknown contr %zu",icontr);return p;
					  }
				      },iel);
      
      //parameters to fit
      minimizer_pars_t pars;
      pars.add("Z2",Z2[0],Z2.err());
      pars.add("M",M[0],M.err());
      pars.add("DZ2",DZ2_fr_Z2[0],DZ2_fr_Z2.err());
      pars.add("SL",SL[0],SL.err());
      
      minimizer_t minimizer(two_pts_fit_obj,pars);
      
      for(iel=0;iel<corr[0].size();iel++)
	{
	  //minimize and print the result
	  vector<double> par_min=minimizer.minimize();
	  M[iel]=par_min[1];
	  DZ2_fr_Z2[iel]=par_min[2];
	  SL[iel]=par_min[3];
	}
      
      //write plots
      if(path!="") write_constant_fit_plot(path,tmin,tmax,M,eff_mass);
      if(path_ins!="") write_fit_plot(path_ins,tmin,tmax,[M,DZ2_fr_Z2,SL,TH,par](double x)->TS
				      {return two_pts_corr_with_ins_ratio_fun(M,DZ2_fr_Z2,SL,TH,x,par);},TV(corr_ins/corr));
    }
  else two_pts_true_fit(Z2,M,corr,TH,tmin,tmax,path,par);
}

/////////////////////////////////////////////////////////////// multi x fit ///////////////////////////////////////////////////////////

//! hold a single element to be minimized
class distr_fit_data_t
{
public:
  using fun_t=function<double(const vector<double> &p,size_t iel)>;
  fun_t num;
  fun_t teo;
  double err;
  
  distr_fit_data_t(const fun_t &num,const fun_t &teo,double err) : num(num),teo(teo),err(err) {}
};

EXTERN_FIT bool distr_fit_debug INIT_TO(false);

//! functor to minimize
template <class TS> class distr_fit_FCN_t : public minimizer_fun_t
{
  //! covariance flag
  bool cov_flag;
  //! data to be fitted
  vector<distr_fit_data_t> data;
  //! inverse covariance
  vector<double> inv_cov;
  //! element of the data-dstribution
  size_t &iel;
  
public:
  //! constructor
  distr_fit_FCN_t(const vector<distr_fit_data_t> &data,size_t &iel) : cov_flag(false),data(data),iel(iel){}
  
  //! add the covariance matrix
  bool add_cov(const vector<TS> &pro_cov,const vector<int> &cov_block)
  {
    cov_flag=true;
    const size_t &n=pro_cov.size();
    
    //fill the covariance matrix
    matr_t cov_matr(n,n);
    for(size_t i=0;i<n;i++)
      for(size_t j=0;j<n;j++)
	if(i==j or (cov_block[i]==cov_block[j] and cov_block[i]>=0))
	  cov_matr(i,j)=cov(pro_cov[i],pro_cov[j]);
    
    //compute eigenvalues
    SelfAdjointEigenSolver<matr_t> es;
    auto e=es.compute(cov_matr);
    auto ei=e.eigenvalues();
    auto ev=e.eigenvectors();
    
    //get the epsilon
    double eps=0.0;//-ei(0);
    if(eps<0) eps=0;
    cout<<"Epsilon: "<<eps<<endl;
    // //debug
    // eps=0;
    
    //fill the inverse
    inv_cov.resize(n*n);
    for(size_t i=0;i<n;i++)
      for(size_t j=0;j<n;j++)
	{
	  inv_cov[i*n+j]=0;
	  for(size_t k=0;k<n;k++) inv_cov[i*n+j]+=ev(i,k)*(1/(ei(k)+eps))*ev(j,k);
      }
    
    // //test inverse
    // for(size_t i=0;i<n;i++)
    //   for(size_t j=0;j<n;j++)
    // 	{
    // 	  double a=0;
    // 	  for(size_t k=0;k<n;k++) a+=cov_matr(i,k)*inv_cov[k*n+j];
    // 	  cout<<" wkehnfwjihoefwfeih "<<i<<" "<<j<<" "<<a<<endl;
    // 	}
    
    return cov_flag;
  }
  
  //! compute the function
  double operator()(const vector<double> &p) const
  {
    double ch2=0;
     if(cov_flag)
        for(size_t ix=0;ix<data.size();ix++)
	  for(size_t iy=0;iy<data.size();iy++)
	    {
      	    double nx=data[ix].num(p,iel);
      	    double tx=data[ix].teo(p,iel);
      	    double ny=data[iy].num(p,iel);
      	    double ty=data[iy].teo(p,iel);
      	    double contr=(nx-tx)*inv_cov[ix*data.size()+iy]*(ny-ty);
      	    ch2+=contr;
	    //if(distr_fit_debug) cout<<contr<<" = [("<<n<<"-f("<<ix<<")="<<t<<")/"<<e<<"]^2]"<<endl;
      	}
    else
      for(size_t ix=0;ix<data.size();ix++)
	{
	  double n=data[ix].num(p,iel);
	  double t=data[ix].teo(p,iel);
	  double e=data[ix].err;
	  double contr=sqr((n-t)/e);
	  ch2+=contr;
	  if(distr_fit_debug) cout<<contr<<" = [("<<n<<"-f("<<ix<<")="<<t<<")/"<<e<<"]^2]"<<endl;
	}
     
     return ch2;
  }
  
  double Up() const {return 1;}
};

//! class to fit
template <class TV,class TS=typename TV::base_type> class distr_fit_t
{
  vector<TS> pro_cov;
  vector<int> cov_block_label; //<! contains the block index for which to fill the covariance matrix
  
  vector<distr_fit_data_t> data;
  minimizer_pars_t pars;
  vector<TS*> out_pars;
public:
  
  //! add a point
  void add_point(const distr_fit_data_t::fun_t &num,const distr_fit_data_t::fun_t &teo,const TS &pro_err,int block_label)
  {
    data.push_back(distr_fit_data_t(num,teo,pro_err.err()));
    pro_cov.push_back(pro_err);
    cov_block_label.push_back(block_label);
  }
  
  //! add a point without error distribution
  void add_point(const distr_fit_data_t::fun_t &num,const distr_fit_data_t::fun_t &teo,const double &err)
  {
    data.push_back(distr_fit_data_t(num,teo,err));
    pro_cov.push_back(TS(gauss_filler_t(0,err,234234)));
    cov_block_label.push_back(DO_NOT_CORRELATE);
  }
  
  //! add a simple point
  void add_point(const TS &num,const distr_fit_data_t::fun_t &teo)
  {add_point([num](const vector<double> &p,int iel){return num[iel];},teo,num.err());}
  
  //! add a parameter to the fit
  size_t add_fit_par(TS &out_par,const string &name,double ans,double err)
  {
    size_t ipar=pars.size();
    pars.add(name.c_str(),ans,err);
    out_pars.push_back(&out_par);
    return ipar;
  }
  size_t add_fit_par(TS &out_par,const string &name,const ave_err_t &ae)
  {return add_fit_par(out_par,name,ae.ave(),ae.err());}
  
  //! add a parameter that gets self-fitted (useful to propagate erorr on x)
  size_t add_self_fitted_point(TS &out_par,string name,const TS &point,int block_label)
  {
    size_t ipar=add_fit_par(out_par,name.c_str(),point[0],point.err());
    add_point(//numerical data
	      [point]
	      (vector<double> p,int iel)
	      {return point[iel];},
	      //ansatz
	      [ipar]
	      (vector<double> p,int iel)
	      {return p[ipar];},
	      //error
	      point,block_label);
    return ipar;
  }
  
  //! same, but with ave and error only
  size_t add_self_fitted_point(TS &out_par,string name,const ave_err_t &ae)
  {
    size_t ipar=add_fit_par(out_par,name.c_str(),ae.ave(),ae.err());
    add_point(//numerical data
	      [ae]
	      (vector<double> p,int iel)
	      {return ae.ave();},
	      //ansatz
	      [ipar]
	      (vector<double> p,int iel)
	      {return p[ipar];},
	      //error
	      ae.err());
    return ipar;
  }
  
  //! perform the fit
  void fit(bool cov_flag=false)
  {
    size_t npars=pars.size();
    size_t nfree_pars=pars.nfree_pars();
    size_t idistr=0;
    distr_fit_FCN_t<TS> distr_fit_FCN(data,idistr);
    if(cov_flag) distr_fit_FCN.add_cov(pro_cov,cov_block_label);
    
    //define minimizator
    minimizer_t minimizer(distr_fit_FCN,pars);
    
    TS ch2;
    for(idistr=0;idistr<out_pars[0]->size();idistr++)
      {
	// distr_fit_debug=false;
	// if(idistr==out_pars[0]->size()-1) distr_fit_debug=true;
	
	//minimize and print the result
	
	//cout<<"----------------------- "<<idistr<<" ----------------------- "<<endl;
	vector<double> pars=minimizer.minimize();
	ch2[idistr]=minimizer.eval(pars);
	
	for(size_t ipar=0;ipar<npars;ipar++) (*(out_pars[ipar]))[idistr]=pars[ipar];
	// distr_fit_debug=false;
    }
    
    //write ch2
    cout<<"Ch2: "<<ch2.ave_err()<<" / "<<data.size()-nfree_pars<<endl;
  }
  
  //! fix a single parameter
  void fix_par(size_t ipar) {pars.fix(ipar);}
  
  //! fix a single parameter to a given value
  void fix_par_to(size_t ipar,double val) {pars.fix_to(ipar,val);}
};
using boot_fit_t=distr_fit_t<dbvec_t>;
using jack_fit_t=distr_fit_t<djvec_t>;

class cont_chir_fit_data_t_pol
{
public:
  double aml;
  size_t ib;
  dboot_t y;
  cont_chir_fit_data_t_pol(double aml,size_t ib,dboot_t y) : aml(aml),ib(ib),y(y) {}
};

//! perform a fit to the continuum and chiral
void cont_chir_fit_pol(const dbvec_t &a,const dbvec_t &z,const vector<cont_chir_fit_data_t_pol> &ext_data,const dboot_t &ml_phys,const string &path,dboot_t &output);

//! solve a linear system
template <class TV> TV lin_solve(vector<double> A,TV b)
{
  if(A.size()!=sqr(b.size())) CRASH("A has different size from b^2, size A = %zu, size b = %zu",A.size(),b.size());
  
  int d=b.size();
  
  // for(int i=0;i<d;i++)
  //   {
  //     for(int j=0;j<d;j++) cout<<A[i*d+j]<<" ";
  //     cout<<endl;
  //   }
  // for(int i=0;i<d;i++) cout<<b[i].ave_err()<<endl;
  
  for(int i=0;i<d;i++)
    {
      double C=A[i*d+i];
      for(int j=i;j<d;j++) A[i*d+j]/=C;
      b[i]/=C;
      
      for(int k=i+1;k<d;k++)
        {
          double C=A[k*d+i];
          for(int j=i;j<d;j++) A[k*d+j]-=A[i*d+j]*C;
          b[k]-=C*b[i];
        }
    }
  
  TV x(d);
  for(int k=d-1;k>=0;k--)
    {
      typename TV::base_type S;
      S=0.0;
      
      for(int i=k+1;i<d;i++) S+=A[k*d+i]*x[i];
      x[k]=b[k]-S;
    }
  
  return x;
}

//! perform polynomial fit
template <class TV> TV poly_fit(const vector<double> &x,const TV &y,int d,double xmin=-1e300,double xmax=1e300)
{
  if(x.size()!=y.size()) CRASH("x and y have different sizes, %zu %zu",x.size(),y.size());
  
  cout<<x<<endl;
  cout<<y.ave_err()<<endl;
  
  vector <double> Al(2*d+1,0.0);
  TV c(d+1);
  c=0.0;
  
  for(int p=0;p<(int)y.size();p++)
    if(x[p]<=xmax and x[p]>=xmin)
      {
        //calculate the weight
        double w=pow(y[p].err(),-2);
	cout<<w<<endl;
        //compute Al and c
        for(int f=0;f<=2*d;f++)
          {
            Al[f]+=w;
            if(f<=d) c[f]+=y[p]*w;
            w*=x[p];
          }
      }
  
  vector<double> A((d+1)*(d+1));
  for(int i=0;i<=d;i++)
    for(int j=0;j<=d;j++)
      A[i*(d+1)+j]=Al[i+j];
  
  return lin_solve(A,c);
}
//! perform a polynomial fit assuming x are ranging from 0 to y.size()
template <class TV> TV poly_fit(const TV &y,int d)
{return poly_fit(vector_up_to<double>(y.size()),y,d,-0.5,y.size()+0.5);}

//! perform the polynomial fit and integrate
template <class TV> typename TV::base_type poly_integrate(const vector<double> &x,const TV &y)
{
  int d=y.size()-1;
  TV pars=poly_fit(x,y,d);
  typename TV::base_type out;
  
  double xmin=x[0],xmax=x[x.size()-1];
  for(int i=0;i<d;i++)
    out+=(pow(xmax,i+1)-pow(xmin,i+1))*pars[i]/(i+1);
  
  return out;
}
template <class TV> typename TV::base_type poly_integrate(const TV &y)
{return poly_integrate(vector_up_to<double>(y.size()),y);}

//! compute the value of the  polynomial in the point
template <class TV,class TS=typename TV::base_type> TS poly_eval(const TV &pars,double x)
{
  TS t=pars[0];
  double R=x;
  for(int ipow=1;ipow<(int)pars.size();ipow++)
    {
      t+=pars[ipow]*R;
      R*=x;
    }
  return t;
}

//! compute the ch2 of a polynomial fit
template <class TV,class TS=typename TV::base_type> TS chi2_poly_fit(const vector<double> &x,const TV &y,int d,double xmin,double xmax,const TV &pars)
{
  TS ch2;
  int ndof=-pars.size();
  
  ch2=0.0;
  for(int ip=0;ip<(int)y.size();ip++)
    if(x[ip]<=xmax and x[ip]>=xmin)
      {
	ch2+=sqr(TS((poly_eval(pars,x[ip])-y[ip])/y[ip].err()));
        ndof++;
      }
  ch2/=max(1,ndof);
  
  return ch2;
}
template <class TV,class TS=typename TV::base_type> TS chi2_poly_fit(const TV &y,int d,const TV &pars)
{return chi2_poly_fit(vector_up_to<double>(y.size()+1),y,d,-0.5,y.size()+0.5,pars);}

#undef EXTERN_FIT
#undef INIT_TO

#endif
