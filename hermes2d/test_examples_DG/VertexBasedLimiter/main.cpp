#include "definitions.h"
#include "../euler_util.h"
#include "algorithms.h"

int polynomialDegree = 2;
int initialRefinementsCount = 4;
const Algorithm algorithm = Multiscale;
SolvedExample solvedExample = MovingPeak;
static std::string SolvedExampleString[5] = { "1D", "CircularConvection", "MovingPeak", "AdvectedCube", "SolidBodyRotation" };
std::string solvedExampleString = SolvedExampleString[solvedExample];
double MovingPeakDiffusivity = 1e-3;

double diffusivity = 1e-2;
double s = -1;
double CFL = 5e-1;

Hermes::vector<int> iter_per_time_step_HSS;
Hermes::vector<int> smoothing_steps_per_V_cycle;
Hermes::vector<int> V_cycles_per_time_step;

int main(int argc, char* argv[])
{
  HermesCommonApi.set_integral_param_value(numThreads, 1);

  if (argc > 1)
    initialRefinementsCount = atoi(argv[1]);
  if (argc > 2)
  {
    diffusivity = (double)atof(argv[2]);
    MovingPeakDiffusivity = diffusivity;
  }
  if (argc > 3)
  {
    solvedExample = (SolvedExample)atoi(argv[3]);
    solvedExampleString = SolvedExampleString[solvedExample];
  }
  if (argc > 4)
    CFL = atof(argv[4]);

  if (is_timedep(solvedExample))
  {
    iter_per_time_step_HSS.push_back(1);
    iter_per_time_step_HSS.push_back(2);
    smoothing_steps_per_V_cycle.push_back(1);
    smoothing_steps_per_V_cycle.push_back(2);
    V_cycles_per_time_step.push_back(1);
    V_cycles_per_time_step.push_back(2);
  }
  else
  {
    CFL = 128.;
    iter_per_time_step_HSS.push_back(1);
    smoothing_steps_per_V_cycle.push_back(2);
    smoothing_steps_per_V_cycle.push_back(3);
    smoothing_steps_per_V_cycle.push_back(5);
    smoothing_steps_per_V_cycle.push_back(10);
    smoothing_steps_per_V_cycle.push_back(15);
    V_cycles_per_time_step.push_back(1);
  }
    
  double sigma = std::pow(2., (double)(initialRefinementsCount)) * (s == -1 ? 10.0 : (s == 1 ? 10. : 0.));

  // Load the mesh.
  double mesh_size;
  MeshSharedPtr mesh(new Mesh);
  MeshReaderH2DXML mloader;
  switch(solvedExample)
  {
  case SolidBodyRotation:
    mloader.load("domain_rotation.xml", mesh);
    for(int i = 0; i < initialRefinementsCount; i++)
      mesh->refine_all_elements();
    break;
    mesh_size = 1.;
  case AdvectedCube:
    mloader.load("larger_domain.xml", mesh);
    for(int i = 0; i < initialRefinementsCount; i++)
      mesh->refine_all_elements();
    mesh_size = 1.;
    break;
  case CircularConvection:
    mloader.load("domain_circular_convection.xml", mesh);
    for(int i = 0; i < initialRefinementsCount; i++)
      mesh->refine_all_elements();
    mesh_size = 1.;
    break;
  case MovingPeak:
    mloader.load("domain.xml", mesh);
    for(int i = 0; i < initialRefinementsCount; i++)
      mesh->refine_all_elements();
    mesh_size = 2.;
    break;
  case Benchmark:
    mloader.load("domain_benchmark.xml", mesh);
    mesh->refine_all_elements(2);
    for(int i = 0; i < initialRefinementsCount; i++)
      mesh->refine_all_elements();
    mesh_size = .6;
    break;
  }
  double time_step_length = mesh_size * CFL * std::pow(2., -(double)initialRefinementsCount);

  // Previous time level solution (initialized by the initial condition).
  ExactSolutionScalar<double>* previous_initial_condition = NULL;
  ExactSolutionScalar<double>* initial_condition = NULL;
  ExactSolutionScalar<double>* initial_condition_der = NULL;
  ExactSolutionScalar<double>* initial_solution = NULL;
  ExactSolutionScalar<double>* exact_sln = NULL;
  switch(solvedExample)
  {
  case SolidBodyRotation:
    initial_condition = new InitialConditionSolidBodyRotation(mesh);
    initial_condition_der = new InitialConditionSolidBodyRotation(mesh);
    previous_initial_condition = new InitialConditionSolidBodyRotation(mesh);
    break;
  case AdvectedCube:
    initial_condition = new InitialConditionAdvectedCube(mesh);
    initial_condition_der = new ZeroSolution<double>(mesh);
    previous_initial_condition = new InitialConditionAdvectedCube(mesh);
    break;
  case CircularConvection:
    initial_condition = new ZeroSolution<double>(mesh);
    initial_condition_der = new ZeroSolution<double>(mesh);
    previous_initial_condition = new ZeroSolution<double>(mesh);
    exact_sln = new ExactSolutionCircularConvection(mesh);
    initial_solution = new ExactSolutionCircularConvection(mesh);
    break;
  case MovingPeak:
    initial_condition = new ExactSolutionMovingPeak(mesh, MovingPeakDiffusivity, M_PI / 2.);
    initial_condition_der = new ExactSolutionMovingPeak(mesh, MovingPeakDiffusivity, M_PI / 2.);
    previous_initial_condition = new ExactSolutionMovingPeak(mesh, MovingPeakDiffusivity, M_PI / 2.);
    exact_sln = new ExactSolutionMovingPeak(mesh, MovingPeakDiffusivity, (1./2.) * M_PI);
    initial_solution = new ExactSolutionMovingPeak(mesh, MovingPeakDiffusivity, (1./2.) * M_PI);
    break;
  case Benchmark:
    initial_condition = new ZeroSolution<double>(mesh);
    initial_condition_der = new ZeroSolution<double>(mesh);
    previous_initial_condition = new ZeroSolution<double>(mesh);
    exact_sln = new ExactSolutionBenchmark2(mesh, diffusivity);
    initial_solution = new ExactSolutionBenchmark2(mesh, diffusivity);
    break;
  }
  
  // Solutions.
  MeshFunctionSharedPtr<double> solution(new Solution<double>);
  MeshFunctionSharedPtr<double> previous_solution(previous_initial_condition);
  MeshFunctionSharedPtr<double> previous_mean_values(initial_condition);
  MeshFunctionSharedPtr<double> previous_derivatives(initial_condition_der);
  MeshFunctionSharedPtr<double> exact_solution(exact_sln);
  MeshFunctionSharedPtr<double> initial_sln(initial_solution);

  // Visualization.
  ScalarView solution_view("Solution", new WinGeom(0, 0, 600, 350));
  ScalarView exact_view("Exact solution", new WinGeom(610, 0, 600, 350));
  
  // Exact solver solution
  SpaceSharedPtr<double> space(new L2Space<double>(mesh, polynomialDegree, new L2ShapesetTaylor));
  if(!is_timedep(solvedExample))
    solve_exact(solvedExample, space, diffusivity, s, sigma, exact_solution, initial_sln, time_step_length, polynomialDegree, initialRefinementsCount);
  
  Hermes::Mixins::TimeMeasurable cpu_time;
  if(algorithm == Multiscale || algorithm == Both)
  {
    for (int si = 0; si < iter_per_time_step_HSS.size(); si++)
    {
      Hermes::Mixins::Loggable logger(true, NULL, false);
      logger.set_timestamps(false);
      logger.set_erase_on_beginning(true);
      Hermes::Mixins::Loggable logger_details(true);
      logger_details.set_timestamps(false);
      logger_details.set_erase_on_beginning(true);
      std::stringstream ss;
      ss << "HSS(" << iter_per_time_step_HSS[si] << ")_" << solvedExampleString << "_" << initialRefinementsCount << "_" << diffusivity << "_CFL=" << CFL << ".h2d";
      logger.set_logFile_name(ss.str());
      std::stringstream ssd;
      ssd << "HSS_detail(" << iter_per_time_step_HSS[si] << ")_" << solvedExampleString << "_" << initialRefinementsCount << "_" << diffusivity << ".h2d";
      logger_details.set_logFile_name(ssd.str());

      cpu_time.tick();
      int iteration_count = multiscale_decomposition(mesh, solvedExample, polynomialDegree, initialRefinementsCount, previous_mean_values, previous_derivatives, diffusivity, s, sigma, time_step_length,
        initial_sln, solution, exact_solution, &solution_view, &exact_view, logger, logger_details, CFL, iter_per_time_step_HSS[si]);

      cpu_time.tick();
      logger.info("%f|%i", cpu_time.last(), iteration_count);
    }
  }

  if(algorithm == pMultigrid || algorithm == Both)
  {
    for (int si = 0; si < smoothing_steps_per_V_cycle.size(); si++)
    {
     for (int siv = 0; siv < V_cycles_per_time_step.size(); siv++)
      {
        Hermes::Mixins::Loggable logger(true, NULL, false);
        logger.set_timestamps(false);
        logger.set_erase_on_beginning(true);
        std::stringstream ss;
        ss << "MG(" << V_cycles_per_time_step[siv] << "-" << smoothing_steps_per_V_cycle[si] << ")_" << solvedExampleString << "_" << initialRefinementsCount << "_" << diffusivity << "_CFL=" << CFL << ".h2d";
        logger.set_logFile_name(ss.str());

        MeshFunctionSharedPtr<double> previous_solution_local(new ExactSolutionMovingPeak(mesh, MovingPeakDiffusivity, M_PI / 2.));
        cpu_time.tick();

        int v_cycle_count = p_multigrid(mesh, solvedExample, polynomialDegree, initialRefinementsCount, previous_solution_local, diffusivity, time_step_length,
          solution, exact_solution, &solution_view, &exact_view, s, sigma, logger, smoothing_steps_per_V_cycle[si], CFL, V_cycles_per_time_step[siv]);

        cpu_time.tick();
        logger.info("%f|%i", cpu_time.last(), v_cycle_count);
      }
    }
  }
  
  View::wait();
  return 0;
}