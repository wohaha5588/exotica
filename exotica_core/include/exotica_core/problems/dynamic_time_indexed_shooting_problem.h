//
// Copyright (c) 2019, Wolfgang Merkt
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of  nor the names of its contributors may be used to
//    endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#ifndef EXOTICA_CORE_DYNAMIC_TIME_INDEXED_SHOOTING_PROBLEM_H_
#define EXOTICA_CORE_DYNAMIC_TIME_INDEXED_SHOOTING_PROBLEM_H_

#include <exotica_core/dynamics_solver.h>
#include <exotica_core/planning_problem.h>
#include <exotica_core/tasks.h>

#include <exotica_core/dynamic_time_indexed_shooting_problem_initializer.h>

namespace exotica
{
class DynamicTimeIndexedShootingProblem : public PlanningProblem, public Instantiable<DynamicTimeIndexedShootingProblemInitializer>
{
public:
    DynamicTimeIndexedShootingProblem();
    virtual ~DynamicTimeIndexedShootingProblem();
    void Instantiate(DynamicTimeIndexedShootingProblemInitializer& init) override;

    void PreUpdate() override;
    void Update(Eigen::VectorXdRefConst u, int t);

    int get_T() const;            ///< Returns the number of timesteps in the state trajectory.
    void set_T(const int& T_in);  ///< Sets the number of timesteps in the state trajectory.

    double get_tau() const;  ///< Returns the discretization timestep tau

    Eigen::MatrixXd get_X() const;             ///< Returns the state trajectory X
    void set_X(Eigen::MatrixXdRefConst X_in);  ///< Sets the state trajectory X (can be used as the initial guess)

    Eigen::MatrixXd get_U() const;             ///< Returns the control trajectory U
    void set_U(Eigen::MatrixXdRefConst U_in);  ///< Sets the control trajectory U (can be used as the initial guess)

    Eigen::MatrixXd get_X_star() const;                  ///< Returns the target state trajectory X
    void set_X_star(Eigen::MatrixXdRefConst X_star_in);  ///< Sets the target state trajectory X

    Eigen::MatrixXd get_Q(int t) const;               ///< Returns the precision matrix at time step t
    void set_Q(Eigen::MatrixXdRefConst Q_in, int t);  ///< Sets the precision matrix for time step t

    double GetStateCost(int t) const;
    double GetControlCost(int t) const;

    Eigen::VectorXd GetStateCostJacobian(int t) const;
    Eigen::VectorXd GetControlCostJacobian(int t) const;

private:
    void ReinitializeVariables();

    int T_;       ///< Number of time steps
    double tau_;  ///< Time step duration

    Eigen::MatrixXd X_;       ///< State trajectory (i.e., positions, velocities). Size: num-states x T
    Eigen::MatrixXd U_;       ///< Control trajectory. Size: num-controls x (T-1)
    Eigen::MatrixXd X_star_;  ///< Goal state trajectory (i.e., positions, velocities). Size: num-states x T

    std::vector<Eigen::MatrixXd> Q_;  ///< State space penalty matrix (precision matrix), per time index
    Eigen::MatrixXd R_;               ///< Control space penalty matrix

    DynamicTimeIndexedShootingProblemInitializer init_;  ///< Stores the problem initializer for re-initialization.
    DynamicsSolverPtr dynamics_solver_;

    std::vector<std::shared_ptr<KinematicResponse>> kinematic_solutions_;
};

typedef std::shared_ptr<exotica::DynamicTimeIndexedShootingProblem> DynamicTimeIndexedShootingProblemPtr;
}

#endif  // EXOTICA_CORE_DYNAMIC_TIME_INDEXED_SHOOTING_PROBLEM_H_
