/*=========================================================================

 Program: FEMUS
 Module: Mesh
 Authors: Eugenio Aulisa
 
 Copyright (c) FEMTTU
 All rights reserved. 

 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

//----------------------------------------------------------------------------
// includes :
//----------------------------------------------------------------------------

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include "mpi.h"
#include <algorithm>
#include "SparseMatrix.hpp"
#include "NumericVector.hpp"
#include "map"
#include "Mesh.hpp"
#include "MeshGeneration.hpp"
#include "metis.h"
#include "GambitIO.hpp"

namespace femus {
  
using std::cout;
using std::endl;
using std::min;
using std::sort;
using std::map;

bool Mesh::_TestSetRefinementFlag=0;

const unsigned Mesh::_END_IND[5]= {0,1,3,4,5};

unsigned Mesh::_dimension=2;
unsigned Mesh::_ref_index=4;  // 8*DIM[2]+4*DIM[1]+2*DIM[0];
unsigned Mesh::_face_index=2; // 4*DIM[2]+2*DIM[1]+1*DIM[0];

//------------------------------------------------------------------------------------------------------

Mesh::~Mesh(){
    delete el;
    _coordinate->FreeSolutionVectors(); 
    delete _coordinate;
    if(_nprocs>=1){
      delete [] epart;
      delete [] npart;
    }
}

/// print Mesh info
void Mesh::PrintInfo() {
  
 std::cout << " Mesh Level        : " << _grid << std::endl; 
 std::cout << "   Number of elements: " << _nelem << std::endl; 
 std::cout << "   Number of nodes   : " << _nnodes << std::endl;
  
}

/**
 *  This function generates the coarse Mesh level, $l_0$, from an input Mesh file (Now only the Gambit Neutral File)
 **/
void Mesh::ReadCoarseMesh(const std::string& name, const double Lref, std::vector<bool> &type_elem_flag) {
    
  vector <vector <double> > coords(3);  
    
  _grid=0;

  if(name.rfind(".neu") < name.size())
  {
    GambitIO(*this).read(name,coords,Lref,type_elem_flag);
  }
  else
  {
    std::cerr << " ERROR: Unrecognized file extension: " << name
	      << "\n   I understand the following:\n\n"
	      << "     *.neu -- Gambit Neutral File\n"
              << std::endl;
  }
  
  RenumberNodes(coords);
  
  BuildAdjVtx();
  
  Buildkel();
  
 if (_nprocs>=1) GenerateMetisMeshPartition();
  vector <double> coords_temp;
  for(int i=0;i<3;i++){
    coords_temp=coords[i];
    for(unsigned j=0;j<_nnodes;j++) {
      coords[i][GetMetisDof(j,2)]=coords_temp[j];
    }
  }
  
  _coordinate = new Solution(this);
 
  _coordinate->AddSolution("X",LAGRANGE,SECOND,1,0); 
  _coordinate->AddSolution("Y",LAGRANGE,SECOND,1,0); 
  _coordinate->AddSolution("Z",LAGRANGE,SECOND,1,0); 
  
  _coordinate->ResizeSolutionVector("X");
  _coordinate->ResizeSolutionVector("Y");
  _coordinate->ResizeSolutionVector("Z");
    
  _coordinate->SetCoarseCoordinates(coords);
  
  _coordinate->AddSolution("AMR",DISCONTINOUS_POLYNOMIAL,ZERO,1,0); 
  
  _coordinate->ResizeSolutionVector("AMR");
    
};

/**
 *  This function generates the coarse Box Mesh level using the built-in generator
 **/
void Mesh::GenerateCoarseBoxMesh(
        const unsigned int nx, const unsigned int ny, const unsigned int nz,
        const double xmin, const double xmax,
        const double ymin, const double ymax,
        const double zmin, const double zmax,
        const ElemType type, std::vector<bool> &type_elem_flag) {
  
  vector <vector <double> > coords(3);  
    
  _grid=0;
    
  MeshTools::Generation::BuildBox(*this,coords,nx,ny,nz,xmin,xmax,ymin,ymax,zmin,zmax,type,type_elem_flag);
  
  RenumberNodes(coords);
  
  BuildAdjVtx();
  
  Buildkel();
  
 if (_nprocs>=1) GenerateMetisMeshPartition();
  vector <double> coords_temp;
  for(int i=0;i<3;i++){
    coords_temp=coords[i];
    for(unsigned j=0;j<_nnodes;j++) {
      coords[i][GetMetisDof(j,2)]=coords_temp[j];
    }
  }
  
  _coordinate = new Solution(this);
 
  _coordinate->AddSolution("X",LAGRANGE,SECOND,1,0); 
  _coordinate->AddSolution("Y",LAGRANGE,SECOND,1,0); 
  _coordinate->AddSolution("Z",LAGRANGE,SECOND,1,0); 
  
  _coordinate->ResizeSolutionVector("X");
  _coordinate->ResizeSolutionVector("Y");
  _coordinate->ResizeSolutionVector("Z");
    
  _coordinate->SetCoarseCoordinates(coords);
  
  _coordinate->AddSolution("AMR",DISCONTINOUS_POLYNOMIAL,ZERO,1,0); 
  
  _coordinate->ResizeSolutionVector("AMR");
  
}  
  

//------------------------------------------------------------------------------------------------------
void Mesh::RenumberNodes(vector < vector < double> > &coords) {
  
  vector <unsigned> dof_index;
  dof_index.resize(_nnodes);
  for(unsigned i=0;i<_nnodes;i++){
    dof_index[i]=i+1;
  }
  //reorder vertices and mid-points vs central points
  for (unsigned iel=0; iel<_nelem; iel++) {
    for (unsigned inode=0; inode<el->GetElementDofNumber(iel,1); inode++) {
      for (unsigned jel=0; jel<_nelem; jel++) {
	for (unsigned jnode=el->GetElementDofNumber(jel,1); jnode<el->GetElementDofNumber(jel,3); jnode++) { 
	  unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
	  unsigned jj=el->GetElementVertexIndex(jel,jnode)-1;
	  unsigned i0=dof_index[ii];
          unsigned i1=dof_index[jj];
	  if(i0>i1){
	    dof_index[ii]=i1;
	    dof_index[jj]=i0; 
	  }
	}
      }
    }
  }
  //reorder vertices vs mid-points
  for (unsigned iel=0; iel<_nelem; iel++) {
    for (unsigned inode=0; inode<el->GetElementDofNumber(iel,0); inode++) {
      for (unsigned jel=0; jel<_nelem; jel++) {
        for (unsigned jnode=el->GetElementDofNumber(jel,0); jnode<el->GetElementDofNumber(jel,1); jnode++) {
          unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
	  unsigned jj=el->GetElementVertexIndex(jel,jnode)-1;
	  unsigned i0=dof_index[ii];
          unsigned i1=dof_index[jj];
	  if(i0>i1){
	    dof_index[ii]=i1;
	    dof_index[jj]=i0; 
	  }
	}
      }
    }
  }
  
  // update all
  for (unsigned iel=0; iel<_nelem; iel++) {
    for (unsigned inode=0; inode<el->GetElementDofNumber(iel,3); inode++) {
      unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
      el->SetElementVertexIndex(iel,inode,dof_index[ii]);
    }
  }
  vector <double> coords_temp;
  for(int i=0;i<3;i++){
    coords_temp=coords[i];
    for(unsigned j=0;j<_nnodes;j++){
      coords[i][dof_index[j]-1]=coords_temp[j];
    }
  }
  // **************  end reoreder mesh dofs **************
 
  el->SetNodeNumber(_nnodes);

  unsigned nv0=0;
  for (unsigned iel=0; iel<_nelem; iel++)
    for (unsigned inode=0; inode<el->GetElementDofNumber(iel,0); inode++) {
      unsigned i0=el->GetElementVertexIndex(iel,inode);
      if (nv0<i0) nv0=i0;
  }
  el->SetVertexNodeNumber(nv0);

  unsigned nv1=0;
  for (unsigned iel=0; iel<_nelem; iel++)
    for (unsigned inode=el->GetElementDofNumber(iel,0); inode<el->GetElementDofNumber(iel,1); inode++) {
      unsigned i1=el->GetElementVertexIndex(iel,inode);
      if (nv1<i1) nv1=i1;
  }
  el->SetMidpointNodeNumber(nv1-nv0);

  el->SetCentralNodeNumber(_nnodes-nv1);
  
}  

/**
 * This function searches all the elements around all the vertices
 **/
void Mesh::BuildAdjVtx() {
  el->AllocateVertexElementMemory();
  for (unsigned iel=0; iel<_nelem; iel++) {
    for (unsigned inode=0; inode < el->GetElementDofNumber(iel,0); inode++) {
      unsigned ii=el->GetElementVertexIndex(iel,inode)-1u;
      unsigned jj=0;
      while ( 0 != el->GetVertexElementIndex(ii,jj) ) jj++;
      el->SetVertexElementIndex(ii,jj,iel+1u);
    }
  }
}


/**
 * This function generates kmid for hex and wedge elements
 **/
void Mesh::Buildkmid() {
  for (unsigned iel=0; iel<el->GetElementNumber(); iel++)
    for (unsigned inode=el->GetElementDofNumber(iel,1); inode<el->GetElementDofNumber(iel,2); inode++)
      el->SetElementVertexIndex(iel,inode,0);

  for (unsigned iel=0; iel<el->GetElementNumber(); iel++) {
    for (unsigned iface=0; iface<el->GetElementFaceNumber(iel,0); iface++) {
      unsigned inode=el->GetElementDofNumber(iel,1)+iface;
      if ( 0==el->GetElementVertexIndex(iel,inode) ) {
        el->SetElementVertexIndex(iel,inode,++_nnodes);
        unsigned i1=el->GetFaceVertexIndex(iel,iface,0);
        unsigned i2=el->GetFaceVertexIndex(iel,iface,1);
        unsigned i3=el->GetFaceVertexIndex(iel,iface,2);
        for (unsigned j=0; j< el->GetVertexElementNumber(i1-1u); j++) {
          unsigned jel= el->GetVertexElementIndex(i1-1u,j)-1u;
          if (jel>iel) {
            for (unsigned jface=0; jface<el->GetElementFaceNumber(jel,0); jface++) {
              unsigned jnode=el->GetElementDofNumber(jel,1)+jface;
              if ( 0==el->GetElementVertexIndex(jel,jnode) ) {
                unsigned j1=el->GetFaceVertexIndex(jel,jface,0);
                unsigned j2=el->GetFaceVertexIndex(jel,jface,1);
                unsigned j3=el->GetFaceVertexIndex(jel,jface,2);
                unsigned j4=el->GetFaceVertexIndex(jel,jface,3);
                if ((i1==j1 || i1==j2 || i1==j3 ||  i1==j4 )&&
                    (i2==j1 || i2==j2 || i2==j3 ||  i2==j4 )&&
                    (i3==j1 || i3==j2 || i3==j3 ||  i3==j4 )) {
                  el->SetElementVertexIndex(jel,jnode,_nnodes);
                }
              }
            }
          }
        }
      }
    }
  }

  for (unsigned iel=0; iel<el->GetElementNumber(); iel++) {
    if (0==el->GetElementType(iel)) {
      el->SetElementVertexIndex(iel,26,++_nnodes);
    }
    if (3==el->GetElementType(iel)) {
      el->SetElementVertexIndex(iel,8,++_nnodes);
    }
  }
  el->SetNodeNumber(_nnodes);

  unsigned nv0= el->GetVertexNodeNumber();
  unsigned nv1= el->GetMidpointNodeNumber();
  el->SetCentralNodeNumber(_nnodes-nv0-nv1);

}

/**
 * This function stores the element adiacent to the element face (iel,iface)
 * and stores it in kel[iel][iface]
 **/
void Mesh::Buildkel() {
  for (unsigned iel=0; iel<el->GetElementNumber(); iel++) {
    for (unsigned iface=0; iface<el->GetElementFaceNumber(iel); iface++) {
      if ( el->GetFaceElementIndex(iel,iface) <= 0) {
        unsigned i1=el->GetFaceVertexIndex(iel,iface,0);
        unsigned i2=el->GetFaceVertexIndex(iel,iface,1);
        unsigned i3=el->GetFaceVertexIndex(iel,iface,2);
        for (unsigned j=0; j< el->GetVertexElementNumber(i1-1u); j++) {
          unsigned jel= el->GetVertexElementIndex(i1-1u,j)-1u;
          if (jel>iel) {
            for (unsigned jface=0; jface<el->GetElementFaceNumber(jel); jface++) {
              if ( el->GetFaceElementIndex(jel,jface) <= 0) {
                unsigned j1=el->GetFaceVertexIndex(jel,jface,0);
                unsigned j2=el->GetFaceVertexIndex(jel,jface,1);
                unsigned j3=el->GetFaceVertexIndex(jel,jface,2);
                unsigned j4=el->GetFaceVertexIndex(jel,jface,3);
// 		if((DIM[2]==1 &&
                if ((Mesh::_dimension==3 &&
                     (i1==j1 || i1==j2 || i1==j3 ||  i1==j4 )&&
                     (i2==j1 || i2==j2 || i2==j3 ||  i2==j4 )&&
                     (i3==j1 || i3==j2 || i3==j3 ||  i3==j4 ))||
// 		   (DIM[1]==1 &&
                    (Mesh::_dimension==2 &&
                     (i1==j1 || i1==j2 )&&
                     (i2==j1 || i2==j2 ))||
// 		   (DIM[0]==1 &&
                    (Mesh::_dimension==1 &&
                     (i1==j1))
                   ) {
                  el->SetFaceElementIndex(iel,iface,jel+1u);
                  el->SetFaceElementIndex(jel,jface,iel+1u);
                }
              }
            }
          }
        }
      }
    }
  }
}


/**
 * This function returns the number of Mesh nodes for different type of elemets
 **/
unsigned Mesh::GetDofNumber(const unsigned type) const {

  switch (type) {
  case 0:
    return el->GetVertexNodeNumber();
    break;
  case 1:
    return el->GetVertexNodeNumber()+el->GetMidpointNodeNumber();
    break;
  case 2:
    return _nnodes;
    break;
  case 3:
    return _nelem;
    break;
  case 4:
    return _nelem*(2+Mesh::_dimension-1);
    break;
  }
  return 0;
}


/**
 * This function copies the refined element index vector in other_vector
 **/
void Mesh::copy_elr(vector <unsigned> &other_vec) const {
  for (unsigned i=0; i<_nelem; i++)
    other_vec[i]=el->GetRefinedElementIndex(i);
}


void Mesh::AllocateAndMarkStructureNode() {
  el->AllocateNodeRegion();
  for (unsigned iel=0; iel<_nelem; iel++) {

    int flag_mat = el->GetElementMaterial(iel);

    if (flag_mat==4) {
      unsigned nve=el->GetElementDofNumber(iel);
      for ( unsigned i=0; i<nve; i++) {
        unsigned inode=el->GetElementVertexIndex(iel,i)-1u;
        el->SetNodeRegion(inode, 1);
      }
    }
  }
  return;
}


/**
 *  This function generates a finer Mesh level, $l_i$, from a coarser Mesh level $l_{i-1}$, $i>0$
 **/

void Mesh::SetFiniteElementPtr(const elem_type * OtherFiniteElement[6][5]){
  
  for(int i=0;i<6;i++)
    for(int j=0;j<5;j++)
      _finiteElement[i][j] = OtherFiniteElement[i][j];
}



//-----------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------
void Mesh::GenerateMetisMeshPartition(){
   
  unsigned eind_size = el->GetElementNumber("Hex")*NVE[0][3] + el->GetElementNumber("Tet")*NVE[1][3] 
                     + el->GetElementNumber("Wedge")*NVE[2][3] + el->GetElementNumber("Quad")*NVE[3][3] 
                     + el->GetElementNumber("Triangle")*NVE[4][3] + el->GetElementNumber("Line")*NVE[5][3];

  idx_t *eptr=new idx_t [_nelem+1];
  idx_t *eind=new idx_t [eind_size];
  
  idx_t objval;
  idx_t options[METIS_NOPTIONS]; 
  
  METIS_SetDefaultOptions(options);
    
  options[METIS_OPTION_NUMBERING]= 0;
  options[METIS_OPTION_DBGLVL]   = 0;
  options[METIS_OPTION_CTYPE]    = METIS_CTYPE_SHEM; 
  options[METIS_OPTION_PTYPE]	 = METIS_PTYPE_KWAY;
  options[METIS_OPTION_IPTYPE]   = METIS_IPTYPE_RANDOM;
  options[METIS_OPTION_CONTIG]   = 0;//params->contig;
  options[METIS_OPTION_MINCONN]  = 1;
  options[METIS_OPTION_NITER]    = 10;
  options[METIS_OPTION_UFACTOR]  = 100;
  
  eptr[0]=0;
  unsigned counter=0;
  for (unsigned iel=0; iel<_nelem; iel++) {
    unsigned ielt=el->GetElementType(iel);
    eptr[iel+1]=eptr[iel]+NVE[ielt][3];
    
    for (unsigned inode=0; inode<el->GetElementDofNumber(iel,3); inode++){
      eind[counter]=el->GetElementVertexIndex(iel,inode)-1;
    
      counter++;
    }
    
  }
  
  idx_t mnel = _nelem; 
  idx_t mnvt = _nnodes;
  idx_t ncommon = _dimension+1;
  nsubdom = _nprocs;
    
  epart=new idx_t [_nelem];
  npart=new idx_t [_nnodes];
  
  if(nsubdom!=1) {
  //I call the Mesh partioning function of Metis library (output is epart(own elem) and npart (own nodes))
  int err = METIS_PartMeshDual(&mnel, &mnvt, eptr, eind, NULL, NULL, &ncommon, &nsubdom, NULL, options, &objval, epart, npart);
  
  if(err==METIS_OK) {
    cout << " METIS PARTITIONING IS OK " << endl;
  }
  else if(err==METIS_ERROR_INPUT) {
    cout << " METIS_ERROR_INPUT " << endl;
    exit(1);
  }
  else if (err==METIS_ERROR_MEMORY) {
    cout << " METIS_ERROR_MEMORY " << endl;
    exit(2);
  }
  else {
    cout << " METIS_GENERIC_ERROR " << endl;
    exit(3);
   }
  }
  else {
    //serial computation
    for(unsigned i=0;i<_nelem;i++) {
      epart[i]=0;
    } 
  }
  
  delete [] eptr;
  delete [] eind;

   //dof map: piecewise liner 0, quadratic 1, biquadratic 2, piecewise constant 3, picewise discontinous linear 4 
  
  //resize the vector IS_Gmt2Mts_dof and dof
  for(int k=0;k<5;k++) {
    IS_Gmt2Mts_dof[k].resize(GetDofNumber(k)); 
    IS_Gmt2Mts_dof_offset[k].resize(nsubdom+1);
  }
  IS_Mts2Gmt_elem.resize(_nelem);
  IS_Mts2Gmt_elem_offset.resize(nsubdom+1);
  
  // I 
  for(unsigned i=0;i<_nnodes;i++) {
    npart[i]=nsubdom;
  }
  
  IS_Mts2Gmt_elem_offset[0]=0;
  vector <unsigned> IS_Gmt2Mts_dof_counter(5,0);
  
   for(int k=0;k<5;k++) {
     IS_Gmt2Mts_dof[k].assign(GetDofNumber(k),GetDofNumber(k)-1); 
     //TODO for domain decomposition pourposes! the non existing dofs point to the last dof!!!!!!
   }
  
  
  IS_Gmt2Mts_dof_counter[3]=0;
  IS_Gmt2Mts_dof_counter[4]=0;
  
  for(int isdom=0;isdom<nsubdom;isdom++) {
    for(unsigned iel=0;iel<_nelem;iel++){
      if(epart[iel]==isdom){
	//filling the piecewise IS_Mts2Gmt_elem metis->gambit
	IS_Mts2Gmt_elem[ IS_Gmt2Mts_dof_counter[3] ]=iel;
	IS_Gmt2Mts_dof[3][iel]=IS_Gmt2Mts_dof_counter[3];
	IS_Gmt2Mts_dof_counter[3]++;
	IS_Mts2Gmt_elem_offset[isdom+1]=IS_Gmt2Mts_dof_counter[3];
	// linear+quadratic+biquadratic
	for (unsigned inode=0; inode<el->GetElementDofNumber(iel,0); inode++) {
	  unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
	  if(npart[ii]>isdom) {
	    npart[ii]=isdom;
	    IS_Gmt2Mts_dof[0][ii]=IS_Gmt2Mts_dof_counter[0];
	    IS_Gmt2Mts_dof_counter[0]++;
	    IS_Gmt2Mts_dof[1][ii]=IS_Gmt2Mts_dof_counter[1];
	    IS_Gmt2Mts_dof_counter[1]++;
	    IS_Gmt2Mts_dof[2][ii]=IS_Gmt2Mts_dof_counter[2];
	    IS_Gmt2Mts_dof_counter[2]++;
	  }
	}
	// quadratic+biquadratic
	for (unsigned inode=el->GetElementDofNumber(iel,0); inode<el->GetElementDofNumber(iel,1); inode++) {
	  unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
	  if(npart[ii]>isdom){
	    npart[ii]=isdom;
	    IS_Gmt2Mts_dof[1][ii]=IS_Gmt2Mts_dof_counter[1];
	    IS_Gmt2Mts_dof_counter[1]++;
	    IS_Gmt2Mts_dof[2][ii]=IS_Gmt2Mts_dof_counter[2];
	    IS_Gmt2Mts_dof_counter[2]++;
	  }
	}
	// biquadratic
	for (unsigned inode=el->GetElementDofNumber(iel,1); inode<el->GetElementDofNumber(iel,3); inode++) {
	  unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
	  if(npart[ii]>isdom){
	    npart[ii]=isdom;
	    IS_Gmt2Mts_dof[2][ii]=IS_Gmt2Mts_dof_counter[2];
	    IS_Gmt2Mts_dof_counter[2]++;
	  }
	}	
      }
    }
    for(unsigned k_dim=0;k_dim<_dimension+1;k_dim++){
      for(unsigned iel=0;iel<_nelem;iel++){
     	if(epart[iel]==isdom){
	  IS_Gmt2Mts_dof[4][iel+k_dim*_nelem]=IS_Gmt2Mts_dof_counter[4];
	  IS_Gmt2Mts_dof_counter[4]++;
	}
      }
    }
  }  
   
  // ghost vs own nodes
  vector <unsigned short> node_count(_nnodes,0);
  
  for(unsigned k=0;k<5;k++){
    ghost_size[k].assign(nsubdom,0);
    own_size[k].assign(nsubdom,0);
  }
  
  for(int isdom=0;isdom<nsubdom;isdom++){
    
    own_size[3][isdom] = IS_Mts2Gmt_elem_offset[isdom+1]-IS_Mts2Gmt_elem_offset[isdom];
    own_size[4][isdom] = (IS_Mts2Gmt_elem_offset[isdom+1]-IS_Mts2Gmt_elem_offset[isdom])*(_dimension+1);
    
    for(unsigned i=IS_Mts2Gmt_elem_offset[isdom];i<IS_Mts2Gmt_elem_offset[isdom+1];i++){
      unsigned iel=IS_Mts2Gmt_elem[i]; 
      
       for (unsigned inode=0; inode<el->GetElementDofNumber(iel,0); inode++) {
	unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
	if(node_count[ii]<isdom+1){
	  node_count[ii]=isdom+1;
	  if( npart[ii] != isdom){
	    ghost_size[0][isdom]++;
	    ghost_size[1][isdom]++;
	    ghost_size[2][isdom]++;
	  }else {
	    own_size[0][isdom]++;
	    own_size[1][isdom]++;
	    own_size[2][isdom]++;
	  }
	}
      }
      
      
       for (unsigned inode=el->GetElementDofNumber(iel,0); inode<el->GetElementDofNumber(iel,1); inode++) {
	unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
	if(node_count[ii]<isdom+1){
	  node_count[ii]=isdom+1;
	  if( npart[ii] != isdom){
	    ghost_size[1][isdom]++;
	    ghost_size[2][isdom]++;
	  }else {
	    own_size[1][isdom]++;
	    own_size[2][isdom]++;
	  }
	}
      }
      
      
      for (unsigned inode=el->GetElementDofNumber(iel,1); inode<el->GetElementDofNumber(iel,3); inode++) {
	unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
	if(node_count[ii]<isdom+1){
	  node_count[ii]=isdom+1;
	  if( npart[ii] != isdom){
	    ghost_size[2][isdom]++;
	  }else {
	    own_size[2][isdom]++;
	  }
	}
      }
      
      
    }
  }
  

  for(int k=0; k<5; k++) {
    ghost_nd[k].resize(nsubdom);
    ghost_nd_mts[k].resize(nsubdom);
    for(int isdom=0; isdom<nsubdom; isdom++) {
      ghost_nd[k][isdom].resize(ghost_size[k][isdom]);
      ghost_nd_mts[k][isdom].resize(ghost_size[k][isdom]);    
    }
  } 
  
  node_count.assign (_nnodes,0);  
  for(int k=0; k<5; k++) {
    ghost_size[k].assign(nsubdom,0);
  }
  
  for(int isdom=0;isdom<nsubdom;isdom++) {
    for(unsigned i=IS_Mts2Gmt_elem_offset[isdom];i<IS_Mts2Gmt_elem_offset[isdom+1];i++){
      unsigned iel=IS_Mts2Gmt_elem[i]; 
      
      
      for (unsigned inode=0; inode<el->GetElementDofNumber(iel,0); inode++) {
	unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
	if(node_count[ii]<isdom+1){
	  node_count[ii]=isdom+1;
	  if(npart[ii] != isdom){
 	    ghost_nd_mts[0][isdom][ghost_size[0][isdom]]=IS_Gmt2Mts_dof[0][ii];
            ghost_nd[0][isdom][ghost_size[0][isdom]]=ii;
	    ghost_size[0][isdom]++;
 	    ghost_nd_mts[1][isdom][ghost_size[1][isdom]]=IS_Gmt2Mts_dof[1][ii];
	    ghost_nd[1][isdom][ghost_size[1][isdom]]=ii;
	    ghost_size[1][isdom]++;
 	    ghost_nd_mts[2][isdom][ghost_size[2][isdom]]=IS_Gmt2Mts_dof[2][ii];
	    ghost_nd[2][isdom][ghost_size[2][isdom]]=ii;
	    ghost_size[2][isdom]++;
	  }
	}
      }
      
      for (unsigned inode=el->GetElementDofNumber(iel,0); inode<el->GetElementDofNumber(iel,1); inode++) {
	unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
	if(node_count[ii]<isdom+1){
	  node_count[ii]=isdom+1;
	  if( npart[ii] != isdom){
 	    ghost_nd_mts[1][isdom][ghost_size[1][isdom]]=IS_Gmt2Mts_dof[1][ii];
	    ghost_nd[1][isdom][ghost_size[1][isdom]]=ii;
	    ghost_size[1][isdom]++;
	    ghost_nd_mts[2][isdom][ghost_size[2][isdom]]=IS_Gmt2Mts_dof[2][ii];
	    ghost_nd[2][isdom][ghost_size[2][isdom]]=ii;
	    ghost_size[2][isdom]++;
	  }
	}
      }
      
      for(unsigned inode=el->GetElementDofNumber(iel,1); inode<el->GetElementDofNumber(iel,3); inode++) {
	unsigned ii=el->GetElementVertexIndex(iel,inode)-1;
	if(node_count[ii]<isdom+1){
	  node_count[ii]=isdom+1;
	  if( npart[ii] != isdom){
 	    ghost_nd_mts[2][isdom][ghost_size[2][isdom]]=IS_Gmt2Mts_dof[2][ii];
            ghost_nd[2][isdom][ghost_size[2][isdom]]=ii;
	    ghost_size[2][isdom]++;
	  }
	}
      } 
    }
  }
  
  
  MetisOffset.resize(5);
  for(int i=0;i<5;i++) 
    MetisOffset[i].resize(nsubdom+1);
  
  MetisOffset[0][0]=0;
  MetisOffset[1][0]=0;
  MetisOffset[2][0]=0;
  MetisOffset[3][0]=0;
  MetisOffset[4][0]=0;
  
  for(int i=1;i<=nsubdom;i++){
    MetisOffset[0][i]= MetisOffset[0][i-1]+own_size[0][i-1];
    MetisOffset[1][i]= MetisOffset[1][i-1]+own_size[1][i-1];
    MetisOffset[2][i]= MetisOffset[2][i-1]+own_size[2][i-1];
    MetisOffset[3][i]= IS_Mts2Gmt_elem_offset[i];
    MetisOffset[4][i]= IS_Mts2Gmt_elem_offset[i]*(_dimension+1);
    
  }
  
   
  return; 
  
}


//----------------------------------------------------------------------------------------------------------------
void Mesh::GenerateVankaPartitions_FAST( const unsigned &block_size, vector < vector< unsigned > > &block_elements,
					 vector <unsigned> &block_type_range){
  unsigned iproc=processor_id();
  unsigned ElemOffset    = IS_Mts2Gmt_elem_offset[iproc];
  unsigned ElemOffsetp1  = IS_Mts2Gmt_elem_offset[iproc+1];
  unsigned OwnedElements = ElemOffsetp1 - ElemOffset;
  unsigned reminder = OwnedElements % block_size;
  
  unsigned nbloks = (0 == reminder)? OwnedElements/block_size : OwnedElements/block_size+1 ;
    
  //vector < unsigned > size(nbloks,0);
  block_elements.resize(nbloks);
  
  for(int i = 0; i < block_elements.size(); i++) 
    block_elements[i].resize(block_size);
  if  (0 != reminder ){
    block_elements[ block_elements.size()-1].resize(reminder); 
  }
  
  for (unsigned iel=0; iel<OwnedElements; iel++) {
    block_elements[iel/block_size][iel%block_size]=iel+ElemOffset;
  }
  block_type_range.resize(2);
  block_type_range[0]=block_elements.size();
  block_type_range[1]=block_elements.size();
} 

//----------------------------------------------------------------------------------------------------------------
void Mesh::GenerateVankaPartitions_FSI( const unsigned &block_size, vector < vector< unsigned > > &block_elements,
					vector <unsigned> &block_type_range){

  unsigned iproc=processor_id();
  unsigned ElemOffset    = IS_Mts2Gmt_elem_offset[iproc];
  unsigned ElemOffsetp1  = IS_Mts2Gmt_elem_offset[iproc+1];
  unsigned OwnedElements = ElemOffsetp1 - ElemOffset;

  block_elements.resize(2); 
  block_elements[0].resize(OwnedElements); 
  block_elements[1].resize(OwnedElements); 
  
  block_type_range.resize(2);
  block_type_range[0]=1;
  block_type_range[1]=2;
  
  
  unsigned counter_f=0;
  unsigned counter_s=0;
  for (unsigned iel_mts = ElemOffset; iel_mts < ElemOffsetp1; iel_mts++) {
    unsigned kel        = IS_Mts2Gmt_elem[iel_mts]; 
    unsigned flag_mat   = el->GetElementMaterial(kel);
    if(2 == flag_mat){
      block_elements[0][counter_f]=iel_mts;
      counter_f++;
    } 
    else{
      block_elements[1][counter_s]=iel_mts;
      counter_s++;
    }
  }  
  
  if(counter_s==0){
    block_elements.erase (block_elements.begin()+1);
    block_type_range[1]=1;
  }
  else{
    block_elements[1].resize(counter_s);
  }
  
  if(counter_f==0){
    block_elements.erase (block_elements.begin());
    block_type_range[0]=0;
    block_type_range[1]=1;
  }
  else{
    block_elements[0].resize(counter_f); 
  }
  
} 

//----------------------------------------------------------------------------------------------------------------
void Mesh::GenerateVankaPartitions_FSI1( const unsigned *block_size, vector < vector< unsigned > > &block_elements,
					 vector <unsigned> &block_type_range){

  unsigned iproc=processor_id();
  unsigned ElemOffset    = IS_Mts2Gmt_elem_offset[iproc];
  unsigned ElemOffsetp1  = IS_Mts2Gmt_elem_offset[iproc+1];
  unsigned OwnedElements = ElemOffsetp1 - ElemOffset;

  unsigned counter[2]={0,0};
  for (unsigned iel_mts = ElemOffset; iel_mts < ElemOffsetp1; iel_mts++) {
    unsigned kel        = IS_Mts2Gmt_elem[iel_mts]; 
    unsigned flag_mat   = el->GetElementMaterial(kel);
//    unsigned flag_mat   = GetElementMaterial(kel);
    if(2 == flag_mat){
      counter[1]++;
    } 
  }
  counter[0]=OwnedElements - counter[1];
  
  block_type_range.resize(2);
 
  unsigned flag_block[2]={4,2};
  
  unsigned block_start=0;
  unsigned iblock=0;
  while(iblock < 2){
    if(counter[iblock] !=0 ){
      unsigned reminder = counter[iblock] % block_size[iblock];
      unsigned blocks = (0 == reminder)? counter[iblock]/block_size[iblock] : counter[iblock]/block_size[iblock] + 1 ;
      block_elements.resize(block_start+blocks);
  
      for(int i = 0; i < blocks; i++) 
	block_elements[block_start + i].resize(block_size[iblock]);
      if  (0 != reminder ){
	block_elements[block_start + (blocks-1u)].resize(reminder); 
      }
      
      unsigned counter=0;
      for (unsigned iel_mts = ElemOffset; iel_mts < ElemOffsetp1; iel_mts++) {
	unsigned kel        = IS_Mts2Gmt_elem[iel_mts]; 
 	unsigned flag_mat   = el->GetElementMaterial(kel);
//	unsigned flag_mat   = GetElementMaterial(kel);
	if( flag_block[iblock] == flag_mat ){
	  block_elements[ block_start + (counter / block_size[iblock]) ][ counter % block_size[iblock] ]=iel_mts;
	  counter++;
	}	 
      }
      block_type_range[iblock]=block_start+blocks;
      block_start += blocks;
    }
    else{
      block_type_range[iblock]=block_start;
    }
    iblock++;
  }

} 

//----------------------------------------------------------------------------------------------------------------
void Mesh::GenerateVankaPartitions_METIS( const unsigned &vnk_blck, vector < vector< unsigned > > &block_elements){
   
 
  
  unsigned iproc=processor_id();
   
  unsigned ElemOffset    = IS_Mts2Gmt_elem_offset[iproc];
  unsigned ElemOffsetp1  = IS_Mts2Gmt_elem_offset[iproc+1];
  unsigned OwnedElements = ElemOffsetp1 - ElemOffset;
    
  vector < idx_t > connectivity_index_size(OwnedElements+1);
  vector < idx_t > connectivity(0); 
  
  connectivity.reserve(static_cast < int > (pow(3,_dimension)) * OwnedElements + 1);
     
  map <unsigned, unsigned> node_map;
    
  connectivity_index_size[0] = 0;
  unsigned counter = 0;
  unsigned node_counter = 0;
  for (unsigned iel_metis = ElemOffset; iel_metis < ElemOffsetp1; iel_metis++) {
    unsigned iel        = IS_Mts2Gmt_elem[iel_metis]; 
    short unsigned ielt = el->GetElementType(iel);
    unsigned nve        = el->GetElementDofNumber(iel,3);
       
    connectivity_index_size[ iel_metis - ElemOffset + 1 ]=connectivity_index_size[iel_metis - ElemOffset]+nve;
    connectivity.resize( connectivity.size() + nve );
    for (unsigned j = 0; j < nve; j++){
      unsigned jnode = el->GetElementVertexIndex(iel,j)-1;
      map <unsigned, unsigned>::iterator it = node_map.find(jnode);
      if(it != node_map.end() ){
	connectivity[counter] = it->second;
      }
      else{
	node_map[jnode]       = node_counter;
	connectivity[counter] = node_counter;
	node_counter++;
      }
    }
  }		     
 
  //cout<<1<<endl;
 
  idx_t objval;
  idx_t options[METIS_NOPTIONS]; 
  
  METIS_SetDefaultOptions(options);
    
  options[METIS_OPTION_NUMBERING]= 0;
  options[METIS_OPTION_DBGLVL]   = 0;
  options[METIS_OPTION_CTYPE]    = METIS_CTYPE_SHEM; 
  options[METIS_OPTION_PTYPE]	 = METIS_PTYPE_KWAY;
  options[METIS_OPTION_IPTYPE]   = METIS_IPTYPE_RANDOM;
  options[METIS_OPTION_CONTIG]   = 0;//params->contig;
  options[METIS_OPTION_MINCONN]  = 1;
  options[METIS_OPTION_NITER]    = 10;
  options[METIS_OPTION_UFACTOR]  = 100;
  
  idx_t mnel = OwnedElements; 
  idx_t mnvt = node_map.size();
  idx_t ncommon = _dimension+1;
  idx_t nsubdom = OwnedElements/vnk_blck;
    
  vector < idx_t > epart(OwnedElements);
  vector < idx_t > npart(node_map.size());
  
  if(nsubdom!=1) {
  //I call the Mesh partioning function of Metis library (output is epart(own elem) and npart (own nodes))
  int err = METIS_PartMeshDual(&mnel, &mnvt, &connectivity_index_size[0], &connectivity[0], 
			       NULL, NULL, &ncommon, &nsubdom, NULL, options, &objval, &epart[0], &npart[0]);
  
    if(err==METIS_OK) {
      cout << " METIS PARTITIONING IS OK " << endl;
    }
    else if(err==METIS_ERROR_INPUT) {
      cout << " METIS_ERROR_INPUT " << endl;
      exit(1);
    }
    else if (err==METIS_ERROR_MEMORY) {
      cout << " METIS_ERROR_MEMORY " << endl;
      exit(2);
    }
    else {
      cout << " METIS_GENERIC_ERROR " << endl;
      exit(3);
    }
  }
  else {
    //serial computation
    for(unsigned i=0;i<OwnedElements;i++) {
      epart[i]=0;
    } 
  }
  
  //cout<<2<<endl;
  
  vector < unsigned > size(nsubdom,0);
  block_elements.resize(nsubdom);
  
  //cout<<4<<endl;
  for(int i=0;i<block_elements.size();i++) 
    block_elements[i].resize(2 * vnk_blck );
   //for(int i=0; i<block_elements.size(); i++) {
    //cout<<i<<" "<<block_elements[i].size() << " "<< vnk_blck<<" "<< OwnedElements<<" "<<2 * OwnedElements / vnk_blck +1 << endl;
   //}
  for(int iel=0; iel<OwnedElements; iel++){
    block_elements[ epart[iel] ] [size[epart[iel]]] = iel + ElemOffset;
    (size[epart[iel]])++;
  }
  
  //cout<<6<<endl;
  
  for(int i=0; i<block_elements.size(); i++) {
    //cout<<size[i]<< " "<<block_elements[i].size() << endl;
    block_elements[i].resize( size[i]);
  }
  //cout<<7<<endl;
  
}




} //end namespace femus


