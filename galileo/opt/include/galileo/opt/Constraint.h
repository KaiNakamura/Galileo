#pragma once

#include <pinocchio/autodiff/casadi.hpp>

namespace galileo
{
    namespace opt
    {
        /**
         * @brief Data necessary to build the problem or constraints.
         * It is good practice to have a corresponding constraint specific
         * Problem Data for any data needed to build a constraint.
         *
         */
        struct GeneralProblemData
        {
            /**
             * @brief Construct a new Problem Data object.
             *
             * @param Fint_ Continuous-time function. The decision variables are infinitesimal deviations from the initial state,
                allowing for states to lie on a manifold. Fint is the function which maps these
                deviations back to the actual state space
             * @param Fdif_ Continuous-time function. The ineverse function of Fint. This is used to generate the initial guess for the states.
             * @param F_ Dynamics of the system
             * @param L_ Running cost
             * @param Phi_ Terminal cost
             */
            GeneralProblemData(casadi::Function Fint_, casadi::Function Fdif_, casadi::Function L_, casadi::Function Phi_)
            {
                this->Fint = Fint_;
                this->Fdif = Fdif_;
                this->L = L_;
                this->Phi = Phi_;
            }

            /**
             * @brief Continuous-time function. The decision variables are infinitesimal deviations from the initial state,
                allowing for states to lie on a manifold. Fint is the function which maps these
                deviations back to the actual state space.
             *
             */
            casadi::Function Fint;

            /**
             * @brief Continuous-time function. The ineverse function of Fint. This is used to generate the initial guess for the states.
             *
             */
            casadi::Function Fdif;

            /**
             * @brief The "running" or integrated cost function.
             *
             */
            casadi::Function L;

            /**
             * @brief The terminal cost function.
             *
             */
            casadi::Function Phi;
        };

        /**
         * @brief Results that describe a "built" constraint.
         * This contains the constraint Function, Bounds, etc.
         *
         */
        struct ConstraintData
        {
            /**
             * @brief Upper bounds of the constraint function.
             *
             */
            casadi::Function upper_bound;

            /**
             * @brief Lower bounds of the constraint function.
             *
             */
            casadi::Function lower_bound;

            /**
             * @brief The constraint function.
             *
             */
            casadi::Function G;
        };

        /**
         * @brief Data for the decision variables.
         *
         */
        struct DecisionData
        {

            /**
             * @brief Upper bound on decision variables.
             *
             */
            casadi::Function upper_bound;

            /**
             * @brief Lower bound on decision variables.
             *
             */
            casadi::Function lower_bound;

            /**
             * @brief Initial guess function for state and input as a function of time. Note that an inverse law for Fint will be used to generate the initial guess for the states.
             *
             */
            casadi::Function initial_guess;

            /**
             * @brief Decision variables at one knot or collocation point (x, u)
             *
             */
            casadi::SX w;
        };

        /**
         * @brief Extend this class to implement constraints.
         *
         */
        template <class ProblemData>
        class ConstraintBuilder
        {
        public:
            /**
             * @brief Construct a new Constraint Builder object.
             *
             */
            ConstraintBuilder() {}

            /**
             * @brief Destroy the Constraint Builder object.
             *
             */
            virtual ~ConstraintBuilder() = default;

            /**
             * @brief Build constraint data for a given problem data.
             *
             * @param problem_data Problem specific data
             * @param phase_index Index to build constraint data for
             * @param constraint_data Constraint specific data
             */
            virtual void buildConstraint(const ProblemData &problem_data, int phase_index, ConstraintData &constraint_data)
            {
                createBounds(problem_data, phase_index, constraint_data.upper_bound, constraint_data.lower_bound);
                createFunction(problem_data, phase_index, constraint_data.G);
            }

            /**
             * @brief Generate bounds for a vector of points.
             *
             * @param problem_data Problem specific data
             * @param phase_index Index to build constraint data for
             * @param upper_bound Upper bound function to return
             * @param lower_bound Lower bound function to return
             */
            virtual void createBounds(const ProblemData &problem_data, int phase_index, casadi::Function &upper_bound, casadi::Function &lower_bound) const = 0;

            /**
             * @brief Generate a function to evaluate each point.
             *
             * @param problem_data Problem specific data
             * @param phase_index Index to build constraint data for
             * @param G The constraint function to return
             */
            virtual void createFunction(const ProblemData &problem_data, int phase_index, casadi::Function &G) const = 0;
        };
    };
};