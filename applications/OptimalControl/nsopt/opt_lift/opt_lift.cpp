// Solving Navier-Stokes problem using automatic differentiation and/or Picards method
// boundary conditions were set in 2D as, no slip in left,right of the box and top to bottom gravity is enforced
// therefore, U=V=0 on left and right, U=0 on top and bottom, V is free 



#include "FemusInit.hpp"
#include "MultiLevelProblem.hpp"
#include "NumericVector.hpp"
#include "VTKWriter.hpp"
#include "GMVWriter.hpp"
#include "NonLinearImplicitSystem.hpp"
#include "adept.h"

// #include "MultiLevelMesh.hpp"
#include "LinearImplicitSystem.hpp"
// #include "WriterEnum.hpp"

#include "Fluid.hpp"
#include "Parameter.hpp"
#include "Files.hpp"
#include <stdio.h>

#include   "../nsopt_params.hpp"


using namespace femus;

 
bool SetBoundaryConditionOpt(const std::vector < double >& x, const char SolName[], double& value, const int facename, const double time) {
  //1: bottom  //2: right  //3: top  //4: left
  
  bool dirichlet = true;
   value = 0.;

  //lid-driven problem----------------------------------------------------------------------
// TOP ==========================  
      if (facename == 3) {
       if (!strcmp(SolName, "UCTRL"))    { dirichlet = false; }
  else if (!strcmp(SolName, "VCTRL"))    { dirichlet = false;} 
	
      }   
   //lid-driven problem----------------------------------------------------------------------
 

//    //Poiseuille problem---------------------------------------------------------------
// // LEFT ==========================  
//       if (facename == 4) { 
// 	if(x[1] > 0.3  && x[1] < 0.7){
// 		if (!strcmp(SolName, "UCTRL"))    {dirichlet = false; }
// 	    else if (!strcmp(SolName, "VCTRL"))    { value = 0.; } 
// 	}
//       }
//       
// // RIGHT ==========================  
//      if (facename == 2) {
//        if (!strcmp(SolName, "UCTRL"))    { dirichlet = false; }
//   else if (!strcmp(SolName, "VCTRL"))    { value = 0.; } 
//       }
//       
//       if (!strcmp(SolName, "P"))  { 
// 	 dirichlet = false;
//            if (facename == 4)  value = 1.; 
//            if (facename == 2)  value = 0.;
//    
//       }
//    //Poiseuille problem---------------------------------------------------------------
      
  return dirichlet;
}



//    //lid-driven problem----------------------------------------------------------------------
// //============== initial conditions =========
// double SetInitialCondition(const MultiLevelProblem * ml_prob, const std::vector <double> &x, const char SolName[]) {
//   
//   double value = 0.;
//   
//   if (x[1] < 1+ 1.e-5 && x[1] > 1 - 1.e-5 ) {
//                 if (!strcmp(SolName, "UCTRL"))       { value = 1.; }
//                 if (!strcmp(SolName, "VCTRL"))       { value = 0.; }
//   }
//   
//   return value;
// }
// //============== initial conditions =========
//    //lid-driven problem----------------------------------------------------------------------


void AssembleNavierStokesOpt_nonAD(MultiLevelProblem &ml_prob);
void AssembleNavierStokesOpt_AD   (MultiLevelProblem& ml_prob);    //, unsigned level, const unsigned &levelMax, const bool &assembleMatrix );

double ComputeIntegral(MultiLevelProblem& ml_prob);



int main(int argc, char** args) {



  // init Petsc-MPI communicator
  FemusInit mpinit(argc, args, MPI_COMM_WORLD);

       // ======= Files ========================
  Files files; 
        files.CheckIODirectories();
	files.RedirectCout();
 
// define multilevel mesh
  MultiLevelMesh mlMsh;
  // read coarse level mesh and generate finers level meshes
  double scalingFactor = 1.;
 
 //Adimensional quantity (Lref,Uref)
  double Lref = 1.;
  double Uref = 1.;
 // *** apparently needed by non-AD assemble only **********************
  // add fluid material
  Parameter parameter(Lref,Uref);
  
  // Generate fluid Object (Adimensional quantities,viscosity,density,fluid-model)
  Fluid fluid(parameter,1,FLUID_DENSITY,"Newtonian");
  std::cout << "Fluid properties: " << std::endl;
  std::cout << fluid << std::endl;
  
// *************************
  

//   MultiLevelMesh mlMsh;
//  mlMsh.ReadCoarseMesh(infile.c_str(),"seventh",Lref);
    mlMsh.GenerateCoarseBoxMesh(NSUB_X,NSUB_Y,0,0.,1.,0.,1.,0.,0.,QUAD9,"seventh");
    
  /* "seventh" is the order of accuracy that is used in the gauss integration scheme
     probably in the furure it is not going to be an argument of this function   */
  unsigned dim = mlMsh.GetDimension();

  unsigned numberOfUniformLevels = 1; 
  unsigned numberOfSelectiveLevels = 0;
  mlMsh.RefineMesh(numberOfUniformLevels , numberOfUniformLevels + numberOfSelectiveLevels, NULL);

  // erase all the coarse mesh levels
  mlMsh.EraseCoarseLevels(numberOfUniformLevels - 1);

  // print mesh info
  mlMsh.PrintInfo();

  MultiLevelSolution mlSol(&mlMsh);

  // add variables to mlSol
  // state =====================  
  mlSol.AddSolution("U", LAGRANGE, SECOND);
  mlSol.AddSolution("V", LAGRANGE, SECOND);
  if (dim == 3) mlSol.AddSolution("W", LAGRANGE, SECOND);
  mlSol.AddSolution("P", LAGRANGE, FIRST);
  // adjoint =====================  
  mlSol.AddSolution("UADJ", LAGRANGE, SECOND);
  mlSol.AddSolution("VADJ", LAGRANGE, SECOND);
  if (dim == 3) mlSol.AddSolution("WADJ", LAGRANGE, SECOND);
  mlSol.AddSolution("PADJ", LAGRANGE, FIRST);
  // control =====================  
  mlSol.AddSolution("UCTRL", LAGRANGE, SECOND);
  mlSol.AddSolution("VCTRL", LAGRANGE, SECOND);
  if (dim == 3) mlSol.AddSolution("WCTRL", LAGRANGE, SECOND);
  mlSol.AddSolution("PCTRL", LAGRANGE, FIRST);
  // control ===================== 
  
  
 // define the multilevel problem attach the mlSol object to it
  MultiLevelProblem mlProb(&mlSol);
  
  mlSol.Initialize("All");

//   mlSol.Initialize("UCTRL", SetInitialCondition,&mlProb);
//   mlSol.Initialize("VCTRL", SetInitialCondition,&mlProb);
  
  // attach the boundary condition function and generate boundary data
  mlSol.AttachSetBoundaryConditionFunction(SetBoundaryConditionOpt);
  mlSol.GenerateBdc("All");
  
 
  mlProb.parameters.set<Fluid>("Fluid") = fluid;
  mlProb.SetFilesHandler(&files);

  // add system NSOptLifting in mlProb as a NonLinear Implicit System
  NonLinearImplicitSystem& system_opt    = mlProb.add_system < NonLinearImplicitSystem > ("NSOpt");
//   LinearImplicitSystem& system_opt    = mlProb.add_system < LinearImplicitSystem > ("NSOpt");

  // NS ===================
  system_opt.AddSolutionToSystemPDE("U");
  system_opt.AddSolutionToSystemPDE("V");
  if (dim == 3) system_opt.AddSolutionToSystemPDE("W");
  system_opt.AddSolutionToSystemPDE("P");
//   // NSADJ ===================
  system_opt.AddSolutionToSystemPDE("UADJ");
  system_opt.AddSolutionToSystemPDE("VADJ");
  if (dim == 3) system_opt.AddSolutionToSystemPDE("WADJ");
  system_opt.AddSolutionToSystemPDE("PADJ");
  // NSCTRL ===================
  system_opt.AddSolutionToSystemPDE("UCTRL");
  system_opt.AddSolutionToSystemPDE("VCTRL");
  if (dim == 3) system_opt.AddSolutionToSystemPDE("WCTRL");
  system_opt.AddSolutionToSystemPDE("PCTRL");
  
  
  
  
  // attach the assembling function to system
//   system_opt.SetAssembleFunction(AssembleNavierStokesOpt_AD);
  system_opt.SetAssembleFunction(AssembleNavierStokesOpt_nonAD);
    
  // initilaize and solve the system
  system_opt.init();
  system_opt.ClearVariablesToBeSolved();
  system_opt.AddVariableToBeSolved("All");

 mlSol.SetWriter(VTK);
 mlSol.GetWriter()->SetDebugOutput(true);
  
 
  system_opt.SetDebugNonlinear(true);
//   system_opt.SetMaxNumberOfNonLinearIterations(5);
//   system_opt.SetNonLinearConvergenceTolerance(1.e-15);
  system_opt.SetDebugLinear(true);
  system_opt.SetMaxNumberOfLinearIterations(6);
  system_opt.SetAbsoluteLinearConvergenceTolerance(1.e-14);

  system_opt.MLsolve();

  ComputeIntegral(mlProb);
  
  // print solutions
  std::vector < std::string > variablesToBePrinted;
  variablesToBePrinted.push_back("All");
 mlSol.GetWriter()->Write(files.GetOutputPath()/*DEFAULT_OUTPUTDIR*/,  "biquadratic", variablesToBePrinted);

  //Destroy all the new systems
  mlProb.clear();


  return 0;
}


void AssembleNavierStokesOpt_AD(MultiLevelProblem& ml_prob) {
  //  ml_prob is the global object from/to where get/set all the data
  //  level is the level of the PDE system to be assembled
  //  levelMax is the Maximum level of the MultiLevelProblem
  //  assembleMatrix is a flag that tells if only the residual or also the matrix should be assembled
std::cout << " ********************************  AD SYSTEM ******************************************** " << std::endl;
  // call the adept stack object
  adept::Stack& s = FemusInit::_adeptStack;

  //  extract pointers to the several objects that we are going to use
  NonLinearImplicitSystem& mlPdeSys   = ml_prob.get_system<NonLinearImplicitSystem> ("NSOpt");   // pointer to the nonlinear implicit system named "NSOpt" 
//   LinearImplicitSystem& mlPdeSys  = ml_prob.get_system<LinearImplicitSystem>("NSOpt");
   const unsigned level = mlPdeSys.GetLevelToAssemble();
 
  Mesh*          msh          	= ml_prob._ml_msh->GetLevel(level);    // pointer to the mesh (level) object
  elem*          el         	= msh->el;  // pointer to the elem object in msh (level)

  MultiLevelSolution*  mlSol    = ml_prob._ml_sol;  // pointer to the multilevel solution object
  Solution*    sol        	= ml_prob._ml_sol->GetSolutionLevel(level);    // pointer to the solution (level) object


   LinearEquationSolver*  pdeSys	 = mlPdeSys._LinSolver[level];   
 SparseMatrix*    JAC         	= pdeSys->_KK;  // pointer to the global stifness matrix object in pdeSys (level)
  NumericVector*   RES          = pdeSys->_RES; // pointer to the global residual vector object in pdeSys (level)

  unsigned    iproc = msh->processor_id(); // get the process_id (for parallel computation)
  
  const unsigned  dim = msh->GetDimension(); // get the domain dimension of the problem
  unsigned dim2 = (3 * (dim - 1) + !(dim - 1));        // dim2 is the number of second order partial derivatives (1,3,6 depending on the dimension)

  // reserve memory for the local standar vectors
  const unsigned maxSize = static_cast< unsigned >(ceil(pow(3, dim)));          // conservative: based on line3, quad9, hex27

  //geometry *******************************
  vector < vector < double > > coordX(dim);    // local coordinates

  unsigned coordXType = 2; // get the finite element type for "x", it is always 2 (LAGRANGE TENSOR-PRODUCT-QUADRATIC)

  for (unsigned  k = 0; k < dim; k++) { 
    coordX[k].reserve(maxSize);
  }
  //geometry *******************************

//STATE######################################################################
  //velocity *******************************
  vector < unsigned > solVIndex(dim);
  solVIndex[0] = mlSol->GetIndex("U");    // get the position of "U" in the ml_sol object
  solVIndex[1] = mlSol->GetIndex("V");    // get the position of "V" in the ml_sol object

  if (dim == 3) solVIndex[2] = mlSol->GetIndex("W");      // get the position of "W" in the ml_sol object

  unsigned solVType = mlSol->GetSolutionType(solVIndex[0]);    // get the finite element type for "u"
  vector < unsigned > solVPdeIndex(dim);
  solVPdeIndex[0] = mlPdeSys.GetSolPdeIndex("U");    // get the position of "U" in the pdeSys object
  solVPdeIndex[1] = mlPdeSys.GetSolPdeIndex("V");    // get the position of "V" in the pdeSys object

  if (dim == 3) solVPdeIndex[2] = mlPdeSys.GetSolPdeIndex("W");
  
  vector < vector < adept::adouble > >  solV(dim);    // local solution
   vector< vector < adept::adouble > > aResV(dim);    // local redidual vector
   
 for (unsigned  k = 0; k < dim; k++) {
    solV[k].reserve(maxSize);
    aResV[k].reserve(maxSize);
  }

  
  vector <double> phiV_gss;  // local test function
  vector <double> phiV_x_gss; // local test function first order partial derivatives
  vector <double> phiV_xx_gss; // local test function second order partial derivatives

  phiV_gss.reserve(maxSize);
  phiV_x_gss.reserve(maxSize * dim);
  phiV_xx_gss.reserve(maxSize * dim2);
  

  //velocity *******************************

  //pressure *******************************
  unsigned solPIndex;
  solPIndex = mlSol->GetIndex("P");    // get the position of "P" in the ml_sol object
  unsigned solPType = mlSol->GetSolutionType(solPIndex);    // get the finite element type for "P"

  unsigned solPPdeIndex;
  solPPdeIndex = mlPdeSys.GetSolPdeIndex("P");    // get the position of "P" in the pdeSys object

  vector < adept::adouble >  solP; // local solution
  vector< adept::adouble > aResP; // local redidual vector
  
  solP.reserve(maxSize);
  aResP.reserve(maxSize);
  
  double* phiP_gss;
  //pressure *******************************
//STATE######################################################################
  
//ADJOINT######################################################################
  //velocity *******************************
  vector < unsigned > solVadjIndex(dim);
  solVadjIndex[0] = mlSol->GetIndex("UADJ");    // get the position of "UADJ" in the ml_sol object
  solVadjIndex[1] = mlSol->GetIndex("VADJ");    // get the position of "VADJ" in the ml_sol object

  if (dim == 3) solVadjIndex[2] = mlSol->GetIndex("WADJ");      // get the position of "WADJ" in the ml_sol object

  unsigned solVadjType = mlSol->GetSolutionType(solVadjIndex[0]);    // get the finite element type for "uADJ"
 vector < unsigned > solVPdeadjIndex(dim);
  solVPdeadjIndex[0] = mlPdeSys.GetSolPdeIndex("UADJ");    // get the position of "UADJ" in the pdeSys object
  solVPdeadjIndex[1] = mlPdeSys.GetSolPdeIndex("VADJ");    // get the position of "VADJ" in the pdeSys object

  if (dim == 3) solVPdeadjIndex[2] = mlPdeSys.GetSolPdeIndex("WADJ");
  
  vector < vector < adept::adouble > >  solVadj(dim);    // local solution
   vector< vector < adept::adouble > > aResVadj(dim);    // local redidual vector
   
 for (unsigned  k = 0; k < dim; k++) {
    solVadj[k].reserve(maxSize);
    aResVadj[k].reserve(maxSize);
  }

  
  vector <double> phiVadj_gss;  // local test function
  vector <double> phiVadj_x_gss; // local test function first order partial derivatives
  vector <double> phiVadj_xx_gss; // local test function second order partial derivatives

  phiVadj_gss.reserve(maxSize);
  phiVadj_x_gss.reserve(maxSize * dim);
  phiVadj_xx_gss.reserve(maxSize * dim2);
  
  //velocity *******************************

  //pressure *******************************
  unsigned solPadjIndex;
  solPadjIndex = mlSol->GetIndex("PADJ");    // get the position of "PADJ" in the ml_sol object
  unsigned solPadjType = mlSol->GetSolutionType(solPadjIndex);    // get the finite element type for "PADJ"

  unsigned solPPdeadjIndex;
  solPPdeadjIndex = mlPdeSys.GetSolPdeIndex("PADJ");    // get the position of "PADJ" in the pdeSys object

  vector < adept::adouble >  solPadj; // local solution
  vector< adept::adouble > aResPadj; // local redidual vector
  
  solPadj.reserve(maxSize);
  aResPadj.reserve(maxSize);
  
  double* phiPadj_gss;
  //pressure *******************************
//ADJOINT######################################################################

  
//CONTROL######################################################################
  //velocity *******************************
  vector < unsigned > solVctrlIndex(dim);
  solVctrlIndex[0] = mlSol->GetIndex("UCTRL");    // get the position of "UCTRL" in the ml_sol object
  solVctrlIndex[1] = mlSol->GetIndex("VCTRL");    // get the position of "VCTRL" in the ml_sol object

  if (dim == 3) solVctrlIndex[2] = mlSol->GetIndex("WCTRL");      // get the position of "WCTRL" in the ml_sol object

  unsigned solVctrlType = mlSol->GetSolutionType(solVctrlIndex[0]);    // get the finite element type for "uCTRL"
 vector < unsigned > solVPdectrlIndex(dim);
  solVPdectrlIndex[0] = mlPdeSys.GetSolPdeIndex("UCTRL");    // get the position of "UCTRL" in the pdeSys object
  solVPdectrlIndex[1] = mlPdeSys.GetSolPdeIndex("VCTRL");    // get the position of "VCTRL" in the pdeSys object

  if (dim == 3) solVPdectrlIndex[2] = mlPdeSys.GetSolPdeIndex("WCTRL");
  
  vector < vector < adept::adouble > >  solVctrl(dim);    // local solution
   vector< vector < adept::adouble > > aResVctrl(dim);    // local redidual vector
   
 for (unsigned  k = 0; k < dim; k++) {
    solVctrl[k].reserve(maxSize);
    aResVctrl[k].reserve(maxSize);
  }

  
  vector <double> phiVctrl_gss;  // local test function
  vector <double> phiVctrl_x_gss; // local test function first order partial derivatives
  vector <double> phiVctrl_xx_gss; // local test function second order partial derivatives

  phiVctrl_gss.reserve(maxSize);
  phiVctrl_x_gss.reserve(maxSize * dim);
  phiVctrl_xx_gss.reserve(maxSize * dim2);
  

  //velocity *******************************

  //pressure *******************************
  unsigned solPctrlIndex;
  solPctrlIndex = mlSol->GetIndex("PCTRL");    // get the position of "PCTRL" in the ml_sol object
  unsigned solPctrlType = mlSol->GetSolutionType(solPctrlIndex);    // get the finite element type for "PCTRL"

  unsigned solPPdectrlIndex;
  solPPdectrlIndex = mlPdeSys.GetSolPdeIndex("PCTRL");    // get the position of "PCTRL" in the pdeSys object

  vector < adept::adouble >  solPctrl; // local solution
  vector< adept::adouble > aResPctrl; // local redidual vector
  
  solPctrl.reserve(maxSize);
  aResPctrl.reserve(maxSize);
  
  double* phiPctrl_gss;
  //pressure *******************************
//CONTROL######################################################################


  
  //Nondimensional values ******************
  double IRe 		= ml_prob.parameters.get<Fluid>("Fluid").get_IReynolds_number();
  //Nondimensional values ******************
  double weight; // gauss point weight
  
  
  vector< int > JACDof; // local to global pdeSys dofs
  JACDof.reserve(3 *(dim + 1) *maxSize);

  vector< double > Res; // local redidual vector
  Res.reserve(3 *(dim + 1) *maxSize);

  vector < double > Jac;
  Jac.reserve(3* (dim + 1) *maxSize * 3*(dim + 1) *maxSize);

  JAC->zero(); // Set to zero all the entries of the Global Matrix

  // element loop: each process loops only on the elements that owns
  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

// geometry
    short unsigned ielGeom = msh->GetElementType(iel);    // element geometry type // via mapping between paralell dof and mesh dof

    unsigned nDofsX = msh->GetElementDofNumber(iel, coordXType);    // number of coordinate element dofs
    
// equation
    unsigned nDofsV = msh->GetElementDofNumber(iel, solVType);    // number of solution element dofs
    unsigned nDofsP = msh->GetElementDofNumber(iel, solPType);    // number of solution element dofs
    unsigned nDofsVP = dim * nDofsV + nDofsP;
   
    
    unsigned nDofsVadj = msh->GetElementDofNumber(iel, solVadjType);    // number of solution element dofs
    unsigned nDofsPadj = msh->GetElementDofNumber(iel, solPadjType);    // number of solution element dofs

     unsigned nDofsVctrl = msh->GetElementDofNumber(iel, solVctrlType);    // number of solution element dofs
    unsigned nDofsPctrl = msh->GetElementDofNumber(iel, solPctrlType);    // number of solution element dofs
    
    
     unsigned nDofsVP_tot = 3*nDofsVP;
     
     
    for (unsigned  k = 0; k < dim; k++) {       coordX[k].resize(nDofsX);    }
     
    for (unsigned  k = 0; k < dim; k++)  {
      solV[k].resize(nDofsV);
      solVadj[k].resize(nDofsVadj);
      solVctrl[k].resize(nDofsVctrl);
    }
    solP.resize(nDofsP);
    solPadj.resize(nDofsPadj);
    solPctrl.resize(nDofsPctrl);
    

//element matrices and vectors
    // resize local arrays
    JACDof.resize(nDofsVP_tot);
    
//     Jac.resize(nDofsVP * nDofsVP);

    for (unsigned  k = 0; k < dim; k++) {
      aResV[k].resize(nDofsV);    //resize
      std::fill(aResV[k].begin(), aResV[k].end(), 0);    //set aRes to zero
    
      aResVadj[k].resize(nDofsVadj);    //resize
      std::fill(aResVadj[k].begin(), aResVadj[k].end(), 0);    //set aRes to zero
      
      aResVctrl[k].resize(nDofsVctrl);    //resize
      std::fill(aResVctrl[k].begin(), aResVctrl[k].end(), 0);    //set aRes to zero

    }

    aResP.resize(nDofsP);    //resize
    std::fill(aResP.begin(), aResP.end(), 0);    //set aRes to zero

     aResPadj.resize(nDofsPadj);    //resize
    std::fill(aResPadj.begin(), aResPadj.end(), 0);    //set aRes to zero
    
    aResPctrl.resize(nDofsPctrl);    //resize
    std::fill(aResPctrl.begin(), aResPctrl.end(), 0);    //set aRes to zero

    // geometry ************
    for (unsigned i = 0; i < nDofsX; i++) {
      unsigned coordXDof  = msh->GetSolutionDof(i, iel, coordXType);    // global to global mapping between coordinates node and coordinate dof // via local to global solution node

      for (unsigned k = 0; k < dim; k++) {
        coordX[k][i] = (*msh->_topology->_Sol[k])(coordXDof);      // global extraction and local storage for the element coordinates
      }
    }
    
     // elem average point 
    vector < double > elem_center(dim);   
    for (unsigned j = 0; j < dim; j++) {  elem_center[j] = 0.;  }
  for (unsigned j = 0; j < dim; j++) {  
      for (unsigned i = 0; i < nDofsX; i++) {
         elem_center[j] += coordX[j][i];
       }
    }
    
   for (unsigned j = 0; j < dim; j++) { elem_center[j] = elem_center[j]/nDofsX; }
  //*************************************** 
  
  //***** set target domain flag ********************************** 
   int target_flag = 0;
   target_flag = ElementTargetFlag(elem_center);
//***************************************   
    
    
   //STATE###################################################################  
    // velocity ************
    for (unsigned i = 0; i < nDofsV; i++) {
      unsigned solVDof = msh->GetSolutionDof(i, iel, solVType);    // global to global mapping between solution node and solution dof // via local to global solution node

      for (unsigned  k = 0; k < dim; k++) {
        solV[k][i] = (*sol->_Sol[solVIndex[k]])(solVDof);      // global extraction and local storage for the solution
        JACDof[i + k * nDofsV] = pdeSys->GetSystemDof(solVIndex[k], solVPdeIndex[k], i, iel);    // global to global mapping between solution node and pdeSys dof
      }
    }
    
    // pressure *************
    for (unsigned i = 0; i < nDofsP; i++) {
      unsigned solPDof = msh->GetSolutionDof(i, iel, solPType);    // global to global mapping between solution node and solution dof // via local to global solution node
      solP[i] = (*sol->_Sol[solPIndex])(solPDof);      // global extraction and local storage for the solution
      JACDof[i + dim * nDofsV] = pdeSys->GetSystemDof(solPIndex, solPPdeIndex, i, iel);    // global to global mapping between solution node and pdeSys dof
    }
//STATE###################################################################

//ADJ###################################################################
     // velocity ************
    for (unsigned i = 0; i < nDofsVadj; i++) {
      unsigned solVadjDof = msh->GetSolutionDof(i, iel, solVadjType);    // global to global mapping between solution node and solution dof // via local to global solution node

      for (unsigned  k = 0; k < dim; k++) {
        solVadj[k][i] = (*sol->_Sol[solVadjIndex[k]])(solVadjDof);      // global extraction and local storage for the solution
        JACDof[i + k * nDofsV +nDofsVP] = pdeSys->GetSystemDof(solVadjIndex[k], solVPdeadjIndex[k], i, iel);    // global to global mapping between solution node and pdeSys dof
      }
    }
    
    // pressure *************
    for (unsigned i = 0; i < nDofsPadj; i++) {
      unsigned solPadjDof = msh->GetSolutionDof(i, iel, solPadjType);    // global to global mapping between solution node and solution dof // via local to global solution node
      solPadj[i] = (*sol->_Sol[solPadjIndex])(solPadjDof);      // global extraction and local storage for the solution
      JACDof[i + dim * nDofsV +nDofsVP] = pdeSys->GetSystemDof(solPadjIndex, solPPdeadjIndex, i, iel);    // global to global mapping between solution node and pdeSys dof
    }
//ADJ###################################################################


//CTRL###################################################################
     // velocity ************
    for (unsigned i = 0; i < nDofsVctrl; i++) {
      unsigned solVctrlDof = msh->GetSolutionDof(i, iel, solVctrlType);    // global to global mapping between solution node and solution dof // via local to global solution node

      for (unsigned  k = 0; k < dim; k++) {
        solVctrl[k][i] = (*sol->_Sol[solVctrlIndex[k]])(solVctrlDof);      // global extraction and local storage for the solution
        JACDof[i + k * nDofsV + 2*nDofsVP] = pdeSys->GetSystemDof(solVctrlIndex[k], solVPdectrlIndex[k], i, iel);    // global to global mapping between solution node and pdeSys dof
      }
    }
    
    // pressure *************
    for (unsigned i = 0; i < nDofsPctrl; i++) {
      unsigned solPctrlDof = msh->GetSolutionDof(i, iel, solPctrlType);    // global to global mapping between solution node and solution dof // via local to global solution node
      solPctrl[i] = (*sol->_Sol[solPctrlIndex])(solPctrlDof);      // global extraction and local storage for the solution
      JACDof[i + dim * nDofsV + 2*nDofsVP] = pdeSys->GetSystemDof(solPctrlIndex, solPPdectrlIndex, i, iel);    // global to global mapping between solution node and pdeSys dof
    }
//CTRL###################################################################




      // start a new recording of all the operations involving adept::adouble variables
      s.new_recording();

      // *** Gauss point loop ***
      for (unsigned ig = 0; ig < msh->_finiteElement[ielGeom][solVType]->GetGaussPointNumber(); ig++) {

//STATE#############################################################################3	
        // *** get gauss point weight, test function and test function partial derivatives ***
        msh->_finiteElement[ielGeom][solVType]->Jacobian(coordX, ig, weight, phiV_gss, phiV_x_gss, phiV_xx_gss);
        phiP_gss = msh->_finiteElement[ielGeom][solPType]->GetPhi(ig);

	
        vector < adept::adouble > solV_gss(dim, 0);
        vector < vector < adept::adouble > > gradSolV_gss(dim);

        for (unsigned  k = 0; k < dim; k++) {
          gradSolV_gss[k].resize(dim);
          std::fill(gradSolV_gss[k].begin(), gradSolV_gss[k].end(), 0);
        }

        for (unsigned i = 0; i < nDofsV; i++) {
          for (unsigned  k = 0; k < dim; k++) {
            solV_gss[k] += phiV_gss[i] * solV[k][i];
          }

          for (unsigned j = 0; j < dim; j++) {
            for (unsigned  k = 0; k < dim; k++) {
              gradSolV_gss[k][j] += phiV_x_gss[i * dim + j] * solV[k][i];
            }
          }
        }

        adept::adouble solP_gss = 0;

        for (unsigned i = 0; i < nDofsP; i++) {
          solP_gss += phiP_gss[i] * solP[i];
        }

//STATE###############################################################################

//ADJOINT#############################################################################3	
        // *** get gauss point weight, test function and test function partial derivatives ***
        msh->_finiteElement[ielGeom][solVadjType]->Jacobian(coordX, ig, weight, phiVadj_gss, phiVadj_x_gss, phiVadj_xx_gss);
        phiPadj_gss = msh->_finiteElement[ielGeom][solPadjType]->GetPhi(ig);

	
        vector < adept::adouble > solVadj_gss(dim, 0);
        vector < vector < adept::adouble > > gradSolVadj_gss(dim);

        for (unsigned  k = 0; k < dim; k++) {
          gradSolVadj_gss[k].resize(dim);
          std::fill(gradSolVadj_gss[k].begin(), gradSolVadj_gss[k].end(), 0);
        }

        for (unsigned i = 0; i < nDofsVadj; i++) {
          for (unsigned  k = 0; k < dim; k++) {
            solVadj_gss[k] += phiVadj_gss[i] * solVadj[k][i];
          }

          for (unsigned j = 0; j < dim; j++) {
            for (unsigned  k = 0; k < dim; k++) {
              gradSolVadj_gss[k][j] += phiVadj_x_gss[i * dim + j] * solVadj[k][i];
            }
          }
        }

        adept::adouble solPadj_gss = 0;

        for (unsigned i = 0; i < nDofsPadj; i++) {
          solPadj_gss += phiPadj_gss[i] * solPadj[i];
        }

//ADJOINT###############################################################################



//CONTROL#############################################################################3	
        // *** get gauss point weight, test function and test function partial derivatives ***
        msh->_finiteElement[ielGeom][solVctrlType]->Jacobian(coordX, ig, weight, phiVctrl_gss, phiVctrl_x_gss, phiVctrl_xx_gss);
        phiPctrl_gss = msh->_finiteElement[ielGeom][solPctrlType]->GetPhi(ig);

	
        vector < adept::adouble > solVctrl_gss(dim, 0);
        vector < vector < adept::adouble > > gradSolVctrl_gss(dim);

        for (unsigned  k = 0; k < dim; k++) {
          gradSolVctrl_gss[k].resize(dim);
          std::fill(gradSolVctrl_gss[k].begin(), gradSolVctrl_gss[k].end(), 0);
        }

        for (unsigned i = 0; i < nDofsVctrl; i++) {
          for (unsigned  k = 0; k < dim; k++) {
            solVctrl_gss[k] += phiVctrl_gss[i] * solVctrl[k][i];
          }

          for (unsigned j = 0; j < dim; j++) {
            for (unsigned  k = 0; k < dim; k++) {
              gradSolVctrl_gss[k][j] += phiVctrl_x_gss[i * dim + j] * solVctrl[k][i];
            }
          }
        }

        adept::adouble solPctrl_gss = 0;

        for (unsigned i = 0; i < nDofsPctrl; i++) {
          solPctrl_gss += phiPctrl_gss[i] * solPctrl[i];
        }

//CONTROL###############################################################################

        // *** phiV_i loop ***
        for (unsigned i = 0; i < nDofsV; i++) {
          vector < adept::adouble > NSV_gss(dim, 0.);
	  vector < adept::adouble > NSVadj_gss(dim, 0.);
	  vector < adept::adouble > NSVctrl_gss(dim, 0.);
	  
          for (unsigned  kdim = 0; kdim < dim; kdim++) {
	      
          for (unsigned jdim = 0; jdim < dim; jdim++) { //focus on single partial derivative
	    
          
	      NSV_gss[kdim]   	 	+=  IRe*phiV_x_gss[i * dim + jdim]*gradSolV_gss[kdim][jdim]; 
						    // /*deformation_tensor*//*IRe * phiV_x_gss[i * dim + jdim] * 
						    // (gradSolV_gss[kdim][jdim] + gradSolV_gss[jdim][kdim])*/;  //diffusion
	      NSV_gss[kdim]  		+=  advection_flag * phiV_gss[i] * (solV_gss[jdim] * gradSolV_gss[kdim][jdim]);                                     //advection (u_hat . \nabla) u_hat
	      NSV_gss[kdim] 		+=  advection_flag * phiV_gss[i] * (solV_gss[jdim] * gradSolVctrl_gss[kdim][jdim]);                                  //advection (u_hat . \nabla) u_0
	      NSV_gss[kdim] 		+=  advection_flag * phiV_gss[i] * (solVctrl_gss[jdim] * gradSolV_gss[kdim][jdim]);                                  //advection (u_0 . \nabla) u_hat 
	      NSV_gss[kdim] 		+=  advection_flag * phiV_gss[i] * (solVctrl_gss[jdim] * gradSolVctrl_gss[kdim][jdim]);                              //advection (u_0 . \nabla) u_0
	      NSV_gss[kdim] 		+= IRe*phiV_x_gss[i * dim + jdim]*gradSolVctrl_gss[kdim][jdim];	 //delta_state-control
	      
	      NSVadj_gss[kdim]   	+=  IRe*phiVadj_x_gss[i * dim + jdim]*gradSolVadj_gss[kdim][jdim];  
	      NSVadj_gss[kdim]		+=   advection_flag * phiVadj_gss[i] * gradSolV_gss[jdim][kdim] * solVadj_gss[jdim];           // c(delta u,u,lambda)
	      NSVadj_gss[kdim]		+=   advection_flag * solV_gss[jdim] * phiVadj_x_gss[i * dim + jdim] * solVadj_gss[kdim] ;     // c(u,delta u, lambda)
	      NSVadj_gss[kdim]		+=   advection_flag * phiVadj_gss[i] * gradSolVctrl_gss[jdim][kdim] * solVadj_gss[jdim];       // c(delta u,u0,lambda)
	      NSVadj_gss[kdim]		+=   advection_flag * solVctrl_gss[jdim] * phiVadj_x_gss[i * dim + jdim] * solVadj_gss[kdim] ;       // c(u0,delta u,lambda)
	      
	      NSVctrl_gss[kdim]   	+=   gamma_val * phiVctrl_x_gss[i * dim + jdim] * gradSolVctrl_gss[kdim][jdim];
	      NSVctrl_gss[kdim]		+= -  advection_flag * solV_gss[jdim] * phiVctrl_x_gss[i * dim + jdim] * solVadj_gss[kdim] ;     // c(u,delta u0, lambda)
	      NSVctrl_gss[kdim]		+= -  advection_flag * phiVctrl_gss[i] * gradSolV_gss[jdim][kdim] * solVadj_gss[jdim];           // c(delta u0,u,lambda)
	      NSVctrl_gss[kdim]		+= -  advection_flag * phiVctrl_gss[i] * gradSolVctrl_gss[jdim][kdim] * solVadj_gss[jdim];       // c(delta u0,u0,lambda)
	      NSVctrl_gss[kdim]		+= -  advection_flag * solVctrl_gss[jdim] * phiVctrl_x_gss[i * dim + jdim] * solVadj_gss[kdim] ;       // c(u0,delta u0,lambda)
	      NSVctrl_gss[kdim] 	+=   - IRe*phiVctrl_x_gss[i * dim + jdim]*gradSolVadj_gss[kdim][jdim];  //nabla_delta_control-nabla_adjoint
						  
	  }  //jdim loop
	  
              NSV_gss[kdim]             += - force[kdim] * phiV_gss[i];
          
              NSVadj_gss[kdim]		+=  - alpha_val * target_flag * solV_gss[kdim]*phiVadj_gss[i]; //delta_adjoint-state
	      NSVadj_gss[kdim] 		+=  - alpha_val * target_flag * solVctrl_gss[kdim]*phiVadj_gss[i]; //delta_adjoint-control
	      NSVadj_gss[kdim] 		+=  + alpha_val* target_flag * Vel_desired[kdim] * phiVadj_gss[i];
	      NSVctrl_gss[kdim] 	+=    alpha_val * target_flag             * solV_gss[kdim]*phiVctrl_gss[i]; //delta_control-state
	      NSVctrl_gss[kdim]   	+=   (alpha_val * target_flag + beta_val) * solVctrl_gss[kdim] * phiVctrl_gss[i] ;
	      NSVctrl_gss[kdim]   	+=  - alpha_val* target_flag * Vel_desired[kdim] * phiVctrl_gss[i];
            
            //velocity-pressure block
          NSV_gss[kdim] 	+= - solP_gss * phiV_x_gss[i * dim + kdim];
	  NSVadj_gss[kdim] 	+= - solPadj_gss * phiVadj_x_gss[i * dim + kdim];
          NSVctrl_gss[kdim] 	+= - solPctrl_gss * phiVctrl_x_gss[i * dim + kdim];
	    
	  } //kdim loop


          for (unsigned  kdim = 0; kdim < dim; kdim++) { // - (b-Ax)
            aResV[kdim][i] 		+=   NSV_gss[kdim]     * weight;
	    aResVadj[kdim][i]   	+=   NSVadj_gss[kdim]  * weight;
            aResVctrl[kdim][i]    	+=   NSVctrl_gss[kdim] * weight;
							 	    
	  }
        } // end phiV_i loop

        // *** phiP_i loop ***
        for (unsigned i = 0; i < nDofsP; i++) {
          for (int kdim = 0; kdim < dim; kdim++) {
            aResP[i] 		+= - (gradSolV_gss[kdim][kdim]) * phiP_gss[i]  * weight;
	    aResPadj[i]  	+= - (gradSolVadj_gss[kdim][kdim]) * phiPadj_gss[i]  * weight;
	    aResPctrl[i]   	+= - (gradSolVctrl_gss[kdim][kdim]) * phiPctrl_gss[i]  * weight;
	    
          }
        } // end phiP_i loop
      } // end gauss point loop
      
              



    //copy the value of the adept::adoube aRes in double Res and store them in RES
    Res.resize(nDofsVP_tot);    //resize

    for (int i = 0; i < nDofsV; i++) {
      for (unsigned  kdim = 0; kdim < dim; kdim++) {
        Res[ i +  kdim * nDofsV ]            =  - aResV[kdim][i].value();
	Res[ i +  kdim * nDofsV + nDofsVP]   =  - aResVadj[kdim][i].value();
	Res[ i +  kdim * nDofsV + 2*nDofsVP] =  - aResVctrl[kdim][i].value();
      }
    }

    for (int i = 0; i < nDofsP; i++) {
      Res[ i + dim * nDofsV ]            = - aResP[i].value();
      Res[ i + dim * nDofsV + nDofsVP]   = - aResPadj[i].value();
      Res[ i + dim * nDofsV + 2*nDofsVP] = - aResPctrl[i].value();
    }
  

    RES->add_vector_blocked(Res, JACDof);

    //Extarct and store the Jacobian
       Jac.resize(nDofsVP_tot * nDofsVP_tot);

      // define the dependent variables
      for (unsigned  kdim = 0; kdim < dim; kdim++) { s.dependent(&aResV[kdim][0], nDofsV);}              s.dependent(&aResP[0], nDofsP);
      for (unsigned  kdim = 0; kdim < dim; kdim++) { s.dependent(&aResVadj[kdim][0], nDofsVadj); }       s.dependent(&aResPadj[0], nDofsPadj);
      for (unsigned  kdim = 0; kdim < dim; kdim++) { s.dependent(&aResVctrl[kdim][0], nDofsVctrl);  }    s.dependent(&aResPctrl[0], nDofsPctrl);
      
      // define the independent variables
      for (unsigned  kdim = 0; kdim < dim; kdim++) {  s.independent(&solV[kdim][0], nDofsV); }         s.independent(&solP[0], nDofsP);
      for (unsigned  kdim = 0; kdim < dim; kdim++) { s.independent(&solVadj[kdim][0], nDofsVadj); }    s.independent(&solPadj[0], nDofsPadj);
      for (unsigned  kdim = 0; kdim < dim; kdim++) { s.independent(&solVctrl[kdim][0], nDofsVctrl); }  s.independent(&solPctrl[0], nDofsPctrl);

      // get the and store jacobian matrix (row-major)
      s.jacobian(&Jac[0] , true);
      
      JAC->add_matrix_blocked(Jac, JACDof, JACDof);
 
      s.clear_independents();
      s.clear_dependents();
    
  } //end element loop for each process

  RES->close();
//   RES->print();

  JAC->close();

  // ***************** END ASSEMBLY *******************
}


double ComputeIntegral(MultiLevelProblem& ml_prob) {

//    NonLinearImplicitSystem* mlPdeSys   = &ml_prob.get_system<NonLinearImplicitSystem> ("NSOpt");   // pointer to the nonlinear implicit system named "NSOpt"
 
   LinearImplicitSystem* mlPdeSys   = &ml_prob.get_system<LinearImplicitSystem> ("NSOpt");   // pointer to the linear implicit system named "NSOpt"
  const unsigned level = mlPdeSys->GetLevelToAssemble();
 

  Mesh*          msh          	= ml_prob._ml_msh->GetLevel(level);    // pointer to the mesh (level) object
  elem*          el         	= msh->el;  // pointer to the elem object in msh (level)

  MultiLevelSolution*  mlSol    = ml_prob._ml_sol;  // pointer to the multilevel solution object
  Solution*    sol        	= ml_prob._ml_sol->GetSolutionLevel(level);    // pointer to the solution (level) object
  
  unsigned    iproc = msh->processor_id(); // get the process_id (for parallel computation)
  
  const unsigned  dim = msh->GetDimension(); // get the domain dimension of the problem
  unsigned dim2 = (3 * (dim - 1) + !(dim - 1));        // dim2 is the number of second order partial derivatives (1,3,6 depending on the dimension)

  // reserve memory for the local standar vectors
  const unsigned maxSize = static_cast< unsigned >(ceil(pow(3, dim)));          // conservative: based on line3, quad9, hex27

  //geometry *******************************
  vector < vector < double > > coordX(dim);    // local coordinates

  unsigned coordXType = 2; // get the finite element type for "x", it is always 2 (LAGRANGE TENSOR-PRODUCT-QUADRATIC)

  for (unsigned  k = 0; k < dim; k++) { 
    coordX[k].reserve(maxSize);
  }
  
  double weight;
  
  
  //geometry *******************************

//STATE######################################################################
  vector < unsigned > solVIndex(dim);
  solVIndex[0] = mlSol->GetIndex("U");    // get the position of "U" in the ml_sol object
  solVIndex[1] = mlSol->GetIndex("V");    // get the position of "V" in the ml_sol object

  if (dim == 3) solVIndex[2] = mlSol->GetIndex("W");      // get the position of "V" in the ml_sol object

  unsigned solVType = mlSol->GetSolutionType(solVIndex[0]);    // get the finite element type for "u"
  
  vector < vector < double > >  solV(dim);    // local solution
  vector <double >  V_gss(dim, 0.);    //  solution
   
 for (unsigned  k = 0; k < dim; k++) {
    solV[k].reserve(maxSize);
  }

  
  vector <double> phiV_gss;  // local test function
  vector <double> phiV_x_gss; // local test function first order partial derivatives
  vector <double> phiV_xx_gss; // local test function second order partial derivatives

  phiV_gss.reserve(maxSize);
  phiV_x_gss.reserve(maxSize * dim);
  phiV_xx_gss.reserve(maxSize * dim2);
  
//STATE######################################################################
  

//CONTROL######################################################################
  vector < unsigned > solVctrlIndex(dim);
  solVctrlIndex[0] = mlSol->GetIndex("UCTRL");    // get the position of "U" in the ml_sol object
  solVctrlIndex[1] = mlSol->GetIndex("VCTRL");    // get the position of "V" in the ml_sol object

  if (dim == 3) solVctrlIndex[2] = mlSol->GetIndex("WCTRL");      // get the position of "V" in the ml_sol object

  unsigned solVctrlType = mlSol->GetSolutionType(solVctrlIndex[0]);    // get the finite element type for "u"
  
  vector < vector < double > >  solVctrl(dim);    // local solution
  vector < double >   Vctrl_gss(dim, 0.);    //  solution
   
 for (unsigned  k = 0; k < dim; k++) {
    solVctrl[k].reserve(maxSize);
  }

  
  vector <double> phiVctrl_gss;  // local test function
  vector <double> phiVctrl_x_gss; // local test function first order partial derivatives
  vector <double> phiVctrl_xx_gss; // local test function second order partial derivatives

  phiVctrl_gss.reserve(maxSize);
  phiVctrl_x_gss.reserve(maxSize * dim);
  phiVctrl_xx_gss.reserve(maxSize * dim2);
  
//CONTROL######################################################################

// Vel_desired##################################################################
  vector <double> phiVdes_gss;  // local test function
  vector <double> phiVdes_x_gss; // local test function first order partial derivatives
  vector <double> phiVdes_xx_gss; // local test function second order partial derivatives

  phiVdes_gss.reserve(maxSize);
  phiVdes_x_gss.reserve(maxSize * dim);
  phiVdes_xx_gss.reserve(maxSize * dim2);

  vector <double>  solVdes(dim,0.);
  vector<double> Vdes_gss(dim, 0.);  
  
// Vel_desired##################################################################




double  integral_target_alpha = 0.;
double	integral_beta   = 0.;
double	integral_gamma  = 0.;
  
  // element loop: each process loops only on the elements that owns
  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

// geometry
    short unsigned ielGeom = msh->GetElementType(iel);    // element geometry type

    unsigned nDofsX = msh->GetElementDofNumber(iel, coordXType);    // number of coordinate element dofs
    
// equation
    unsigned nDofsV = msh->GetElementDofNumber(iel, solVType);    // number of solution element dofs
//     unsigned nDofsVdes = msh->GetElementDofNumber(iel, solVType);    // number of solution element dofs
    unsigned nDofsVctrl = msh->GetElementDofNumber(iel, solVctrlType);    // number of solution element dofs
    
    for (unsigned  k = 0; k < dim; k++) {       coordX[k].resize(nDofsX);    }
     
    for (unsigned  k = 0; k < dim; k++)  {
      solV[k].resize(nDofsV);
      solVctrl[k].resize(nDofsVctrl);
//       solVdes[k].resize(nDofsVdes);
    }


    // geometry ************
    for (unsigned i = 0; i < nDofsX; i++) {
      unsigned coordXDof  = msh->GetSolutionDof(i, iel, coordXType);    // global to global mapping between coordinates node and coordinate dof

      for (unsigned k = 0; k < dim; k++) {
        coordX[k][i] = (*msh->_topology->_Sol[k])(coordXDof);      // global extraction and local storage for the element coordinates
      }
    }
    
     // elem average point 
    vector < double > elem_center(dim);   
    for (unsigned j = 0; j < dim; j++) {  elem_center[j] = 0.;  }
  for (unsigned j = 0; j < dim; j++) {  
      for (unsigned i = 0; i < nDofsX; i++) {
         elem_center[j] += coordX[j][i];
       }
    }
    
   for (unsigned j = 0; j < dim; j++) { elem_center[j] = elem_center[j]/nDofsX; }
  //*************************************** 
  
  //***** set target domain flag ********************************** 
   int target_flag = 0;
   target_flag = ElementTargetFlag(elem_center);
//***************************************       
    
    
 //STATE###################################################################  
    // velocity ************
    for (unsigned i = 0; i < nDofsV; i++) {
      unsigned solVDof = msh->GetSolutionDof(i, iel, solVType);    // global to global mapping between solution node and solution dof

      for (unsigned  k = 0; k < dim; k++) {
        solV[k][i] = (*sol->_Sol[solVIndex[k]])(solVDof);      // global extraction and local storage for the solution
      }
    }
//STATE###################################################################

//CONTROL###################################################################  
    // velocity ************
    for (unsigned i = 0; i < nDofsV; i++) {
      unsigned solVctrlDof = msh->GetSolutionDof(i, iel, solVctrlType);    // global to global mapping between solution node and solution dof

      for (unsigned  k = 0; k < dim; k++) {
        solVctrl[k][i] = (*sol->_Sol[solVctrlIndex[k]])(solVctrlDof);      // global extraction and local storage for the solution
      }
    }
//CONTROL###################################################################




  //DESIRED VEL###################################################################  
    // velocity ************
//     for (unsigned i = 0; i < nDofsV; i++) {
//       unsigned solVdesDof = msh->GetSolutionDof(i, iel, solVType);    // global to global mapping between solution node and solution dof

      for (unsigned  k = 0; k < solVdes.size() /*dim*/; k++) {
        solVdes[k]/*[i]*/ = Vel_desired[k] /*(*sol->_Sol[solVIndex[k]])(solVdesDof)*/;      // global extraction and local storage for the solution
      }
//     }
 //DESIRED VEL###################################################################

      // *** Gauss point loop ***
      for (unsigned ig = 0; ig < msh->_finiteElement[ielGeom][solVType]->GetGaussPointNumber(); ig++) {

//STATE#############################################################################	
        // *** get gauss point weight, test function and test function partial derivatives ***
        msh->_finiteElement[ielGeom][solVType]->Jacobian(coordX, ig, weight, phiV_gss, phiV_x_gss, phiV_xx_gss);
	
	msh->_finiteElement[ielGeom][solVctrlType]->Jacobian(coordX, ig, weight, phiVctrl_gss, phiVctrl_x_gss, phiVctrl_xx_gss);

	msh->_finiteElement[ielGeom][solVType  /*solVdes*/]->Jacobian(coordX, ig, weight, phiVdes_gss, phiVdes_x_gss, phiVdes_xx_gss);

	  vector < vector < double > > gradVctrl_gss(dim);
      for (unsigned  k = 0; k < dim; k++) {
          gradVctrl_gss[k].resize(dim);
          std::fill(gradVctrl_gss[k].begin(), gradVctrl_gss[k].end(), 0);
        }
	
//     for (unsigned  k = 0; k < dim; k++) {
//       V_gss[k]       = 0.;
//       Vdes_gss[k]    = 0.;
//        Vctrl_gss[k]  = 0.;
//     }
    
      for (unsigned i = 0; i < nDofsV; i++) {
	 for (unsigned  k = 0; k < dim; k++) {
	   	V_gss[k] += solV[k][i] * phiV_gss[i];
		Vdes_gss[k] += solVdes[k]/*[i]*/ * phiVdes_gss[i];
		}
      }
	
      for (unsigned i = 0; i < nDofsVctrl; i++) {
	 for (unsigned  k = 0; k < dim; k++) {
	   Vctrl_gss[k] += solVctrl[k][i] * phiVctrl_gss[i];
	 }
     for (unsigned j = 0; j < dim; j++) {
            for (unsigned  k = 0; k < dim; k++) {
              gradVctrl_gss[k][j] += phiVctrl_x_gss[i * dim + j] * solVctrl[k][i];
            }
          }
      }
          
          
	
      for (unsigned  k = 0; k < dim; k++) {
	 integral_target_alpha +=target_flag* (V_gss[k] + Vctrl_gss[k] - Vdes_gss[k]) * (V_gss[k] + Vctrl_gss[k] - Vdes_gss[k])*weight;
	 integral_beta	+= ((Vctrl_gss[k])*(Vctrl_gss[k])*weight);
      }
      for (unsigned  k = 0; k < dim; k++) {
	for (unsigned  j = 0; j < dim; j++) {	
		integral_gamma	  += ((gradVctrl_gss[k][j])*(gradVctrl_gss[k][j])*weight);
	}
      }
   
  
      }// end gauss point loop
    } //end element loop  

    std::cout << "The value of the integral of target for alpha "<< std::setprecision(0)<< std::scientific<<  alpha_val<< " is " << std::setw(11) << std::setprecision(10) << std::fixed<< integral_target_alpha << std::endl;
    std::cout << "The value of the integral of beta for beta "<<  std::setprecision(0)<<std::scientific<<beta_val << " is " << std::setw(11) << std::setprecision(10) <<  std::fixed<< integral_beta << std::endl;
    std::cout << "The value of the integral of gamma for gamma "<< std::setprecision(0)<<std::scientific<<gamma_val<< " is " << std::setw(11) << std::setprecision(10) <<  std::fixed<< integral_gamma << std::endl; 
    std::cout << "The value of the total integral is " << std::setw(11) << std::setprecision(10) <<  integral_target_alpha *(alpha_val*0.5)+ integral_beta *(beta_val*0.5) + integral_gamma*(gamma_val*0.5) << std::endl; 
   
    
    return  integral_target_alpha *(alpha_val*0.5)+ integral_beta*(beta_val*0.5) + integral_gamma*(gamma_val*0.5) ; 
	  
  
}



void AssembleNavierStokesOpt_nonAD(MultiLevelProblem& ml_prob){
     
 std::cout << " ********************************  NON-AD SYSTEM ******************************************** " << std::endl;
 //pointers
  NonLinearImplicitSystem& mlPdeSys  = ml_prob.get_system<NonLinearImplicitSystem>("NSOpt");
//   LinearImplicitSystem& mlPdeSys  = ml_prob.get_system<LinearImplicitSystem>("NSOpt");
  const unsigned level = mlPdeSys.GetLevelToAssemble();

  bool assembleMatrix = mlPdeSys.GetAssembleMatrix(); 
   
  Solution*	 sol  	         = ml_prob._ml_sol->GetSolutionLevel(level);
  LinearEquationSolver*  pdeSys	 = mlPdeSys._LinSolver[level];   
  const char* pdename            = mlPdeSys.name().c_str();
  
  MultiLevelSolution* mlSol = ml_prob._ml_sol;
  
  Mesh*		 msh    = ml_prob._ml_msh->GetLevel(level);
  elem*		 el	= msh->el;
  SparseMatrix*	 JAC	= pdeSys->_KK;
  NumericVector* RES 	= pdeSys->_RES;
    
  //data
  const unsigned dim 	= msh->GetDimension();
  unsigned nel		= msh->GetNumberOfElements();
  unsigned igrid	= msh->GetLevel();
  unsigned iproc 	= msh->processor_id();
 
  const unsigned maxSize = static_cast< unsigned > (ceil(pow(3,dim)));

  // geometry *******************************************
  vector< vector < double> > coordX(dim);	//local coordinates
  unsigned coordXType = 2; // get the finite element type for "x", it is always 2 (LAGRANGE TENSOR-PRODUCT-QUADRATIC)
  for(int i=0;i<dim;i++) {   
       coordX[i].reserve(maxSize); 
  }
  // geometry *******************************************

 
  
  // solution variables *******************************************
  const int n_vars = dim+1;
  const int n_unknowns = 3*n_vars; //(2.*dim)+1; //state , adjoint of velocity terms and one pressure term
  const int vel_type_pos = 0;
  const int press_type_pos = dim;
  const int adj_vel_type_pos = vel_type_pos;
  const int state_pos_begin = 0;
  const int adj_pos_begin   = dim+1;
  const int ctrl_pos_begin   = 2*(dim+1);
  
  vector < std::string > Solname(n_unknowns);  // const char Solname[4][8] = {"U","V","W","P"};
  Solname              [state_pos_begin+0] =                "U";
  Solname              [state_pos_begin+1] =                "V";
  if (dim == 3) Solname[state_pos_begin+2] =                "W";
  Solname              [state_pos_begin + press_type_pos] = "P";
  
  Solname              [adj_pos_begin + 0] =              "UADJ";
  Solname              [adj_pos_begin + 1] =              "VADJ";
  if (dim == 3) Solname[adj_pos_begin + 2] =              "WADJ";
  Solname              [adj_pos_begin + press_type_pos] = "PADJ";

  Solname              [ctrl_pos_begin + 0] =              "UCTRL";
  Solname              [ctrl_pos_begin + 1] =              "VCTRL";
  if (dim == 3) Solname[ctrl_pos_begin + 2] =              "WCTRL";
  Solname              [ctrl_pos_begin + press_type_pos] = "PCTRL";
  
  vector < unsigned > SolPdeIndex(n_unknowns);
  vector < unsigned > SolIndex(n_unknowns);  
  vector < unsigned > SolFEType(n_unknowns);  


  for(unsigned ivar=0; ivar < n_unknowns; ivar++) {
    SolPdeIndex[ivar]	= mlPdeSys.GetSolPdeIndex(Solname[ivar].c_str());
    SolIndex[ivar]	= mlSol->GetIndex        (Solname[ivar].c_str());
    SolFEType[ivar]	= mlSol->GetSolutionType(SolIndex[ivar]);
  }

  vector < double > Sol_n_el_dofs(n_unknowns);
  
  //==========================================================================================
  // velocity ************************************
  //-----------state------------------------------
  vector < vector < double > > phi_gss_fe(NFE_FAMS);
  vector < vector < double > > phi_x_gss_fe(NFE_FAMS);
  vector < vector < double > > phi_xx_gss_fe(NFE_FAMS);
 
  for(int fe=0; fe < NFE_FAMS; fe++) {  
        phi_gss_fe[fe].reserve(maxSize);
      phi_x_gss_fe[fe].reserve(maxSize*dim);
     phi_xx_gss_fe[fe].reserve(maxSize*(3*(dim-1)));
   }
   
  vector < double > phiV_gss_bd;
  vector < double > phiV_x_gss_bd;
  phiV_gss_bd.reserve(maxSize);
  phiV_x_gss_bd.reserve(maxSize*dim);
  
  //=================================================================================================
  
  // quadratures ********************************
  double weight;
  
  // equation ***********************************
  vector < vector < int > > JACDof(n_unknowns); 
  vector < vector < double > > Res(n_unknowns); /*was F*/
  vector < vector < vector < double > > > Jac(n_unknowns); /*was B*/
 
  for(int i = 0; i < n_unknowns; i++) {     
    JACDof[i].reserve(maxSize);
      Res[i].reserve(maxSize);
  }
   
  if(assembleMatrix){
    for(int i = 0; i < n_unknowns; i++) {
      Jac[i].resize(n_unknowns);    
      for(int j = 0; j < n_unknowns; j++) {
	Jac[i][j].reserve(maxSize*maxSize);	
      }
    }
  }
  
  //----------- dofs ------------------------------
  vector < vector < double > > SolVAR_eldofs(n_unknowns);
  vector < vector < double > > gradSolVAR_eldofs(n_unknowns);
  
  for(int k=0; k<n_unknowns; k++) {
    SolVAR_eldofs[k].reserve(maxSize);
    gradSolVAR_eldofs[k].reserve(maxSize*dim);    
  }

  //------------ at quadrature points ---------------------
  vector < double > SolVAR_qp(n_unknowns);
    vector < vector < double > > gradSolVAR_qp(n_unknowns);
    for(int k=0; k<n_unknowns; k++) {  gradSolVAR_qp[k].resize(dim);  }
      
    
  double IRe = ml_prob.parameters.get<Fluid>("Fluid").get_IReynolds_number();

  // Set to zero all the global structures
   RES->zero();
    if(assembleMatrix) JAC->zero();
  
  // ****************** element loop *******************
 
  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

  // geometry *****************************
    short unsigned ielGeom = msh->GetElementType(iel);

   unsigned nDofsX = msh->GetElementDofNumber(iel, coordXType);    // number of coordinate element dofs
    
    for(int ivar=0; ivar<dim; ivar++) {
      coordX[ivar].resize(nDofsX);
    }
    
   for( unsigned i=0;i<nDofsX;i++) {
      unsigned coordXDof  = msh->GetSolutionDof(i, iel, coordXType);    // global to global mapping between coordinates node and coordinate dof // via local to global solution node
      for(unsigned ivar = 0; ivar < dim; ivar++) {
	coordX[ivar][i] = (*msh->_topology->_Sol[ivar])(coordXDof);
      }
    }

     // elem average point 
    vector < double > elem_center(dim);   
    for (unsigned j = 0; j < dim; j++) {  elem_center[j] = 0.;  }
  for (unsigned j = 0; j < dim; j++) {  
      for (unsigned i = 0; i < nDofsX; i++) {
         elem_center[j] += coordX[j][i];
       }
    }
    
   for (unsigned j = 0; j < dim; j++) { elem_center[j] = elem_center[j]/nDofsX; }
  //***************************************  
  
  // geometry end *****************************
  
  // equation *****************************
    unsigned nDofsV = msh->GetElementDofNumber(iel, SolFEType[vel_type_pos]);    // number of solution element dofs
    unsigned nDofsP = msh->GetElementDofNumber(iel, SolFEType[state_pos_begin + press_type_pos]);    // number of solution element dofs
    
    unsigned nDofsVadj = msh->GetElementDofNumber(iel,SolFEType[adj_pos_begin]);    // number of solution element dofs
    unsigned nDofsPadj = msh->GetElementDofNumber(iel,SolFEType[adj_pos_begin + press_type_pos]);    // number of solution element dofs

    unsigned nDofsVctrl = msh->GetElementDofNumber(iel,SolFEType[ctrl_pos_begin]);    // number of solution element dofs
    unsigned nDofsPctrl = msh->GetElementDofNumber(iel,SolFEType[ctrl_pos_begin + press_type_pos] );    // number of solution element dofs

    unsigned nDofsVP = dim * nDofsV + nDofsP;
    unsigned nDofsVP_tot = 3*nDofsVP;
  // equation end *****************************
  
  //***** set target domain flag ********************************** 
   int target_flag = 0;
   target_flag = ElementTargetFlag(elem_center);
   //***************************************       
  
   //STATE###################################################################  
  for (unsigned  k = 0; k < n_unknowns; k++) {
    unsigned ndofs_unk = msh->GetElementDofNumber(iel, SolFEType[k]);
	Sol_n_el_dofs[k]=ndofs_unk;
       SolVAR_eldofs[k].resize(ndofs_unk);
       JACDof[k].resize(ndofs_unk); 
    for (unsigned i = 0; i < ndofs_unk; i++) {
       unsigned solDof = msh->GetSolutionDof(i, iel, SolFEType[k]);    // global to global mapping between solution node and solution dof // via local to global solution node
       SolVAR_eldofs[k][i] = (*sol->_Sol[SolIndex[k]])(solDof);      // global extraction and local storage for the solution
       JACDof[k][i] = pdeSys->GetSystemDof(SolIndex[k], SolPdeIndex[k], i, iel);    // global to global mapping between solution node and pdeSys dof
      }
    }
  //CTRL###################################################################

       
    for(int ivar=0; ivar<n_unknowns; ivar++) {
      
      Res[SolPdeIndex[ivar]].resize(Sol_n_el_dofs[ivar]);
      memset(&Res[SolPdeIndex[ivar]][0],0.,Sol_n_el_dofs[ivar]*sizeof(double));
    }
   
    for(int ivar=0; ivar<n_unknowns; ivar++) {
      for(int jvar=0; jvar<n_unknowns; jvar++) {
      if(assembleMatrix){  //MISMATCH
	Jac[ SolPdeIndex[ivar] ][ SolPdeIndex[jvar] ].resize(Sol_n_el_dofs[ivar]*Sol_n_el_dofs[jvar]);
	memset(&Jac[SolPdeIndex[ivar]][SolPdeIndex[jvar]][0],0.,Sol_n_el_dofs[ivar]*Sol_n_el_dofs[jvar]*sizeof(double));
      }
    }
  }
  
    //=============================================================================

   
      // ********************** Gauss point loop *******************************
      for(unsigned ig=0;ig < ml_prob._ml_msh->_finiteElement[ielGeom][SolFEType[vel_type_pos]]->GetGaussPointNumber(); ig++) {
	
	// *** get Jacobian and test function and test function derivatives ***
      for(int fe=0; fe < NFE_FAMS; fe++) {
	ml_prob._ml_msh->_finiteElement[ielGeom][fe]->Jacobian(coordX,ig,weight,phi_gss_fe[fe],phi_x_gss_fe[fe],phi_xx_gss_fe[fe]);
      }
         //HAVE TO RECALL IT TO HAVE BIQUADRATIC JACOBIAN
  	ml_prob._ml_msh->_finiteElement[ielGeom][BIQUADR_FE]->Jacobian(coordX,ig,weight,phi_gss_fe[BIQUADR_FE],phi_x_gss_fe[BIQUADR_FE],phi_xx_gss_fe[BIQUADR_FE]);


 //begin unknowns eval at gauss points ********************************
	for(unsigned unk = 0; unk < /*n_vars*/ n_unknowns; unk++) {
	  SolVAR_qp[unk] = 0.;
	  for(unsigned ivar2=0; ivar2<dim; ivar2++){ 
	    gradSolVAR_qp[unk][ivar2] = 0.; 
	  }
	  
	  for(unsigned i = 0; i < Sol_n_el_dofs[unk]; i++) {
	    SolVAR_qp[unk] += phi_gss_fe[ SolFEType[unk] ][i] * SolVAR_eldofs[unk][i];
	    for(unsigned ivar2=0; ivar2<dim; ivar2++) {
	      gradSolVAR_qp[unk][ivar2] += phi_x_gss_fe[ SolFEType[unk] ][i*dim+ivar2] * SolVAR_eldofs[unk][i]; 
	    }
	  }
	  
	}  
 //end unknowns eval at gauss points ********************************
	
	
//  // I x = 5 test ********************************
// 	for(unsigned i_unk=dim; i_unk<n_unknowns; i_unk++) { 
// 	    for(unsigned i_dof=0; i_dof < Sol_n_el_dofs[i_unk]; i_dof++) {
// 		/*if ( i_unk!=0 && i_unk!=1 && i_unk!=2 && i_unk!=3 && i_unk!=4 && i_unk!=6 && i_unk!=7 )*/  Res[SolPdeIndex[i_unk]][i_dof] +=  (               0.* phi_gss_fe[SolFEType[i_unk]][i_dof] 
// 		                                    - SolVAR_qp[i_unk]*phi_gss_fe[SolFEType[i_unk]][i_dof] )*weight;
// 		  for(unsigned j_unk=dim; j_unk<n_unknowns; j_unk++) {
// 		  	for(unsigned j_dof=0; j_dof < Sol_n_el_dofs[j_unk]; j_dof++) {
// 			  
// 		              if (i_unk==j_unk /*&& i_unk!=0 && i_unk!=1 && i_unk!=2 && i_unk!=3 && i_unk!=4 && i_unk!=6 && i_unk!=7*/)   {
// 				Jac[ SolPdeIndex[i_unk] ][ SolPdeIndex[j_unk] ][ i_dof*Sol_n_el_dofs[i_unk] + j_dof ] += 
// 				        ( phi_gss_fe[SolFEType[i_unk]][i_dof]*phi_gss_fe[SolFEType[j_unk]][j_dof] )*weight;
// 			      }
// 			  
// 			} //j_dof
// 		  }  //j_unk
// 	    }  //i_dof
// 	}  //i_unk
//  // I x = 5 test ********************************
 
 
 
 
//============ delta_state row ============================================================================================

  for (unsigned i = 0; i < nDofsV; i++) {
// FIRST ROW
	for (unsigned  kdim = 0; kdim < dim; kdim++) { // velocity block row 
	              double lap_res_du_u 			= 0.; 
		      double lap_res_du_ctrl 			= 0.;
		      double adv_res_uold_nablauold 		= 0.;
		      double adv_res_uold_nablauctrlold 	= 0.;
		      double adv_res_uctrlold_nablauold 	= 0.;
		      double adv_res_uctrlold_nablauctrlold 	= 0.;
	      for (unsigned jdim = 0; jdim < dim; jdim++) {
		    lap_res_du_u  		 += gradSolVAR_qp[SolPdeIndex[kdim]][jdim]		    * phi_x_gss_fe[SolFEType[kdim]][i * dim + jdim];
		    lap_res_du_ctrl 		 += gradSolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]][jdim] * phi_x_gss_fe[SolFEType[kdim]][i * dim + jdim];
		   adv_res_uold_nablauold 	 += SolVAR_qp[SolPdeIndex[jdim]] 		  * gradSolVAR_qp[SolPdeIndex[kdim]][jdim]		    * phi_gss_fe[ SolFEType[kdim] ][i];
		  adv_res_uold_nablauctrlold 	 += SolVAR_qp[SolPdeIndex[jdim]] 		  * gradSolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]][jdim] * phi_gss_fe[ SolFEType[kdim] ][i];
		  adv_res_uctrlold_nablauold	 += SolVAR_qp[SolPdeIndex[jdim + ctrl_pos_begin]] * gradSolVAR_qp[SolPdeIndex[kdim]][jdim] 		    * phi_gss_fe[ SolFEType[kdim] ][i];
		  adv_res_uctrlold_nablauctrlold += SolVAR_qp[SolPdeIndex[jdim + ctrl_pos_begin]] * gradSolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]][jdim] * phi_gss_fe[ SolFEType[kdim] ][i];
	      }      
	      Res[kdim][i]   +=  (         + force[kdim] * phi_gss_fe[SolFEType[kdim]][i]
                                           - IRe*lap_res_du_u 
                                           - IRe*lap_res_du_ctrl
                                           - advection_flag * adv_res_uold_nablauold 
                                           - advection_flag * adv_res_uold_nablauctrlold
                                           - advection_flag * adv_res_uctrlold_nablauold
					   - advection_flag * adv_res_uctrlold_nablauctrlold
					   + SolVAR_qp[SolPdeIndex[press_type_pos]] * phi_x_gss_fe[SolFEType[kdim]][i * dim + kdim]) * weight; 
	}	    
//DIAG BLOCK delta_state - state--------------------------------------------------------------------------------
	for (unsigned j = 0; j < nDofsV; j++) {
		      double lap_jac_du_u = 0.;
		      vector < double > adv_uold_nablaunew(dim,0.);
		      vector < double > adv_uctrlold_nablaunew(dim,0.);
	      for (unsigned  kdim = 0; kdim < dim; kdim++) { 
		    lap_jac_du_u += phi_x_gss_fe[SolFEType[kdim]][i * dim + kdim]	* 	phi_x_gss_fe[SolFEType[kdim]][j * dim + kdim];
		for (unsigned  jdim = 0; jdim < dim; jdim++) { //diagonal blocks only
		    adv_uold_nablaunew[kdim] 	 += SolVAR_qp[SolPdeIndex[jdim]]		  * phi_x_gss_fe[ SolFEType[kdim] ][j * dim + jdim] * phi_gss_fe[ SolFEType[kdim] ][i];
		    adv_uctrlold_nablaunew[kdim] += SolVAR_qp[SolPdeIndex[jdim + ctrl_pos_begin]] * phi_x_gss_fe[ SolFEType[kdim] ][j * dim + jdim] * phi_gss_fe[ SolFEType[kdim] ][i];
                }  //jdim
	      }
	      for (unsigned  kdim = 0; kdim < dim; kdim++) { 
		Jac[kdim][kdim][i*nDofsV + j] += (  + IRe*lap_jac_du_u 
						    + advection_flag * phi_gss_fe[ SolFEType[kdim] ][j] * gradSolVAR_qp[SolPdeIndex[kdim]][kdim] 		  * phi_gss_fe[ SolFEType[kdim] ][i]	 // c(u_hat_new,u_hat_old,delta_lambda) diagonal blocks  ..... unew_nablauold
						    + advection_flag * adv_uold_nablaunew[kdim] 													 // c(u_hat_old, u_hat_new, delta_lambda)
						    + advection_flag * phi_gss_fe[ SolFEType[kdim] ][j] * gradSolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]][kdim] * phi_gss_fe[ SolFEType[kdim] ][i]	 // c(u_hat_new,u_0_old,delta_lambda) diagonal blocks  ..... unew_nablauctrlold
						    + advection_flag * adv_uctrlold_nablaunew[kdim] 												  	 // c(u_0_old, u_hat_new, delta_lambda)
						    ) * weight; 
						    
               unsigned int off_kdim = (kdim+1)%dim; //off-diagonal blocks
		Jac[kdim][off_kdim][i*nDofsV + j] += (+	advection_flag * phi_gss_fe[ SolFEType[off_kdim] ][j] * gradSolVAR_qp[SolPdeIndex[kdim]][off_kdim] 		    * phi_gss_fe[ SolFEType[kdim] ][i]	// c(u_hat_new,u_hat_old,delta_lambda) off-diagonal blocks  ..... unew_nablauold
						      + advection_flag * phi_gss_fe[ SolFEType[off_kdim] ][j] * gradSolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]][off_kdim] * phi_gss_fe[ SolFEType[kdim] ][i]	// c(u_hat_new,u_0_old,delta_lambda) off-diagonal blocks  ..... unew_nablauctrlold
						      ) * weight;
	      }
	} //j_du_u loop

//BLOCK delta_state - control------------------------------------------------------------------------------------
	for (unsigned j = 0; j < nDofsVctrl; j++) {
		      double lap_jac_du_ctrl = 0.;
		      vector < double > adv_uold_nablauctrlnew(dim,0.);
		      vector < double > adv_uctrlold_nablauctrlnew(dim,0.);
	      for (unsigned  kdim = 0; kdim < dim; kdim++) { 
		    lap_jac_du_ctrl += phi_x_gss_fe[SolFEType[kdim]][i * dim + kdim] * phi_x_gss_fe[SolFEType[kdim + ctrl_pos_begin]][j * dim + kdim];
		for (unsigned  jdim = 0; jdim < dim; jdim++) {  //diagonal blocks only
		    adv_uold_nablauctrlnew[kdim]	 += SolVAR_qp[SolPdeIndex[jdim]] 		  * phi_x_gss_fe[ SolFEType[kdim +  ctrl_pos_begin] ][j * dim + jdim] * phi_gss_fe[ SolFEType[kdim] ][i];
		    adv_uctrlold_nablauctrlnew[kdim] 	 += SolVAR_qp[SolPdeIndex[jdim + ctrl_pos_begin]] * phi_x_gss_fe[ SolFEType[kdim +  ctrl_pos_begin] ][j * dim + jdim] * phi_gss_fe[ SolFEType[kdim] ][i];
                }  //jdim
	      }
	      for (unsigned  kdim = 0; kdim < dim; kdim++) { 
		Jac[kdim][kdim + ctrl_pos_begin ][i*nDofsVctrl + j] += (+ IRe * lap_jac_du_ctrl 
									+ advection_flag * adv_uold_nablauctrlnew[kdim]														 		 // c(u_hat_old, u_0_new, delta_lambda)
									+ advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][j] * gradSolVAR_qp[SolPdeIndex[kdim]][kdim]		       * phi_gss_fe[ SolFEType[kdim] ][i]	 // c(u_0_new,u_hat_old,delta_lambda) diagonal blocks  ..... uctrlnew_nablauold
									+ advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][j] * gradSolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]][kdim] * phi_gss_fe[ SolFEType[kdim] ][i]	 // c(u_0_new,u_0_old,delta_lambda) diagonal blocks  ..... uctrlnew_nablauctrlold
									+ advection_flag * adv_uctrlold_nablauctrlnew[kdim]															 // c(u_0_old, u_0_new, delta_lambda)
									) * weight;
									
               unsigned int off_kdim = (kdim+1)%dim; //off-diagonal blocks
		Jac[kdim][off_kdim + ctrl_pos_begin][i*nDofsVctrl + j] += (   +	advection_flag * phi_gss_fe[ SolFEType[off_kdim + ctrl_pos_begin] ][j] * gradSolVAR_qp[SolPdeIndex[kdim]][off_kdim]		     * phi_gss_fe[ SolFEType[kdim] ][i]	 // c(u_0_new,u_hat_old,delta_lambda) off-diagonal blocks  ..... uctrlnew_nablauold
									      + advection_flag * phi_gss_fe[ SolFEType[off_kdim + ctrl_pos_begin] ][j] * gradSolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]][off_kdim] * phi_gss_fe[ SolFEType[kdim] ][i]	 // c(u_0_new,u_0_old,delta_lambda) off-diagonal blocks  ..... uctrlnew_nablauctrlold
									  ) * weight;
	      }
	} //j_du_ctrl loop

     
//BLOCK Pressure
      for (unsigned j = 0; j < nDofsP; j++) {
	    for (unsigned  kdim = 0; kdim < dim; kdim++) {
	      Jac[kdim][press_type_pos][i*nDofsP + j] += -( phi_gss_fe[SolFEType[press_type_pos]][j] * phi_x_gss_fe[SolFEType[kdim]][i * dim + kdim] ) * weight;
	    }
      }//j_press loop
   }//i_state loop

//DIV_state
  for (unsigned i = 0; i < nDofsP; i++) {
		    double div_u_du_qp =0.;
      for (unsigned  kdim = 0; kdim < dim; kdim++) {
	      div_u_du_qp += gradSolVAR_qp[SolPdeIndex[kdim]][kdim] ;
      }
      Res[press_type_pos][i]  +=  ( (div_u_du_qp) * phi_gss_fe[SolFEType[press_type_pos]][i] ) * weight;
      for (unsigned j = 0; j < nDofsV; j++) {
	  for (unsigned  kdim = 0; kdim < dim; kdim++) {
	      Jac[press_type_pos][kdim][i*nDofsV + j] += -( phi_gss_fe[SolFEType[press_type_pos]][i] * phi_x_gss_fe[SolFEType[kdim]][j * dim + kdim] ) * weight;
	  }
      } //j loop
   }//i_div_state
    //============ delta_state row ============================================================================================


    
//============ delta_adjoint row =============================================================================================
  
  for (unsigned i = 0; i < nDofsVadj; i++) {
// SECOND ROW
     for (unsigned kdim = 0; kdim < dim; kdim++) { 
		    double lap_res_dadj_adj 			= 0.;
		    double adv_res_phiadj_nablauold_uadjold 	= 0.;
		    double adv_res_uold_nablaphiadj_uadjold 	= 0.;
		    double adv_res_phiadj_nablauctrlold_uadjold = 0.;
		    double adv_res_uctrlold_nablaphiadj_uadjold = 0.;
	   for (unsigned jdim = 0; jdim < dim; jdim++) {
		lap_res_dadj_adj              	     += gradSolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]][jdim]  *  phi_x_gss_fe[SolFEType[kdim + adj_pos_begin]][i * dim + jdim];
		adv_res_phiadj_nablauold_uadjold     += phi_gss_fe[SolFEType[kdim + adj_pos_begin]][i] * gradSolVAR_qp[SolPdeIndex[jdim]][kdim] 			* SolVAR_qp[SolPdeIndex[jdim + adj_pos_begin]];
		adv_res_uold_nablaphiadj_uadjold     += SolVAR_qp[SolPdeIndex[jdim]]		       * phi_x_gss_fe[SolFEType[kdim + adj_pos_begin]][i * dim + jdim]  * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]];
		adv_res_phiadj_nablauctrlold_uadjold += phi_gss_fe[SolFEType[kdim + adj_pos_begin]][i] * gradSolVAR_qp[SolPdeIndex[jdim  + ctrl_pos_begin]][kdim]	* SolVAR_qp[SolPdeIndex[jdim + adj_pos_begin]];
		adv_res_uctrlold_nablaphiadj_uadjold += SolVAR_qp[SolPdeIndex[jdim + ctrl_pos_begin]]  * phi_x_gss_fe[SolFEType[kdim + adj_pos_begin]][i * dim + jdim]  * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]];
	   }
	  Res[kdim + adj_pos_begin][i] += ( - alpha_val * target_flag * Vel_desired[kdim] 			      * phi_gss_fe[SolFEType[kdim + adj_pos_begin]][i]
					    + alpha_val * target_flag * SolVAR_qp[SolPdeIndex[kdim]] 		      * phi_gss_fe[SolFEType[kdim + adj_pos_begin]][i]
					    + alpha_val * target_flag * SolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]] * phi_gss_fe[SolFEType[kdim + adj_pos_begin]][i]
					    - IRe * lap_res_dadj_adj
					    - advection_flag * adv_res_phiadj_nablauold_uadjold
					    - advection_flag * adv_res_uold_nablaphiadj_uadjold
					    - advection_flag * adv_res_phiadj_nablauctrlold_uadjold
					    - advection_flag * adv_res_uctrlold_nablaphiadj_uadjold
					    + SolVAR_qp[SolPdeIndex[press_type_pos + adj_pos_begin]] * phi_x_gss_fe[SolFEType[kdim + adj_pos_begin]][i * dim + kdim]) * weight;
      }
      
//BLOCK delta_adjoint - state------------------------------------------------------------------------------------------
     for (unsigned j = 0; j < nDofsV; j++) {
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
	      Jac[kdim + adj_pos_begin][kdim][i*nDofsV + j] += ( - alpha_val * target_flag * phi_gss_fe[SolFEType[kdim + adj_pos_begin]][i] * phi_gss_fe[SolFEType[kdim]][j] 
								 + advection_flag * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i]    * phi_x_gss_fe[ SolFEType[kdim] ][j*dim + kdim] 		* SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]   //c(delta_u, u_hat_new, lambda_old)  diagonal blocks  ......phiadj_nablaunew_uadjold 
								 + advection_flag * phi_gss_fe[ SolFEType[kdim] ][j] 			* phi_x_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i*dim + kdim] * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]	 //c(u_hat_new, delta_u, lambda_old) diagonal blocks  ......unew_nablaphiadj_uadjold
								     ) * weight;
               unsigned int off_kdim = (kdim+1)%dim; //off-diagonal blocks
		Jac[kdim + adj_pos_begin][off_kdim][i*nDofsV + j] += (+ advection_flag * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i] * phi_x_gss_fe[ SolFEType[off_kdim] ][j*dim + kdim]		      * SolVAR_qp[SolPdeIndex[off_kdim + adj_pos_begin]]   //c(delta_u, u_hat_new, lambda_old)  off-diagonal blocks  ......phiadj_nablaunew_uadjold 
								      + advection_flag * phi_gss_fe[ SolFEType[off_kdim] ][j] 		  * phi_x_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i*dim + off_kdim] * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]	   //c(u_hat_new, delta_u, lambda_old) off-diagonal blocks  ......unew_nablaphiadj_uadjold
								      ) * weight;
	    
	  }
     }//j_dadj_u loop

//BLOCK delta_adjoint - control-----------------------------------------------------------------------------------------
     for (unsigned j = 0; j < nDofsVctrl; j++) {
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
	     Jac[kdim + adj_pos_begin][kdim + ctrl_pos_begin][i*nDofsVctrl + j] += ( - alpha_val * target_flag * phi_gss_fe[SolFEType[kdim + adj_pos_begin]][i] * phi_gss_fe[SolFEType[kdim + ctrl_pos_begin]][j] 
										    + advection_flag * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i]  * phi_x_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][j*dim + kdim] * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]   //c(delta_u, u_0_new, lambda_old)  diagonal blocks  ......phiadj_nablauctrlnew_uadjold 
										    + advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][j] * phi_x_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i*dim + kdim]  * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]   //c(u_0_new, delta_u, lambda_old) diagonal blocks  ......uctrlnew_nablaphiadj_uadjold
										    ) * weight;
               unsigned int off_kdim = (kdim+1)%dim; //off-diagonal blocks
	     Jac[kdim + adj_pos_begin][off_kdim + ctrl_pos_begin][i*nDofsVctrl + j] += (+ advection_flag * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i]      * phi_x_gss_fe[ SolFEType[off_kdim + ctrl_pos_begin] ][j*dim + kdim] * SolVAR_qp[SolPdeIndex[off_kdim + adj_pos_begin]]   //c(delta_u, u_0_new, lambda_old)  off-diagonal blocks  ......phiadj_nablauctrlnew_uadjold 
											+ advection_flag * phi_gss_fe[ SolFEType[off_kdim + ctrl_pos_begin] ][j] * phi_x_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i*dim + off_kdim]  * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]	   //c(u_0_new, delta_u, lambda_old) off-diagonal blocks  ......uctrlnew_nablaphiadj_uadjold
											) * weight;
	    
	  }
     }//j_dadj_ctrl loop

//DIAG BLOCK delta_adjoint - adjoint---------------------------------------------------------------------------------
     for (unsigned j = 0; j < nDofsVadj; j++) {
		    double lap_jac_dadj_adj = 0.;
		    vector < double > adv_uold_nablaphiadj_uadjnew(dim, 0.);
		    vector < double > adv_uctrlold_nablaphiadj_uadjnew(dim, 0.);
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
		  lap_jac_dadj_adj                 +=  phi_x_gss_fe[SolFEType[kdim + adj_pos_begin]][i * dim + kdim] * phi_x_gss_fe[SolFEType[kdim + adj_pos_begin]][j * dim + kdim];
	   for (unsigned jdim = 0; jdim < dim; jdim++) { //diagonal blocks only
	     adv_uold_nablaphiadj_uadjnew[kdim]     += SolVAR_qp[SolPdeIndex[jdim]] 		     * phi_x_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i*dim + jdim] * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][j] ;
	     adv_uctrlold_nablaphiadj_uadjnew[kdim] += SolVAR_qp[SolPdeIndex[jdim + ctrl_pos_begin]] * phi_x_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i*dim + jdim] * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][j] ;
	   }
	  }
	  
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
	      Jac[kdim + adj_pos_begin][kdim + adj_pos_begin][i*nDofsVadj + j] += ( + IRe*lap_jac_dadj_adj 
										    + advection_flag * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i] * gradSolVAR_qp[SolPdeIndex[kdim]][kdim] 		  * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][j]   //c(delta_u, u_hat_old, lambda_new)  diagonal blocks  ......phiadj_nablauold_uadjnew  
										    + advection_flag * adv_uold_nablaphiadj_uadjnew[kdim] 	//c(u_hat_old, delta_uhat, lambda_new)
										    + advection_flag * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i] * gradSolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]][kdim] * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][j]   //c(delta_u, u_0_old, lambda_new)  diagonal blocks  ......phiadj_nablauctrlold_uadjnew  
										    + advection_flag * adv_uctrlold_nablaphiadj_uadjnew[kdim] 	//c(u_0_old, delta_uhat, lambda_new)
											) * weight;
               unsigned int off_kdim = (kdim+1)%dim; //off-diagonal blocks
		  Jac[kdim + adj_pos_begin][off_kdim + adj_pos_begin][i*nDofsVadj + j] += (+ advection_flag * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i] * gradSolVAR_qp[SolPdeIndex[off_kdim]][kdim]		    * phi_gss_fe[ SolFEType[off_kdim + adj_pos_begin] ][j]   //c(delta_u, u_hat_old, lambda_new)  off-diagonal blocks  ......phiadj_nablauold_uadjnew   
											   + advection_flag * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][i] * gradSolVAR_qp[SolPdeIndex[off_kdim + ctrl_pos_begin]][kdim] * phi_gss_fe[ SolFEType[off_kdim + adj_pos_begin] ][j]   //c(delta_u, u_0_old, lambda_new)  off-diagonal blocks  ......phiadj_nablauctrlold_uadjnew  
											  ) * weight;
	    
	  }
      } //j_dadj_adj loop
      
//BLOCK Pressure_adj
    for (unsigned j = 0; j < nDofsPadj; j++) {
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
	      Jac[kdim + adj_pos_begin][press_type_pos + adj_pos_begin][i*nDofsPadj + j] += - (    phi_gss_fe[SolFEType[press_type_pos + adj_pos_begin]][j] * phi_x_gss_fe[SolFEType[kdim + adj_pos_begin]][i * dim + kdim] ) * weight;
	  }
    }//j_press_adj loop
  }//i_adj loop

//DIV_adj
  for (unsigned i = 0; i < nDofsPadj; i++) {
		double div_adj_dadj_qp = 0.;
      for (unsigned kdim = 0; kdim < dim; kdim++) {
	    div_adj_dadj_qp += gradSolVAR_qp[SolPdeIndex[kdim + adj_pos_begin ]][kdim] ;
      }
      Res[press_type_pos + adj_pos_begin][i] += ( (div_adj_dadj_qp) * phi_gss_fe[ SolFEType[press_type_pos + adj_pos_begin] ][i] ) * weight;
      for (unsigned j = 0; j < nDofsVadj; j++) {
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
	    Jac[press_type_pos + adj_pos_begin][kdim + adj_pos_begin][i*nDofsVadj + j] += - (   phi_gss_fe[SolFEType[press_type_pos + adj_pos_begin]][i] * phi_x_gss_fe[SolFEType[kdim + adj_pos_begin]][j * dim + kdim] ) * weight;
	  }
      }//j loop
  }//i_div_adj

      //============ delta_adjoint row =============================================================================================


//============ delta_control row ==================================================================================================
// THIRD ROW
  for (unsigned i = 0; i < nDofsVctrl; i++) {
      for (unsigned kdim = 0; kdim < dim; kdim++) { 
		    double lap_res_dctrl_ctrl 			 = 0.;
		    double lap_res_dctrl_adj 			 = 0.;
		    double adv_res_phictrl_nablauold_uadjold 	 = 0.;
		    double adv_res_uold_nablaphictrl_uadjold 	 = 0.;
		    double adv_res_phictrl_nablauctrlold_uadjold = 0.;
		    double adv_res_uctrlold_nablaphictrl_uadjold = 0.;
     for (unsigned jdim = 0; jdim < dim; jdim++) {
		lap_res_dctrl_ctrl		      += gradSolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]][jdim] 	*   phi_x_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i * dim + jdim];
		lap_res_dctrl_adj 		      += gradSolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]][jdim] 	*   phi_x_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i * dim + jdim];
		adv_res_uold_nablaphictrl_uadjold     += SolVAR_qp[SolPdeIndex[jdim]]			 * phi_x_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i * dim + jdim]  * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]];
		adv_res_phictrl_nablauold_uadjold     += phi_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i] * gradSolVAR_qp[SolPdeIndex[jdim]][kdim]   			   * SolVAR_qp[SolPdeIndex[jdim + adj_pos_begin]];
		adv_res_phictrl_nablauctrlold_uadjold += phi_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i] * gradSolVAR_qp[SolPdeIndex[jdim  + ctrl_pos_begin]][kdim]	   * SolVAR_qp[SolPdeIndex[jdim + adj_pos_begin]];
		adv_res_uctrlold_nablaphictrl_uadjold += SolVAR_qp[SolPdeIndex[jdim + ctrl_pos_begin]]   * phi_x_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i * dim + jdim]  * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]];
      }
      
      Res[kdim + ctrl_pos_begin][i] += ( + alpha_val * target_flag * Vel_desired[kdim] * phi_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i]
					 - alpha_val * target_flag * SolVAR_qp[SolPdeIndex[kdim]] * phi_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i]
					 - alpha_val * target_flag * SolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]] * phi_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i]
					 - beta_val * SolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]] * phi_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i]
					 - gamma_val * lap_res_dctrl_ctrl
					 + IRe * lap_res_dctrl_adj
					+ advection_flag * adv_res_uold_nablaphictrl_uadjold
					+ advection_flag * adv_res_phictrl_nablauold_uadjold
					+ advection_flag * adv_res_phictrl_nablauctrlold_uadjold
					+ advection_flag * adv_res_uctrlold_nablaphictrl_uadjold				
					+ SolVAR_qp[SolPdeIndex[press_type_pos + ctrl_pos_begin]] * phi_x_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i * dim + kdim]) * weight;
      }

//BLOCK delta_control - state------------------------------------------------------------------------------------------------
      for (unsigned j = 0; j < nDofsV; j++) {
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
	      Jac[kdim + ctrl_pos_begin][kdim][i*nDofsV + j] += (+ alpha_val * target_flag * phi_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i] * phi_gss_fe[SolFEType[kdim]][j] 
								 - advection_flag * phi_gss_fe[ SolFEType[kdim] ][j] 		       * phi_x_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i*dim + kdim] * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]	 //c(u_hat_new, delta_u0, lambda_old) diagonal blocks  ......unew_nablaphictrl_uadjold
								 - advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i]  * phi_x_gss_fe[ SolFEType[kdim] ][j*dim + kdim] 			* SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]   //c(delta_u0, u_hat_new, lambda_old)  diagonal blocks  ......phiadj_nablaunew_uadjold 
								    ) * weight;
               unsigned int off_kdim = (kdim+1)%dim; //off-diagonal blocks
	      Jac[kdim + ctrl_pos_begin][off_kdim][i*nDofsV + j] += ( - advection_flag * phi_gss_fe[ SolFEType[off_kdim] ][j] 		   * phi_x_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i*dim + off_kdim] * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]	 //c(u_hat_new, delta_u0, lambda_old) off-diagonal blocks  ......unew_nablaphictrl_uadjold
								      - advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i] * phi_x_gss_fe[ SolFEType[off_kdim] ][j*dim + kdim] 			* SolVAR_qp[SolPdeIndex[off_kdim + adj_pos_begin]]   //c(delta_u0, u_hat_new, lambda_old)  off-diagonal blocks  ......phiadj_nablaunew_uadjold 
								    ) * weight;
	    
	  }
      }//j_dctrl_u loop
      
//BLOCK delta_control - adjoint------------------------------------------------------------------------------------------------
      for (unsigned j = 0; j < nDofsVadj; j++) {
		    double lap_jac_dctrl_adj = 0.;
		    vector < double > adv_uold_nablaphictrl_uadjnew(dim, 0.);
		    vector < double > adv_uctrlold_nablaphictrl_uadjnew(dim, 0.);
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
		lap_jac_dctrl_adj += phi_x_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i * dim + kdim]	*	phi_x_gss_fe[SolFEType[kdim + adj_pos_begin]][j * dim + kdim];
	   for (unsigned jdim = 0; jdim < dim; jdim++) { //diagonal blocks only
	     adv_uold_nablaphictrl_uadjnew[kdim]     += SolVAR_qp[SolPdeIndex[jdim]] 		     * phi_x_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i*dim + jdim] * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][j] ;
	     adv_uctrlold_nablaphictrl_uadjnew[kdim] += SolVAR_qp[SolPdeIndex[jdim + ctrl_pos_begin]]* phi_x_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i*dim + jdim] * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][j] ;
	   }
	  }
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
	      Jac[kdim + ctrl_pos_begin][kdim + adj_pos_begin][i*nDofsVadj + j] += ( - IRe * lap_jac_dctrl_adj 
										    - advection_flag * adv_uold_nablaphictrl_uadjnew[kdim] 	//c(u_hat_old, delta_u0, lambda_new)
										    - advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i] * gradSolVAR_qp[SolPdeIndex[kdim]][kdim] 		    * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][j]   //c(delta_u0, u_hat_old, lambda_new)  diagonal blocks  ......phictrl_nablauold_uadjnew  
										    - advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i] * gradSolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]][kdim]  * phi_gss_fe[ SolFEType[kdim + adj_pos_begin] ][j]   //c(delta_u0, u_0_old, lambda_new)  diagonal blocks  ......phictrl_nablauctrlold_uadjnew  
										    - advection_flag * adv_uctrlold_nablaphictrl_uadjnew[kdim] 	//c(u_0_old, delta_u0, lambda_new)
										      ) * weight;
               unsigned int off_kdim = (kdim+1)%dim; //off-diagonal blocks
	      Jac[kdim + ctrl_pos_begin][off_kdim + adj_pos_begin][i*nDofsVadj + j] += ( - advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i] * gradSolVAR_qp[SolPdeIndex[off_kdim]][kdim]  		     * phi_gss_fe[ SolFEType[off_kdim + adj_pos_begin] ][j]   //c(delta_u0, u_hat_old, lambda_new)  off-diagonal blocks  ......phictrl_nablauold_uadjnew  
										         - advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i] * gradSolVAR_qp[SolPdeIndex[off_kdim + ctrl_pos_begin]][kdim]  * phi_gss_fe[ SolFEType[off_kdim + adj_pos_begin] ][j]   //c(delta_u0, u_0_old, lambda_new)  off-diagonal blocks  ......phictrl_nablauctrlold_uadjnew  
										      ) * weight;
	    
	  }
      }//j_dctrl_adj loop

//DIAG BLOCK delta_control - control--------------------------------------------------------------------------------------
      for (unsigned j = 0; j < nDofsVctrl; j++) {
		      double lap_jac_dctrl_ctrl = 0.;
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
		lap_jac_dctrl_ctrl += phi_x_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i * dim + kdim]	*	phi_x_gss_fe[SolFEType[kdim + ctrl_pos_begin]][j * dim + kdim];
	  }
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
	      Jac[kdim + ctrl_pos_begin][kdim + ctrl_pos_begin][i*nDofsVctrl + j] += (  + (alpha_val * target_flag + beta_val) * phi_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i] * phi_gss_fe[SolFEType[kdim + ctrl_pos_begin]][j]
											+  gamma_val * lap_jac_dctrl_ctrl 
											- advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i]    * phi_x_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][j*dim + kdim] * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]   //c(delta_u0, u_0_new, lambda_old)  diagonal blocks  ......phictrl_nablauctrlnew_uadjold 
											- advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][j] 	* phi_x_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i*dim + kdim] * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]	 //c(u_0_new, delta_u0, lambda_old) diagonal blocks  ......uctrlnew_nablaphictrl_uadjold
											) * weight;
               unsigned int off_kdim = (kdim+1)%dim; //off-diagonal blocks
	      Jac[kdim + ctrl_pos_begin][off_kdim + ctrl_pos_begin][i*nDofsVctrl + j] += ( - advection_flag * phi_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i]     * phi_x_gss_fe[ SolFEType[off_kdim + ctrl_pos_begin] ][j*dim + kdim] * SolVAR_qp[SolPdeIndex[off_kdim + adj_pos_begin]]   //c(delta_u0, u_0_new, lambda_old)  off-diagonal blocks  ......phictrl_nablauctrlnew_uadjold 
											   - advection_flag * phi_gss_fe[ SolFEType[off_kdim + ctrl_pos_begin] ][j] * phi_x_gss_fe[ SolFEType[kdim + ctrl_pos_begin] ][i*dim + off_kdim] * SolVAR_qp[SolPdeIndex[kdim + adj_pos_begin]]	 //c(u_0_new, delta_u0, lambda_old) off-diagonal blocks  ......uctrlnew_nablaphictrl_uadjold
											) * weight;
	    
	  }
      }//j_dctrl_ctrl loop

//BLOCK Pressure_ctrl
      for (unsigned j = 0; j < nDofsPctrl; j++) {
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
	      Jac[kdim + ctrl_pos_begin][press_type_pos + ctrl_pos_begin][i*nDofsPctrl + j] += -( phi_gss_fe[SolFEType[press_type_pos + ctrl_pos_begin]][j] * phi_x_gss_fe[SolFEType[kdim + ctrl_pos_begin]][i * dim + kdim] ) * weight;
	  }
      }//j_press_ctrl
  }//i_ctrl loop

//DIV_ctrl
  for (unsigned i = 0; i < nDofsPctrl; i++) {
			  double div_ctrl_dctrl_qp = 0.;
	  for (unsigned kdim = 0; kdim < dim; kdim++) {
		div_ctrl_dctrl_qp += gradSolVAR_qp[SolPdeIndex[kdim + ctrl_pos_begin]][kdim] ;
	  }
	  Res[press_type_pos + ctrl_pos_begin][i] += ( (div_ctrl_dctrl_qp) * phi_gss_fe[SolFEType[press_type_pos + ctrl_pos_begin]][i] ) * weight;
	  for (unsigned j = 0; j < nDofsVctrl; j++) {
	      for (unsigned kdim = 0; kdim < dim; kdim++) {
		Jac[press_type_pos + ctrl_pos_begin][kdim + ctrl_pos_begin][i*nDofsVctrl + j] += - ( phi_gss_fe[SolFEType[press_type_pos + ctrl_pos_begin]][i] * phi_x_gss_fe[SolFEType[kdim + ctrl_pos_begin]][j * dim + kdim] ) * weight;
	      }
	  }//j loop
  }//i_div_ctrl
 
   //============ delta_control row ==================================================================================================
 
 
 
 
      }  // end gauss point loop
 

      //***************************************************************************************************************
      

    //Sum the local matrices/vectors into the Global Matrix/Vector
    for(unsigned i_unk=0; i_unk < n_unknowns; i_unk++) {
      RES->add_vector_blocked(Res[SolPdeIndex[i_unk]],JACDof[i_unk]);
        for(unsigned j_unk=0; j_unk < n_unknowns; j_unk++) {
	  if(assembleMatrix) JAC->add_matrix_blocked( Jac[ SolPdeIndex[i_unk] ][ SolPdeIndex[j_unk] ], JACDof[i_unk], JACDof[j_unk]);
        }
    }
 
   //--------------------------------------------------------------------------------------------------------  
  } //end list of elements loop for each subdomain
  
  
  JAC->close();
  RES->close();
  // ***************** END ASSEMBLY *******************
}
 
