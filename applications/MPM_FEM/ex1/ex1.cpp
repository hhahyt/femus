
#include "FemusInit.hpp"
#include "MultiLevelProblem.hpp"
#include "VTKWriter.hpp"
#include "TransientSystem.hpp"
#include "NonLinearImplicitSystem.hpp"
#include "Marker.hpp"
#include "Line.hpp"

#include "Fluid.hpp"
#include "Solid.hpp"
#include "Parameter.hpp"

#include "NumericVector.hpp"
#include "adept.h"

#include "../include/mpmFem.hpp"

using namespace femus;

// OLD BEST RESULT WITH E = 4.2 * 1.e6, 5 levels, dt= 0.01, NR = 300, R0 = 1.5, factor = 1.3
// MOST BEST RESULT WITH E = 4.2 * 1.e6, 4 levels, dt= 0.01, NR = 300, R0 = 1.4, factor = 1.14,  beta = 0.3, Gamma = 0.5

double SetVariableTimeStep(const double time)
{
  double dt =  0.01;
  return dt;
}

bool SetBoundaryCondition(const std::vector < double >& x, const char name[], double& value, const int facename, const double time)
{
  bool test = 1; //dirichlet
  value = 0.;

  if(!strcmp(name, "DY")) {
    if(3 == facename || 4 == facename) {
      test = 0;
      value = 0;
    }
  }
  else if(!strcmp(name, "DX")) {
    if(/*1 == facename ||*/ 2 == facename) {
      test = 0;
      value = 0;
    }
  }

  return test;

}

int main(int argc, char** args)
{

  // init Petsc-MPI communicator
  FemusInit mpinit(argc, args, MPI_COMM_WORLD);

  MultiLevelMesh mlMsh;
  double scalingFactor = 1.;
  unsigned numberOfUniformLevels = 5; //for refinement in 3D
  //unsigned numberOfUniformLevels = 1;
  unsigned numberOfSelectiveLevels = 0;

  double Lref = 1.;
  double Uref = 1.;
  double rhos = 1000;
  double nu = 0.4;
  double E = 4.2 * 1.e8;
  
  beta = 0.3; //was 0.25 
  Gamma = 0.5;

  Parameter par(Lref, Uref);

  // Generate Solid Object
  Solid solid;
  solid = Solid(par, E, nu, rhos, "Neo-Hookean");

  mlMsh.ReadCoarseMesh("../input/inclined_plane_2D_bl.neu", "fifth", scalingFactor);
  mlMsh.RefineMesh(numberOfUniformLevels + numberOfSelectiveLevels, numberOfUniformLevels , NULL);

  mlMsh.EraseCoarseLevels(numberOfUniformLevels - 1);
  numberOfUniformLevels = 1;

  unsigned dim = mlMsh.GetDimension();

  MultiLevelSolution mlSol(&mlMsh);
  // add variables to mlSol
  mlSol.AddSolution("DX", LAGRANGE, SECOND, 2);
  if(dim > 1) mlSol.AddSolution("DY", LAGRANGE, SECOND, 2);
  if(dim > 2) mlSol.AddSolution("DZ", LAGRANGE, SECOND, 2);

  mlSol.AddSolution("M", LAGRANGE, SECOND, 2);
  mlSol.AddSolution("Mat", DISCONTINOUS_POLYNOMIAL, ZERO, 0, false);

  mlSol.Initialize("All");

  mlSol.AttachSetBoundaryConditionFunction(SetBoundaryCondition);

  // ******* Set boundary conditions *******
  mlSol.GenerateBdc("DX", "Steady");
  if(dim > 1) mlSol.GenerateBdc("DY", "Steady");
  if(dim > 2) mlSol.GenerateBdc("DZ", "Steady");
  mlSol.GenerateBdc("M", "Steady");

  MultiLevelProblem ml_prob(&mlSol);

  ml_prob.parameters.set<Solid> ("SolidMPM") = solid;
  ml_prob.parameters.set<Solid> ("SolidFEM") = solid;

  // ******* Add MPM system to the MultiLevel problem *******
  TransientNonlinearImplicitSystem& system = ml_prob.add_system < TransientNonlinearImplicitSystem > ("MPM_FEM");
  system.AddSolutionToSystemPDE("DX");
  if(dim > 1)system.AddSolutionToSystemPDE("DY");
  if(dim > 2) system.AddSolutionToSystemPDE("DZ");

  // ******* System MPM Assembly *******
  system.SetAssembleFunction(AssembleMPMSys);
  //system.SetAssembleFunction(AssembleFEM);
  // ******* set MG-Solver *******
  system.SetMgType(V_CYCLE);


  system.SetAbsoluteLinearConvergenceTolerance(1.0e-10);
  system.SetMaxNumberOfLinearIterations(1);
  system.SetNonLinearConvergenceTolerance(1.e-9);
  system.SetMaxNumberOfNonLinearIterations(20);

  system.SetNumberPreSmoothingStep(1);
  system.SetNumberPostSmoothingStep(1);

  // ******* Set Preconditioner *******
  system.SetMgSmoother(GMRES_SMOOTHER);

  system.init();

  // ******* Set Smoother *******
  system.SetSolverFineGrids(GMRES);

  system.SetPreconditionerFineGrids(ILU_PRECOND);

  system.SetTolerances(1.e-10, 1.e-15, 1.e+50, 40, 40);


  //BEGIN init particles
  unsigned size = 1;
  std::vector < std::vector < double > > x; // marker
  double yc = 0.15	;  // FOR E = 4.2 * 1.e8 --> 0.115 (for 3 refinements) 0.09 (for 4) and 0.05  (for 5, this one maybe to be changed) 
                     // FOR E = 4.2 * 1.e6 --> 0.1. (for 3 refinements) 0.075 (for 4) and 0.05  (for 5)
  
  x.resize(size);
  x[0].resize(dim, 0.);
  x[0][1] = yc;
 
  double R = 1.6;
  double R0 = 1.4; 
  
  double PI = acos(-1.);
  unsigned NR = 300;
  unsigned NL = NR / (2 * PI);
  double DL = R0 / NL;

  for(unsigned i = 0; i < NL; i++) {
    double  r = R0 - i * DL;
    unsigned Nr = static_cast <unsigned>(ceil(NR * r / R0));
    double dtheta = 2 * PI / Nr;
    unsigned sizeOld = x.size();
    x.resize(sizeOld + Nr);
    for(unsigned s = sizeOld; s < x.size(); s++) {
      x[s].resize(dim);
    }
    for(unsigned j = 0; j < Nr; j++) {
      x[sizeOld + j][0] = r * cos(j * dtheta);
      x[sizeOld + j][1] = yc + r * sin(j * dtheta);
    }
  }
  double MASS = PI * R0 * R0 * rhos;
  size = x.size();
  std::vector < double > mass(x.size(), MASS / x.size()); // uniform marker volume
  
  if( fabs(R-R0) > 1.0e-10 ) {
    
    double factor = 1.14; 
    unsigned NL = getNumberOfLayers((R-R0)/DL, factor);
    std::cout << NL <<std::endl;
      
    double  r = R0;
    for(unsigned i = 1; i <= NL; i++) {
      DL = DL / factor;
      r += DL;
      NR = static_cast <unsigned>(ceil (NR * factor) );
      double dtheta = 2 * PI / NR;
      unsigned sizeOld = x.size();
      x.resize(sizeOld + NR);
      for(unsigned s = sizeOld; s < x.size(); s++) {
        x[s].resize(dim);
      }
      for(unsigned j = 0; j < NR; j++) {
        x[sizeOld + j][0] = r * cos(j * dtheta);
        x[sizeOld + j][1] = yc + r * sin(j * dtheta);
      }
      mass.resize(x.size(), rhos * r * dtheta * DL);
    }
    size = x.size();
  }
  
  double totalMass = 0;
  for(unsigned i = 0; i < mass.size(); i++){
    totalMass += mass[i];
  }
  
  std::cout << totalMass<<" "<< rhos * PI * R * R << std::endl;
  
  //return 1;

  std::vector < MarkerType > markerType;
  markerType.resize(size);

  for(unsigned j = 0; j < size; j++) {
    markerType[j] = VOLUME;
  }

  std::vector < std::vector < std::vector < double > > > line(1);
  std::vector < std::vector < std::vector < double > > > line0(1);

  unsigned solType = 2;
  linea = new Line(x, mass, markerType, mlSol.GetLevel(numberOfUniformLevels - 1), solType);
      
  //linea->SetParticlesMass(MASS/rhos, rhos);
  //linea->ScaleParticleMass(scale);

  linea->GetLine(line0[0]);
  PrintLine(DEFAULT_OUTPUTDIR, line0, false, 0);
  linea->GetParticlesToGridMaterial();

  //END init particles 
  
  // ******* Print solution *******
  mlSol.SetWriter(VTK);

  std::vector<std::string> mov_vars;
  mov_vars.push_back("DX");
  mov_vars.push_back("DY");
  mov_vars.push_back("DZ");
  mlSol.GetWriter()->SetMovingMesh(mov_vars);

  std::vector<std::string> print_vars;
  print_vars.push_back("All");


  mlSol.GetWriter()->SetDebugOutput(true);
  mlSol.GetWriter()->Write(DEFAULT_OUTPUTDIR, "biquadratic", print_vars, 0);

  double theta = PI / 4;
  gravity[0] = 9.81 * sin(theta);
  gravity[1] = -9.81 * cos(theta);

  system.AttachGetTimeIntervalFunction(SetVariableTimeStep);
  unsigned n_timesteps = 350;
  for(unsigned time_step = 1; time_step <= n_timesteps; time_step++) {

    system.CopySolutionToOldSolution();

    system.MGsolve();

    // ******* Print solution *******
    mlSol.GetWriter()->Write(DEFAULT_OUTPUTDIR, "biquadratic", print_vars, time_step);

    GridToParticlesProjection(ml_prob, *linea);

    linea->GetLine(line[0]);
    PrintLine(DEFAULT_OUTPUTDIR, line, false, time_step);

  }

  delete linea;
  return 0;

} //end main


