#include "traj_test.h"

int main(int argc, char **argv)
{
    double q0[] = {
        0, 0, 1.0627, 0, 0, 0, 1, 0.0000, 0.0000, -0.3207, 0.7572, -0.4365,
        0.0000, 0.0000, 0.0000, -0.3207, 0.7572, -0.4365, 0.0000};

    Eigen::Map<ConfigVector> q0_vec(q0, 19);

    LeggedBody<Scalar> bot(huron_location, num_ees, end_effector_names);

    Model model = bot.model;
    Data data = Data(model);

    // Create environment Surfaces
    shared_ptr<environment::EnvironmentSurfaces> surfaces = make_shared<environment::EnvironmentSurfaces>();
    surfaces->push_back(environment::CreateInfiniteGround());
    std::cout << "EnvironmentSurfaces created" << std::endl;

    shared_ptr<contact::ContactSequence> contact_sequence = make_shared<contact::ContactSequence>(num_ees);
    std::cout << "ContactSequence created" << std::endl;

    legged::contact::RobotEndEffectors ees = bot.getEndEffectors();
    std::cout << "RobotEndEffectors created" << std::endl;

    ADModel cmodel = model.cast<ADScalar>();
    ADData cdata(cmodel);
    std::cout << "ADModel and ADData created" << std::endl;

    auto nq = model.nq;
    auto nv = model.nv;
    shared_ptr<opt::LeggedRobotStates> si = make_shared<opt::LeggedRobotStates>(nq, nv, ees);
    std::cout << "LeggedRobotStates created" << std::endl;

    contact::ContactMode initial_mode;
    initial_mode.combination_definition = bot.getContactCombination(0b11);
    initial_mode.contact_surfaces = {0, 0};
    initial_mode.createModeDynamics(make_shared<Model>(cmodel),ees, si);
    std::cout << "Initial mode created" << std::endl;

    contact_sequence->addPhase(initial_mode, 100, 0.2);
    std::cout << "Initial mode added to sequence" << std::endl;

    casadi::Function F;
    initial_mode.getModeDynamics(F);
    std::cout << "Initial mode dynamics created" << std::endl;

    SX cx = SX::sym("x", si->nx);
    SX cx2 = SX::sym("x2", si->nx);
    SX cdx = SX::sym("dx", si->ndx);
    SX cu = SX::sym("u", si->nu);
    SX cdt = SX::sym("dt");

    auto ch = si->get_ch(cx);
    auto ch_d = si->get_ch_d(cdx);
    auto cdh = si->get_cdh(cx);
    auto cdh_d = si->get_cdh_d(cdx);
    auto cq = si->get_q(cx);
    auto cq_d = si->get_q_d(cdx);
    auto cqj = si->get_qj(cx);
    auto cv = si->get_v(cx);
    auto cv_d = si->get_v_d(cdx);
    auto cvj = si->get_vj(cx);
    auto cvju = si->get_vju(cu);
    auto cwrenches = si->get_all_wrenches(cu);
    
    auto ch2 = si->get_ch(cx2);
    auto cdh2 = si->get_cdh(cx2);
    auto cq2 = si->get_q(cx2);
    auto cv2 = si->get_v(cx2);

    ConfigVectorAD q_AD(model.nq);
    q_AD = Eigen::Map<ConfigVectorAD>(static_cast<vector<ADScalar>>(cq).data(), model.nq, 1);
    
    ConfigVectorAD q2_AD(model.nq);
    q2_AD = Eigen::Map<ConfigVectorAD>(static_cast<vector<ADScalar>>(cq2).data(), model.nq, 1);

    Function Fint("Fint",
                  {cx, cdx, cdt},
                  {vertcat(ch + ch_d,
                           cdh + cdh_d,
                           // cq_result, // returns different expression than python version, NO idea why
                           custom_fint(cx, cdx, cdt),
                           cv + cv_d)});

    ConfigVectorAD v_result = pinocchio::difference(cmodel, q_AD, q2_AD);
    SX cv_result(model.nv, 1);
    pinocchio::casadi::copy(v_result, cv_result);

    Function Fdif("Fdif",
                  {cx, cx2, cdt},
                  {vertcat(ch2 - ch,
                           cdh2 - cdh,
                           cv_result / cdt,
                           cv2 - cv)});
    SX cq0(model.nq);
    pinocchio::casadi::copy(q0_vec, cq0);

    Function L("L",
               {cx, cu},
               {1e-3 * SX::sumsqr(cvju) +
                1e-4 * SX::sumsqr(cwrenches) +
                1e1 * SX::sumsqr(cqj - cq0(Slice(7, nq)))});

    Function Phi("Phi",
                 {cx},
                 {1e2 * SX::sumsqr(cqj - cq0(Slice(7, nq)))});

    Dict opts;
    opts["ipopt.linear_solver"] = "ma97";
    opts["ipopt.ma97_order"] = "metis";
    opts["ipopt.fixed_variable_treatment"] = "make_constraint";
    opts["ipopt.max_iter"] = 1;

    using namespace legged;
    using namespace constraints;

    shared_ptr<GeneralProblemData> gp_data = make_shared<GeneralProblemData>(Fint, Fdif, F, L, Phi);

    std::shared_ptr<ConstraintBuilder<LeggedRobotProblemData>> friction_cone_constraint_builder =
        std::make_shared<FrictionConeConstraintBuilder<LeggedRobotProblemData>>();

    std::shared_ptr<ConstraintBuilder<LeggedRobotProblemData>> velocity_constraint_builder =
        std::make_shared<VelocityConstraintBuilder<LeggedRobotProblemData>>();

    std::shared_ptr<ConstraintBuilder<LeggedRobotProblemData>> contact_constraint_builder =
        std::make_shared<ContactConstraintBuilder<LeggedRobotProblemData>>();

    vector<shared_ptr<ConstraintBuilder<LeggedRobotProblemData>>> builders = {friction_cone_constraint_builder, velocity_constraint_builder, contact_constraint_builder};

    shared_ptr<LeggedRobotProblemData> legged_problem_data = make_shared<LeggedRobotProblemData>(gp_data, surfaces, contact_sequence, si, make_shared<ADModel>(cmodel), make_shared<ADData>(cdata), ees, cx, cu, cdt, 20);

    std::vector<opt::ConstraintData> constraint_datas;
    for (auto builder : builders)
    {
        std::cout << "Building constraint" << std::endl;
        opt::ConstraintData some_data;
        builder->BuildConstraint(*legged_problem_data, 0, some_data);
        std::cout << "Built constraint" << std::endl;
        constraint_datas.push_back(some_data);
    }

    TrajectoryOpt<LeggedRobotProblemData> traj(legged_problem_data, builders, opts);

    DM X0 = DM::zeros(si->nx, 1);
    int j = 0;
    for (int i = si->nh + si->ndh; i < si->nh + si->ndh + si->nq; ++i)
    {
        X0(i) = q0_vec[j];
        ++j;
    }

    traj.init_finite_elements(1, X0);

    auto sol = traj.optimize();
    cout << sol << endl;

    return 0;
}