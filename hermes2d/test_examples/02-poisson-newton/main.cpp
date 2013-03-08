#include "definitions.h"

// This example shows how to use Newton (Robin) boundary conditions.
// These conditions are used, for example, in heat transfer problems
// when the heat transfer coefficient is known but the heat flux
// itself is unknown.
//
// PDE: Poisson equation -div(LAMBDA grad u) - VOLUME_HEAT_SRC = 0.
//
// Boundary conditions:
//   Boundary part "Outer": Newton condition LAMBDA * du/dn = ALPHA * (T_EXTERIOR - u).
//   Boundary parts "Bottom", "Inner" and "Left":
//                          Dirichlet u(x, y) = A*x + B*y + C where the
//                          constants A, B, C are arbitrary constants (defined below).
//
// The following parameters can be changed:

const bool HERMES_VISUALIZATION = true;           // Set to "false" to suppress Hermes OpenGL visualization.
const bool VTK_VISUALIZATION = true;              // Set to "true" to enable VTK output.
const int P_INIT = 5;                             // Uniform polynomial degree of mesh elements.
const int INIT_REF_NUM = 0;                       // Number of initial uniform mesh refinements.

// Possibilities: SOLVER_AMESOS, SOLVER_AZTECOO, SOLVER_MUMPS,
// SOLVER_PETSC, SOLVER_SUPERLU, SOLVER_UMFPACK.
Hermes::MatrixSolverType matrix_solver_type = Hermes::SOLVER_UMFPACK;

// Problem parameters.
const double LAMBDA_AL = 236.0;            // Thermal cond. of Al for temperatures around 20 deg Celsius.
const double LAMBDA_CU = 386.0;            // Thermal cond. of Cu for temperatures around 20 deg Celsius.
const double VOLUME_HEAT_SRC = 0.0;        // Volume heat sources generated by electric current.
const double ALPHA = 5.0;                  // Heat transfer coefficient.
const double T_EXTERIOR = 50.0;            // Exterior temperature.
const double BDY_A_PARAM = 0.0;
const double BDY_B_PARAM = 0.0;
const double BDY_C_PARAM = 20.0;

int main(int argc, char* argv[])
{
  // Load the mesh->
  MeshSharedPtr mesh(new Mesh);
  Hermes::Hermes2D::MeshReaderH2D mloader;
  mloader.load("domain.mesh", mesh);

  // Perform initial mesh refinements (optional).
  for (int i = 0; i < INIT_REF_NUM; i++)
    mesh->refine_all_elements();

  // Initialize the weak formulation.
  CustomWeakFormPoissonNewton wf("Aluminum", new Hermes::Hermes1DFunction<double>(LAMBDA_AL),
    "Copper", new Hermes::Hermes1DFunction<double>(LAMBDA_CU),
    new Hermes::Hermes2DFunction<double>(-VOLUME_HEAT_SRC),
    "Outer", ALPHA, T_EXTERIOR);

  // Initialize boundary conditions.
  CustomDirichletCondition bc_essential(Hermes::vector<std::string>("Bottom", "Inner", "Left"),
    BDY_A_PARAM, BDY_B_PARAM, BDY_C_PARAM);
  Hermes::Hermes2D::EssentialBCs<double> bcs(&bc_essential);

  // Create an H1 space with default shapeset.
  SpaceSharedPtr<double> space(new Hermes::Hermes2D::H1Space<double>(mesh, &bcs, P_INIT));
  int ndof = space->get_num_dofs();

  // Initialize the Newton solver.
  Hermes::Hermes2D::NewtonSolver<double> newton;

  newton.set_weak_formulation(&wf);
  newton.set_space(space);

  // Perform Newton's iteration and translate the resulting coefficient vector into a Solution.
  MeshFunctionSharedPtr<double> sln(new Hermes::Hermes2D::Solution<double>());

  try{
    newton.solve();
  }
  catch(Hermes::Exceptions::Exception e)
	{
    e.print_msg();
  }

  Hermes::Hermes2D::Solution<double>::vector_to_solution(newton.get_sln_vector(), space, sln);

  // VTK output.
  if(VTK_VISUALIZATION) {
    // Output solution in VTK format.
    Hermes::Hermes2D::Views::Linearizer lin;
    bool mode_3D = true;
    //lin.save_solution_vtk(slnAf, "sln.vtk", "Temperature", mode_3D);

    // Output mesh and element orders in VTK format.
    Hermes::Hermes2D::Views::Orderizer ord;
    ord.save_orders_vtk(space, "ord.vtk");
    }

  // Visualize the solution.
  if(HERMES_VISUALIZATION) {
    Hermes::Hermes2D::Views::ScalarView view("Solution", new Hermes::Hermes2D::Views::WinGeom(0, 0, 440, 350));
    view.show(sln, Hermes::Hermes2D::Views::HERMES_EPS_VERYHIGH);
    Hermes::Hermes2D::Views::View::wait();
  }

  return 0;
}
