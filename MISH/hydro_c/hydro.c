#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "hydro.h"
#include "float.h"

hydro_args *Ha;
hydro_prob *Hp;
double *q;
double *qr, *ql;
double *flx;

double slope(double *q,int ind);

//Utility function to get wall runtime
double getNow(){
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (double)tv.tv_sec+1e-6*(double)tv.tv_usec;
}

//Utility function for debugging, prints entire state var array
//in relatively readable format
void printArray(char* label,double *arr, int nvar, int nx, int ny){
  int nV,i,j;
  printf("Array %s\n",label);
  for(nV=0;nV<nvar;nV++){
    printf("Variable %d:\n",nV);
    printf("%3s","");
    for(i=0;i<nx;i++){
      printf("|i%8di",i);
    }
    printf("|\n");
    for(j=0;j<ny;j++){
      printf("%3d",j);
      for(i=0;i<nx;i++){
	printf("|%10g",arr[i+nx*(j+ny*nV)]);
      }
      printf("|\n");
    }
  }
}

//Function to calculate timestep
double calcDT(double *mesh){
  int i;
  double denom, max_denom;
  double r,vx,vy,eint,p;
  double c,cx,cy;
  double smallp;

  //Initialize constants
  c=0.0;
  max_denom=Ha->smallc;

  smallp=Ha->smallc*Ha->smallc/Hp->gamma;
  for (i=0; i<Hp->nx*Hp->ny; i++){
    //Get primitive vars
    r   =MAX(mesh[i+Hp->nx*Hp->ny*VARRHO],Ha->smallr);
    vx  =    mesh[i+Hp->nx*Hp->ny*VARVX ]/r;
    vy  =    mesh[i+Hp->nx*Hp->ny*VARVY ]/r;
    eint=    mesh[i+Hp->nx*Hp->ny*VARPR ]-0.5*r*(vx*vx+vy*vy);
    p   =MAX((Hp->gamma-1.0)*eint,r*smallp);
    
    //get sound speed
    c=sqrt((Hp->gamma*p/r));

    //Get max expected velocity in each dim
    cx=(c+fabs(vx))/Hp->dx;
    cy=(c+fabs(vy))/Hp->dy;
    denom=cx+cy;
    if(max_denom<denom)max_denom=denom;
  }
  return 0.5/max_denom;
}

//Convert conserved to primitive for x pass
void toPrimX(double *q, double *mesh){
  int i;
  int xI, yI;
  double r,vx,vy,eint,p;
  double smallp;

  smallp=Ha->smallc*Ha->smallc/Hp->gamma;
  for(i=0;i<Hp->ny*Hp->nx;i++){
    xI=i%Hp->nx;
    yI=i/Hp->nx;
    r   =MAX(mesh[i+Hp->nx*Hp->ny*VARRHO],Ha->smallr);
    vx  =mesh[i+Hp->nx*Hp->ny*VARVX ]/r;
    vy  =mesh[i+Hp->nx*Hp->ny*VARVY ]/r;
    eint=mesh[i+Hp->nx*Hp->ny*VARPR ]-0.5*r*(vx*vx+vy*vy);
    p   =MAX((Hp->gamma-1)*r*eint,smallp);
    q[xI+2+(Hp->nx+4)*(yI+Hp->ny*VARRHO)]=r;
    q[xI+2+(Hp->nx+4)*(yI+Hp->ny*VARVX )]=vx;
    q[xI+2+(Hp->nx+4)*(yI+Hp->ny*VARVY )]=vy;
    q[xI+2+(Hp->nx+4)*(yI+Hp->ny*VARPR )]=p;
  }
}

//Convert conserved to primitive for y pass
void toPrimY(double *q, double *mesh){
  int i;
  int xI, yI;
  double r,vx,vy,eint,p;
  double smallp;

  smallp=Ha->smallc*Ha->smallc/Hp->gamma;
  for(i=0;i<Hp->ny*Hp->nx;i++){
    xI=i%Hp->nx;
    yI=i/Hp->nx;
    r   =MAX(mesh[i+Hp->nx*Hp->ny*VARRHO],Ha->smallr);
    vx  =mesh[i+Hp->nx*Hp->ny*VARVX ]/r;
    vy  =mesh[i+Hp->nx*Hp->ny*VARVY ]/r;
    eint=mesh[i+Hp->nx*Hp->ny*VARPR ]-0.5*r*(vx*vx+vy*vy);
    p   =MAX((Hp->gamma-1)*r*eint,smallp);
    q[yI+2+(Hp->ny+4)*(xI+Hp->nx*VARRHO)]=r;
    q[yI+2+(Hp->ny+4)*(xI+Hp->nx*VARVX )]=vy;
    q[yI+2+(Hp->ny+4)*(xI+Hp->nx*VARVY )]=vx;
    q[yI+2+(Hp->ny+4)*(xI+Hp->nx*VARPR )]=p;
  }
}

//Set boundary conditions on pass variable
void setBndCnd(double* q, int cndL, int cndH, int np, int nt){
  int i;
  int pI,tI;
  int wInd, rInd;


  //printf("Running bnd cnds for %dx%d prims\n",np,nt);
  for(i=0;i<2*nt;i++){
    pI=i%2;
    tI=i/2;
    wInd=pI+(np+4)*tI;
    rInd=3-pI+(np+4)*tI;
    //printf("BND: %d refs %d\n",wInd,rInd);
    if(cndL==BND_REFL){
      q[wInd+(np+4)*nt*VARRHO]= q[rInd+(np+4)*nt*VARRHO];
      q[wInd+(np+4)*nt*VARVX ]=-q[rInd+(np+4)*nt*VARVX ];
      q[wInd+(np+4)*nt*VARVY ]= q[rInd+(np+4)*nt*VARVY ];
      q[wInd+(np+4)*nt*VARPR ]= q[rInd+(np+4)*nt*VARPR ];
    }else if(cndL==BND_PERM){
      q[wInd+(np+4)*nt*VARRHO]= q[rInd+(np+4)*nt*VARRHO];
      q[wInd+(np+4)*nt*VARVX ]= q[rInd+(np+4)*nt*VARVX ];
      q[wInd+(np+4)*nt*VARVY ]= q[rInd+(np+4)*nt*VARVY ];
      q[wInd+(np+4)*nt*VARPR ]= q[rInd+(np+4)*nt*VARPR ];
    }
    wInd=np+2+pI+(np+4)*tI;
    rInd=np+1-pI+(np+4)*tI;
    //printf("BND: %d refs %d\n",wInd,rInd);
    if(cndH==BND_REFL){
      q[wInd+(np+4)*nt*VARRHO]= q[rInd+(np+4)*nt*VARRHO];
      q[wInd+(np+4)*nt*VARVX ]=-q[rInd+(np+4)*nt*VARVX ];
      q[wInd+(np+4)*nt*VARVY ]= q[rInd+(np+4)*nt*VARVY ];
      q[wInd+(np+4)*nt*VARPR ]= q[rInd+(np+4)*nt*VARPR ];
    }else if(cndH==BND_PERM){
      q[wInd+(np+4)*nt*VARRHO]= q[rInd+(np+4)*nt*VARRHO];
      q[wInd+(np+4)*nt*VARVX ]= q[rInd+(np+4)*nt*VARVX ];
      q[wInd+(np+4)*nt*VARVY ]= q[rInd+(np+4)*nt*VARVY ];
      q[wInd+(np+4)*nt*VARPR ]= q[rInd+(np+4)*nt*VARPR ];
    }
  }
}

//Calculate ql and qr from q
void trace(double *ql, double *qr, double *q, double dtdx, int np, int nt){
  int lI;
  int i,j;
  double  r, u, v1, p, a;
  double dr,du,dv1,dp,da;
  double cc,csq;
  double alpham,alphap,alphazr;
  double spplus,spzerol,spzeror,spminus;
  double ap,am,azr,azv1,acmp;

  for(lI=0;lI<(np+2)*nt;lI++){
    i=lI%(np+2);
    j=lI/(np+2);
    //Get local state vars
    r =q[i+1+(np+4)*(j+nt*VARRHO)];
    u =q[i+1+(np+4)*(j+nt*VARVX )];
    v1=q[i+1+(np+4)*(j+nt*VARVY )];
    p =q[i+1+(np+4)*(j+nt*VARPR )];
    
    csq=Hp->gamma*p/r;
    cc=sqrt(csq);
    
    //Calculate slopes
    dr =slope(q,i+1+(np+4)*(j+nt*VARRHO));
    du =slope(q,i+1+(np+4)*(j+nt*VARVX ));
    dv1=slope(q,i+1+(np+4)*(j+nt*VARVY ));
    dp =slope(q,i+1+(np+4)*(j+nt*VARPR ));
    
    alpham = 0.5*(dp/(r*cc)-du)*r/cc;
    alphap = 0.5*(dp/(r*cc)+du)*r/cc;
    alphazr= dr-dp/csq;
    
    //right
    spminus=((u-cc)>=0.0)?0.0:(u-cc)*dtdx+1.0;
    spzeror=((u   )>=0.0)?0.0:(u   )*dtdx+1.0;
    spplus =((u+cc)>=0.0)?0.0:(u+cc)*dtdx+1.0;
    ap  =-0.5*spplus *alphap ;
    am  =-0.5*spminus*alpham ;
    azr =-0.5*spzeror*alphazr;
    azv1=-0.5*spzeror*dv1;
    qr[i+(np+2)*(j+nt*VARRHO)]=r +(ap+am+azr);
    qr[i+(np+2)*(j+nt*VARVX )]=u +(ap-am    )*cc/r;
    qr[i+(np+2)*(j+nt*VARVY )]=v1+(azv1     );
    qr[i+(np+2)*(j+nt*VARPR )]=p +(ap+am    )*csq;
    
    //left
    spminus=((u-cc)<=0.0)?0.0:(u-cc)*dtdx-1.0;
    spzerol=((u   )<=0.0)?0.0:(u   )*dtdx-1.0;
    spplus =((u+cc)<=0.0)?0.0:(u+cc)*dtdx-1.0;
    ap  =-0.5*spplus *alphap ;
    am  =-0.5*spminus*alpham ;
    azr =-0.5*spzerol*alphazr;
    azv1=-0.5*spzerol*dv1;
    ql[i+(np+2)*(j+nt*VARRHO)]=r +(ap+am+azr);
    ql[i+(np+2)*(j+nt*VARVX )]=u +(ap-am    )*cc/r;
    ql[i+(np+2)*(j+nt*VARVY )]=v1+(azv1     );
    ql[i+(np+2)*(j+nt*VARPR )]=p +(ap+am    )*csq;
  }
}

//Approximates slope at a given point in the mesh
// Uses the consistent structure of the pass state
// arrays to limit to single index requirement
double slope(double *q,int ind){
  double dlft, drgt, dcen, dsgn, dlim;
  dlft=q[ind  ]-q[ind-1];
  drgt=q[ind+1]-q[ind  ];
  dcen=0.5*(dlft+drgt);
  //Get direction of slope
  if(dcen>=0)dsgn=1.0;
  else dsgn=-1.0;
  //If area looks like an extrema, limit slope to 0
  if(dlft*drgt<=0)dlim=0.0;
  else dlim=MIN(fabs(dlft),fabs(drgt));
  return dsgn*MIN(dlim,fabs(dcen));
}

void riemann(double *flx, double *qxm, double *qxp, int np, int nt){
  int lI, i,j,n;
  double smallp, smallpp;
  double gmma6, gra, entho;
  double qgdnvR,qgdnvVX,qgdnvVY,qgdnvP;
  double rl,vxl,vyl,pl,cl,wl,ql,vsl;
  double rr,vxr,vyr,pr,cr,wr,qr,vsr;
  double ro,vxo,po,wo,co;
  double rx,vxx,px,wx,cx;
  double sgnm, scr, frac;
  double spout,spin,ushk;
  double ekin,etot,delp;

  smallp=Ha->smallc*Ha->smallc/Hp->gamma;
  smallpp=Ha->smallr*smallp;
  gmma6=(Hp->gamma+1.0)/(2.0*Hp->gamma);
  gra=(Hp->gamma-1.0)/(1.0*Hp->gamma);
  entho=1.0/(Hp->gamma-1.0);
  for(lI=0;lI<(np+1)*nt;lI++){
    i=lI%(np+1);
    j=lI/(np+1);
    
    //Get state vars on either side of interface
    rl =MAX(qxm[i  +(np+2)*(j+nt*VARRHO)],Ha->smallr);
    vxl=    qxm[i  +(np+2)*(j+nt*VARVX )];
    vyl=    qxm[i  +(np+2)*(j+nt*VARVY )];
    pl =MAX(qxm[i  +(np+2)*(j+nt*VARPR )],rl*smallp);

    rr =MAX(qxp[i+1+(np+2)*(j+nt*VARRHO)],Ha->smallr);
    vxr=    qxp[i+1+(np+2)*(j+nt*VARVX )];
    vyr=    qxp[i+1+(np+2)*(j+nt*VARVY )];
    pr =MAX(qxp[i+1+(np+2)*(j+nt*VARPR )],rr*smallp);
    
    cl=Hp->gamma*pl*rl;
    cr=Hp->gamma*pr*rr;
    
    wl=sqrt(cl);
    wr=sqrt(cr);
    
    //Actual riemann solver - newton raphson iterations
    px=((wr*pl+wl*pr)+wl*wr*(vxl-vxr))/(wl+wr);
    px=MAX(px,0.0);
    for(n=0;n<Ha->niter_riemann;n++){
      wl=sqrt(cl*(1.0+gmma6*(px-pl)/pl));
      wr=sqrt(cr*(1.0+gmma6*(px-pr)/pr));
      ql=2.0*wl*wl*wl/(wl*wl+cl);
      qr=2.0*wr*wr*wr/(wr*wr+cr);
      vsl=vxl-(px-pl)/wl;
      vsr=vxr+(px-pr)/wr;
      delp=qr*ql/(qr+ql)*(vsl-vsr);
      delp=MAX(delp,-px+smallp);
      px+=delp;
      vxo=fabs(delp/(px+smallpp));
      if(vxo<1.0e-6)break;
    }
    wl=sqrt(cl*(1.0+gmma6*(px-pl)/pl));
    wr=sqrt(cr*(1.0+gmma6*(px-pr)/pr));
    ql=2.0*wl*wl*wl/(wl*wl+cl);
    qr=2.0*wr*wr*wr/(wr*wr+cr);
    vxx=((vxl-(px-pl)/wl)*ql+
         (vxr+(px-pr)/wr)*qr)/(ql+qr);
    if(vxx>=0.0){
      sgnm=1.0;
      ro = rl;
      vxo=vxl;
      po = pl;
      wo = wl;
      qgdnvVY=vyl;
    }else{
      sgnm=-1.0;
      ro = rr;
      vxo=vxr;
      po = pr;
      wo = wr;
      qgdnvVY=vyr;
    }
    co=MAX(Ha->smallc,sqrt(fabs(Hp->gamma*po/ro)));
    
    rx=MAX(Ha->smallr,ro/(1.0+ro*(po-px)/(wo*wo)));
    
    cx=MAX(Ha->smallc,sqrt(fabs(Hp->gamma*px/rx)));
  
    spout=co   -sgnm*vxo;
    spin =cx   -sgnm*vxx;
    ushk =wo/ro-sgnm*vxo;
    
    if(spout<spin){
      spin=ushk;
      spout=ushk;
    }

    scr=MAX(spout-spin,Ha->smallc+fabs(spout+spin));

    frac=0.5*(1.0+(spout+spin)/scr);
    frac=MAX(0.0,MIN(1.0,frac));
    qgdnvR =frac* rx+(1.0-frac)* ro;
    qgdnvVX=frac*vxx+(1.0-frac)*vxo;
    qgdnvP =frac* px+(1.0-frac)* po;
    if(spout<0.0){
      qgdnvR =ro;
      qgdnvVX=vxo;
      qgdnvP =po;
    }
    if(spin>0.0){
      qgdnvR =rx;
      qgdnvVX=vxx;
      qgdnvP =px;
    }

    //Calculate fluxes
    flx[i+(np+1)*(j+nt*VARRHO)]=qgdnvR*qgdnvVX;
    flx[i+(np+1)*(j+nt*VARVX )]=qgdnvR*qgdnvVX*qgdnvVX+qgdnvP;
    flx[i+(np+1)*(j+nt*VARVY )]=qgdnvR*qgdnvVX*qgdnvVY;
    ekin=0.5*qgdnvR*(qgdnvVX*qgdnvVX+qgdnvVY*qgdnvVY);
    etot=qgdnvP*entho+ekin;
    flx[i+(np+1)*(j+nt*VARPR )]=qgdnvVX*(etot+qgdnvP);
  }
}

//Add flux from x pass to conserved state vars
void addFluxX(double *mesh, double *flx, double dtdx, int np, int nt){
  int lI, i, j;

  for(lI=0;lI<np*nt;lI++){
    i=lI%np;
    j=lI/np;
    mesh[i+np*(j+nt*VARRHO)]+=dtdx*(flx[i  +(np+1)*(j+nt*VARRHO)]-
				    flx[i+1+(np+1)*(j+nt*VARRHO)]);
    mesh[i+np*(j+nt*VARVX )]+=dtdx*(flx[i  +(np+1)*(j+nt*VARVX )]-
				    flx[i+1+(np+1)*(j+nt*VARVX )]);
    mesh[i+np*(j+nt*VARVY )]+=dtdx*(flx[i  +(np+1)*(j+nt*VARVY )]-
				    flx[i+1+(np+1)*(j+nt*VARVY )]);
    mesh[i+np*(j+nt*VARPR )]+=dtdx*(flx[i  +(np+1)*(j+nt*VARPR )]-
				    flx[i+1+(np+1)*(j+nt*VARPR )]);
  }
}

//Add flux from y pass to conserved state vars
void addFluxY(double *mesh, double *flx, double dtdx, int np, int nt){
  int lI, i, j;

  for(lI=0;lI<np*nt;lI++){
    i=lI%np;
    j=lI/np;
    mesh[j+nt*(i+np*VARRHO)]+=dtdx*(flx[i  +(np+1)*(j+nt*VARRHO)]-
				    flx[i+1+(np+1)*(j+nt*VARRHO)]);
    mesh[j+nt*(i+np*VARVX )]+=dtdx*(flx[i  +(np+1)*(j+nt*VARVY )]-
				    flx[i+1+(np+1)*(j+nt*VARVY )]);
    mesh[j+nt*(i+np*VARVY )]+=dtdx*(flx[i  +(np+1)*(j+nt*VARVX )]-
				    flx[i+1+(np+1)*(j+nt*VARVX )]);
    mesh[j+nt*(i+np*VARPR )]+=dtdx*(flx[i  +(np+1)*(j+nt*VARPR )]-
				    flx[i+1+(np+1)*(j+nt*VARPR )]);
  }
}

//Utility function to sum a state variable over the entire mesh
double sumArray(double *mesh, int var, int nx, int ny){
  int i;
  double sum, corr;
  double nsum, c_nxt;

  sum=0.0;
  corr=0.0;

  for(i=0;i<nx*ny;i++){
    c_nxt=mesh[i+nx*ny*var];
    nsum=sum+c_nxt;
    corr=(nsum-sum)-c_nxt;
    sum=nsum;
  }
  return sum+corr;
}

//Convenience fuction to run pass of either dim
void runPass(double *mesh, double dt, int n, int dir){
  int bndL,bndH;
  int np,nt;
  double dxp,dxt;
  char dirCh, outfile[30];

  //Set relevant reference values for direction
  if(dir==0){
    //x-dir
    np=Hp->nx;
    nt=Hp->ny;
    bndL=Hp->bndL;
    bndH=Hp->bndR;
    dxp=Hp->dx;
    dxt=Hp->dy;
    dirCh='x';
    toPrimX(q,mesh);
  }else{
    //y-dir
    np=Hp->ny;
    nt=Hp->nx;
    bndL=Hp->bndU;
    bndH=Hp->bndD;
    dxp=Hp->dy;
    dxt=Hp->dx;
    dirCh='y';
    toPrimY(q,mesh);
  }
  setBndCnd(q,bndL,bndH,np,nt);
  trace(ql,qr,q,dt/dxp,np,nt);
  riemann(flx,ql,qr,np,nt);
  //Add calculated flux to state var array
  if(dir==0){
    addFluxX(mesh,flx,dt/dxp,np,nt);
  }else{
    addFluxY(mesh,flx,dt/dxp,np,nt);
  }
}

//Comptutational engine function to handle run
void engine(double *mesh, hydro_prob *Hyp, hydro_args *Hya){
  int n, i,j;
  int bndL;
  int bndH;
  double dt;
  double cTime, nxttout;

  double volCell;
  double oTM,oTE;
  double TM,TE;
  int M_exp, E_exp;
  double M_prec, E_prec;

  char outfile[30];

  size_t primSize, qSize, flxSize;

  double initT, endT;

  Hp=Hyp;
  Ha=Hya;

  n=0;
  cTime=0;
  nxttout=-1.0;

  //Get state var sizes for allocation
  if(Hp->ny>=Hp->nx){
    primSize=Hp->nvar*(Hp->nx+4)*Hp->ny;
    qSize   =Hp->nvar*(Hp->nx+2)*Hp->ny;
    flxSize =Hp->nvar*(Hp->nx+1)*Hp->ny;
  }else{
    primSize=Hp->nvar*(Hp->ny+4)*Hp->nx;
    qSize   =Hp->nvar*(Hp->ny+2)*Hp->nx;
    flxSize =Hp->nvar*(Hp->ny+1)*Hp->nx;
  }

  //If no end condition provided, end without running
  if(Ha->nstepmax<0&&Ha->tend<0.0)return;

  //Allocate state vars
  q  =(double*)malloc(primSize*sizeof(double));
  qr =(double*)malloc(qSize*sizeof(double));
  ql =(double*)malloc(qSize*sizeof(double));
  flx=(double*)malloc(flxSize*sizeof(double));

  //Set initial value of next time to aim to hit exactly
  if(Ha->tend>0.0){
    nxttout=Ha->tend;
  }
  if(Ha->dtoutput>0.0&&nxttout>Ha->dtoutput){
    nxttout=Ha->dtoutput;
  }

  //Setup conservation check vars
  volCell=Hp->dx*Hp->dy;
  oTM=sumArray(mesh,VARRHO,Hp->nx,Hp->ny);
  oTE=sumArray(mesh,VARPR ,Hp->nx,Hp->ny);
#ifdef M_PREC_CMP
  frexp(oTM,&M_exp);
  frexp(oTE,&E_exp);

  M_prec=ldexp(DBL_EPSILON,M_exp-1);
  E_prec=ldexp(DBL_EPSILON,E_exp-1);

  printf("TM:%g+-%g TE:%g+-%g\n",volCell*oTM,volCell*M_prec,volCell*oTE,volCell*E_prec);
#endif

  //Print initial condition
  snprintf(outfile,29,"%s%05d",Ha->outPre,n);
  writeVis(outfile,mesh,Hp->dx,Hp->dy,Hp->nvar,Hp->nx,Hp->ny);
  printf("INIT: TM: %g TE: %g\n",volCell*oTM,volCell*oTE);
  
  //Initialize timer
  initT=getNow();

  while((n<Ha->nstepmax||Ha->nstepmax<0)&&(cTime<Ha->tend||Ha->tend<0)){
    //Calculate timestep
    dt=Ha->sigma*calcDT(mesh);
    if(nxttout>0.0&&dt>(nxttout-cTime)){
      printf("Adjusting timestep from %g to %g for iter %d\n",dt,nxttout-cTime,n);
      dt=(nxttout-cTime);
    }
    if(n%2==0){
      //X Dir
      runPass(mesh,dt,n,0);
      //Y Dir
      runPass(mesh,dt,n,1);
    }else{
      //Y Dir
      runPass(mesh,dt,n,1);
      //X Dir
      runPass(mesh,dt,n,0);
    }
    //increment timestep and model time
    n+=1;
    cTime+=dt;
    //Print simple output line
    if(n%Ha->nprtLine==0){
      TM=sumArray(mesh,VARRHO,Hp->nx,Hp->ny);
      TE=sumArray(mesh,VARPR ,Hp->nx,Hp->ny);
      printf("Iter %05d time %f dt %g TM: %g TE: %g\n",n,cTime,dt,volCell*TM,volCell*TE);
      if(0&&(fabs(TM-oTM)>0.0||fabs(TE-oTE)>0.0)){
	printf("ERR(%5s): Mass %g Ene %g\n","RAW",volCell*(TM-oTM),volCell*(TE-oTE));
	printf("ERR(%5s): Mass %g Ene %g\n","%",100.0*(TM-oTM)/oTM,100.0*(TE-oTE)/oTE);
#ifdef M_PREC_CMP
	printf("ERR(%5s): Mass %g Ene %g\n","M PRE",(TM-oTM)/M_prec,(TE-oTE)/E_prec);
#endif
      }
    }
    //Print visualization file
    if((cTime>=nxttout&&nxttout>0)||(Ha->noutput>0&&n%Ha->noutput==0)){
      printf("Vis file @ time %f iter %d\n",cTime,n);
      if(cTime>=nxttout&&nxttout>0.0){
	nxttout+=Ha->dtoutput;
	if(nxttout>Ha->tend&&Ha->tend>0)nxttout=Ha->tend;
	//printf("Next Vis Time: %f\n",nxttout);
      }
      snprintf(outfile,29,"%s%05d",Ha->outPre,n);
      writeVis(outfile,mesh,Hp->dx,Hp->dy,Hp->nvar,Hp->nx,Hp->ny);
    }
  }
  printf("time: %f, %d iters run\n",cTime,n);

  endT=getNow();

  //Print timing information in manner easily extracted to process as csv
  printf("TFMT:%s,%s,%s,%s,%s,%s,%s,%s\n","cType","mType","init","nproc","nth","niters","ncells","wRunt");
  printf("TIME:%s,%s,%s,%d,%d,%d,%d,%g\n","\"C\"","\"CPU:?\"","\"Init\"",1,1,n,Hp->nx*Hp->ny,endT-initT);

  //Print final condition
  snprintf(outfile,29,"%s%05d",Ha->outPre,n);
  writeVis(outfile,mesh,Hp->dx,Hp->dy,Hp->nvar,Hp->nx,Hp->ny);

  Hp->t+=cTime;
  free(q  );
  free(qr );
  free(ql );
  free(flx);
}
