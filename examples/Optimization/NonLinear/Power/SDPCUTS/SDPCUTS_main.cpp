//
// Created by kbestuzheva on 12/11/17.
//
#include <stdio.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <cstring>
#include <fstream>
#include "../PowerNet.h"
#include <gravity/solver.h>
#include <stdio.h>
#include <stdlib.h>
#include <optionParser.hpp>

using namespace std;
using namespace gravity;


/* main */
int main (int argc, char * argv[]) {
    int output = 0;
    bool sdp_cuts = true;
    
    bool current_from = true, llnc=true, current_to=true, loss=true, loss_bounds=true;
    
    size_t num_bags = 0;
    string num_bags_s = "100";
    string solver_str = "ipopt";
    string sdp_cuts_s = "yes";
    string current_from_s = "yes";
    string orig_s = "yes";
    string current_to_s="yes";
    string lazy_s = "no";
    bool lazy_bool = false;
    SolverType solv_type = ipopt;
    double tol = 1e-6;
    string mehrotra = "no";
    
    
    string fname = string(prj_dir)+"/data_sets/Power/nesta_case5_pjm.m";
    
    // create a OptionParser with options
    op::OptionParser opt;
    opt.add_option("h", "help",
                   "shows option help"); // no default value means boolean options, which default value is false
    opt.add_option("f", "file", "Input file name", fname);
    opt.add_option("s", "solver", "Solvers: ipopt/cplex/gurobi, default = ipopt", solver_str);
    opt.add_option("b", "numbags", "Number of bags per iteration", num_bags_s);
    opt.add_option("If", "current_from", "add from current constraints", current_from_s);
    opt.add_option("o", "original", "add original variables and linking constraints", orig_s);
    opt.add_option("It", "current_to", "add to current constraints", current_to_s);
    opt.add_option("lz", "lazy", "Generate 3d SDP cuts in a lazy fashion, default = no", lazy_s);
    // parse the options and verify that all went well. If not, errors and help will be shown
    bool correct_parsing = opt.parse_options(argc, argv);
    
    if (!correct_parsing) {
        return EXIT_FAILURE;
    }
    
    fname = opt["f"];
    bool has_help = op::str2bool(opt["h"]);
    if (has_help) {
        opt.show_help();
        exit(0);
    }
    solver_str = opt["s"];
    if (solver_str.compare("gurobi")==0) {
        solv_type = gurobi;
    }
    else if(solver_str.compare("cplex")==0) {
        solv_type = cplex;
    }else if(solver_str.compare("Mosek")==0) {
        solv_type = _mosek;
    }
    lazy_s = opt["lz"];
    if (lazy_s.compare("no")==0) {
        lazy_bool = false;
    }
    else if(lazy_s.compare("yes")==0) {
        lazy_bool = true;
    }
    
    current_from_s = opt["If"];
    if (current_from_s.compare("no")==0) {
        current_from = false;
    }
    else {
        current_from = true;
    }
    bool add_original = true;
    
    orig_s = opt["o"];
    if (orig_s.compare("no")==0) {
        add_original = false;
    }
    else {
        add_original = true;
    }
    
    
    
    current_to_s = opt["It"];
    if (current_to_s.compare("no")==0) {
        current_to = false;
    }
    else {
        current_to = true;
    }
    
    current_from=true;
    current_to=true;
    loss=true;
    
    num_bags = atoi(opt["b"].c_str());
    
    cout << "\nnum bags = " << num_bags << endl;
    
    double total_time_start = get_wall_time();
    PowerNet grid;
    grid.readgrid(fname);
    grid.update_ref_bus();
    
    grid.get_tree_decomp_bags();
    auto bags_3d=grid.decompose_bags_3d();
    
    
    /* Grid Stats */
    auto nb_gen = grid.get_nb_active_gens();
    auto nb_lines = grid.get_nb_active_arcs();
    auto nb_buses = grid.get_nb_active_nodes();
    DebugOn("nb active gens = " << nb_gen << endl);
    DebugOn("nb active lines = " << nb_lines << endl);
    DebugOn("nb active buses = " << nb_buses << endl);
    
    /** Sets */
    auto bus_pairs = grid.get_bus_pairs();
    auto bus_pairs_chord = grid.get_bus_pairs_chord(bags_3d);
    if (grid._tree || !grid.add_3d_nlin || !sdp_cuts) {
        bus_pairs_chord = bus_pairs;
    }
    auto nodes = indices(grid.nodes);
    auto arcs = indices(grid.arcs);
    auto gens = indices(grid.gens);
    auto gen_nodes = grid.gens_per_node();
    auto out_arcs = grid.out_arcs_per_node();
    auto in_arcs = grid.in_arcs_per_node();
    
    /* Grid Parameters */
    auto pg_min = grid.pg_min.in(gens);
    auto pg_max = grid.pg_max.in(gens);
    auto qg_min = grid.qg_min.in(gens);
    auto qg_max = grid.qg_max.in(gens);
    auto c1 = grid.c1.in(gens);
    auto c2 = grid.c2.in(gens);
    auto c0 = grid.c0.in(gens);
    auto pl = grid.pl.in(nodes);
    auto ql = grid.ql.in(nodes);
    auto gs = grid.gs.in(nodes);
    auto bs = grid.bs.in(nodes);
    auto b = grid.b.in(arcs);
    auto g = grid.g.in(arcs);
    auto as = grid.as.in(arcs);
    auto ch = grid.ch.in(arcs);
    auto tr = grid.tr.in(arcs);
    auto th_min = grid.th_min.in(bus_pairs);
    auto th_max = grid.th_max.in(bus_pairs);
    auto g_ft = grid.g_ft.in(arcs);
    auto g_ff = grid.g_ff.in(arcs);
    auto g_tt = grid.g_tt.in(arcs);
    auto g_tf = grid.g_tf.in(arcs);
    auto b_ft = grid.b_ft.in(arcs);
    auto b_ff = grid.b_ff.in(arcs);
    auto b_tf = grid.b_tf.in(arcs);
    auto b_tt = grid.b_tt.in(arcs);
    auto S_max = grid.S_max.in(arcs);
    auto v_max = grid.v_max.in(nodes);
    auto v_min = grid.v_min.in(nodes);
    auto w_max = grid.w_max.in(nodes);
    auto w_min = grid.w_min.in(nodes);
    auto tan_th_min = grid.tan_th_min.in(bus_pairs);
    auto tan_th_max = grid.tan_th_max.in(bus_pairs);
    auto wr_min = grid.wr_min.in(bus_pairs_chord);
    auto wr_max = grid.wr_max.in(bus_pairs_chord);
    auto wi_min = grid.wi_min.in(bus_pairs_chord);
    auto wi_max = grid.wi_max.in(bus_pairs_chord);
    auto lij_min=grid.lij_min.in(arcs);
    auto lij_max=grid.lij_max.in(arcs);
    auto cc=grid.cc.in(arcs);
    auto dd=grid.dd.in(arcs);
    auto ch_half=grid.ch_half.in(arcs);
    auto arcs_inductive=grid.arcs_inductive_only();
    auto lji_min=grid.lji_min.in(arcs);
    auto lji_max=grid.lji_max.in(arcs);
    
    
    double upper_bound = grid.solve_acopf(ACRECT);

    
    /** Build model */
    Model<> SDP("SDP Model");
   // Model<> SDPOA("SDP-OA Model");
    
    /** Variables */
    /* Power generation variables */
    var<> Pg("Pg", pg_min, pg_max);
    var<> Qg ("Qg", qg_min, qg_max);
    var<> eta("eta", 0, 1);
    SDP.add(Pg.in(gens),Qg.in(gens));
    SDP.add(eta.in(range(0,0)));
//    SDPOA.add(Pg.in(gens),Qg.in(gens));
//    SDPOA.add(eta.in(range(0,0)));
    
    /* Power flow variables */
    var<> Pf_from("Pf_from", -1.*S_max,S_max);
    var<> Qf_from("Qf_from", -1.*S_max,S_max);
    var<> Pf_to("Pf_to", -1.*S_max,S_max);
    var<> Qf_to("Qf_to", -1.*S_max,S_max);
    
    SDP.add(Pf_from.in(arcs), Qf_from.in(arcs),Pf_to.in(arcs),Qf_to.in(arcs));
    //SDPOA.add(Pf_from.in(arcs), Qf_from.in(arcs),Pf_to.in(arcs),Qf_to.in(arcs));
    
    
    /* Real part of Wij = ViVj */
    var<>  R_Wij("R_Wij", wr_min, wr_max);
    /* Imaginary part of Wij = ViVj */
    var<>  Im_Wij("Im_Wij", wi_min, wi_max);
    /* Magnitude of Wii = Vi^2 */
    var<>  Wii("Wii", w_min, w_max);
    SDP.add(Wii.in(nodes),R_Wij.in(bus_pairs_chord),Im_Wij.in(bus_pairs_chord));
    //SDPOA.add(Wii.in(nodes),R_Wij.in(bus_pairs_chord),Im_Wij.in(bus_pairs_chord));
    
    
    /* Initialize variables */
    R_Wij.initialize_all(1.0);
    Wii.initialize_all(1.001);
    
    bool current = true;
    var<> lij("lij", lij_min,lij_max);
    var<> lji("lji", lji_min,lji_max);
    //var<> eta("eta", 0, 10);
    if(current){
        SDP.add(lij.in(arcs),lji.in(arcs));
        //SDPOA.add(lij.in(arcs),lji.in(arcs));
    }
    
    //SDP.add(eta.in(range(0, 0)));
    /**  Objective */

   // auto obj = (product(c1,Pg) + product(c2,pow(Pg,2)) + sum(c0));
   // obj=eta("0")*(-1);
    
    //auto obj_eta=eta(0);
    
    SDP.min(eta(0));
    //SDPOA.min(eta(0));
    
    Constraint<> obj_UB("obj_UB");
    obj_UB  = (product(c1,Pg) + product(c2,pow(Pg,2)) + sum(c0))-eta(0)*upper_bound;
    SDP.add(obj_UB.in(range(0,0)) <= 0);


    
    
    /** Constraints */
    auto bag_size = bags_3d.size();
    Constraint<> SDP3("SDP_3D");
    //      Constraint<> SDPD("SDPD");
    if(!grid._tree && grid.add_3d_nlin && sdp_cuts)
    {
        DebugOn("\nNum of bags = " << bag_size << endl);
        DebugOn("Adding 3d determinant polynomial cuts\n");
        auto R_Wij_ = R_Wij.pairs_in_bags(bags_3d, 3);
        auto Im_Wij_ = Im_Wij.pairs_in_bags(bags_3d, 3);
        auto Wii_ = Wii.in_bags(bags_3d, 3);
        
        
        
        SDP3 = 2 * R_Wij_[0] * (R_Wij_[1] * R_Wij_[2] + Im_Wij_[1] * Im_Wij_[2]);
        SDP3 -= 2 * Im_Wij_[0] * (R_Wij_[2] * Im_Wij_[1] - Im_Wij_[2] * R_Wij_[1]);
        SDP3 -= (pow(R_Wij_[0], 2) + pow(Im_Wij_[0], 2)) * Wii_[2];
        SDP3 -= (pow(R_Wij_[1], 2) + pow(Im_Wij_[1], 2)) * Wii_[0];
        SDP3 -= (pow(R_Wij_[2], 2) + pow(Im_Wij_[2], 2)) * Wii_[1];
        SDP3 += Wii_[0] * Wii_[1] * Wii_[2];
        if (lazy_bool) {
            SDP.add_lazy(SDP3.in(range(0, bag_size-1)) >= 0);
            //SDPOA.add_lazy(SDP3.in(orig) >= 0);
        }
        else {
            SDP.add(SDP3.in(range(0, bag_size-1)) >= 0);
            // SDPOA.add(SDP3.in(orig) >= 0);
            DebugOn("Number of 3d determinant cuts = " << SDP3.get_nb_instances() << endl);
        }
        
        //
        //        SDPD= - 0.002491499038*Im_Wij_[0] - 0.002405078286*Im_Wij_[1] + 0.002576479796*Im_Wij_[2] + 0.0006191834985*R_Wij_[0] - 0.002142755127*R_Wij_[1] - 0.004868374713*R_Wij_[2] + 0.002190591032*Wii_[0] + 0.0007469140267*Wii_[1] + 0.003457224761*Wii_[2] + 1.999982567e-08;
        //        SDP.add(SDPD.in(orig1) >= 0);
    }
    
    /** Constraints */
    /* Second-order cone constraints */
    Constraint<> SOC("SOC");
    SOC = pow(R_Wij, 2) + pow(Im_Wij, 2) - Wii.from(bus_pairs_chord)*Wii.to(bus_pairs_chord);
    SDP.add(SOC.in(bus_pairs_chord) == 0, true);
    //SDPOA.add(SOC.in(bus_pairs_chord) = 0,true);
    
    /* Flow conservation */
    Constraint<> KCL_P("KCL_P");
    KCL_P  = sum(Pf_from, out_arcs) + sum(Pf_to, in_arcs) + pl - sum(Pg, gen_nodes) + gs*Wii;
    SDP.add(KCL_P.in(nodes) == 0);
    //SDPOA.add(KCL_P.in(nodes) == 0);
    
    Constraint<> KCL_Q("KCL_Q");
    KCL_Q  = sum(Qf_from, out_arcs) + sum(Qf_to, in_arcs) + ql - sum(Qg, gen_nodes) - bs*Wii;
    SDP.add(KCL_Q.in(nodes) == 0);
    //SDPOA.add(KCL_Q.in(nodes) == 0);
    
    /* AC Power Flow */
    Constraint<> Flow_P_From("Flow_P_From");
    Flow_P_From = Pf_from - (g_ff*Wii.from(arcs) + g_ft*R_Wij.in_pairs(arcs) + b_ft*Im_Wij.in_pairs(arcs));
    SDP.add(Flow_P_From.in(arcs) == 0);
   // SDPOA.add(Flow_P_From.in(arcs) == 0);
    
    Constraint<> Flow_P_To("Flow_P_To");
    Flow_P_To = Pf_to - (g_tt*Wii.to(arcs) + g_tf*R_Wij.in_pairs(arcs) - b_tf*Im_Wij.in_pairs(arcs));
    SDP.add(Flow_P_To.in(arcs) == 0);
   // SDPOA.add(Flow_P_To.in(arcs) == 0);
    
    Constraint<> Flow_Q_From("Flow_Q_From");
    Flow_Q_From = Qf_from - (g_ft*Im_Wij.in_pairs(arcs) - b_ff*Wii.from(arcs) - b_ft*R_Wij.in_pairs(arcs));
    SDP.add(Flow_Q_From.in(arcs) == 0);
   // SDPOA.add(Flow_Q_From.in(arcs) == 0);
    
    Constraint<> Flow_Q_To("Flow_Q_To");
    Flow_Q_To = Qf_to + b_tt*Wii.to(arcs) + b_tf*R_Wij.in_pairs(arcs) + g_tf*Im_Wij.in_pairs(arcs);
    SDP.add(Flow_Q_To.in(arcs) == 0);
    //SDPOA.add(Flow_Q_To.in(arcs) == 0);
    
    /* Phase Angle Bounds constraints */
    Constraint<> PAD_UB("PAD_UB");
    PAD_UB = Im_Wij.in(bus_pairs);
    PAD_UB <= tan_th_max*R_Wij.in(bus_pairs);
    SDP.add(PAD_UB.in(bus_pairs));
   // SDPOA.add(PAD_UB.in(bus_pairs));
    
    Constraint<> PAD_LB("PAD_LB");
    PAD_LB =  Im_Wij.in(bus_pairs);
    PAD_LB >= tan_th_min*R_Wij.in(bus_pairs);
    SDP.add(PAD_LB.in(bus_pairs));
    //SDPOA.add(PAD_LB.in(bus_pairs));
    
    /* Thermal Limit Constraints */
    Constraint<> Thermal_Limit_from("Thermal_Limit_from");
    Thermal_Limit_from = pow(Pf_from, 2) + pow(Qf_from, 2);
    Thermal_Limit_from <= pow(S_max,2);
    SDP.add(Thermal_Limit_from.in(arcs));
    //SDPOA.add(Thermal_Limit_from.in(arcs));
    
    
    
    Constraint<> Thermal_Limit_to("Thermal_Limit_to");
    Thermal_Limit_to = pow(Pf_to, 2) + pow(Qf_to, 2);
    Thermal_Limit_to <= pow(S_max,2);
    SDP.add(Thermal_Limit_to.in(arcs));
    //SDPOA.add(Thermal_Limit_to.in(arcs));
    
    func<> theta_L = atan(min(Im_Wij.get_lb().in(bus_pairs)/R_Wij.get_ub().in(bus_pairs),Im_Wij.get_lb().in(bus_pairs)/R_Wij.get_lb().in(bus_pairs)));
    func<> theta_U = atan(max(Im_Wij.get_ub().in(bus_pairs)/R_Wij.get_lb().in(bus_pairs),Im_Wij.get_ub().in(bus_pairs)/R_Wij.get_ub().in(bus_pairs)));
    func<> phi=(theta_U.in(bus_pairs)+theta_L.in(bus_pairs))/2.0;
    func<> del=(theta_U.in(bus_pairs)-theta_L.in(bus_pairs))/2.0;
    
    
    Constraint<> LNC1("LNC1");
    LNC1 += (sqrt(Wii.get_lb().from(bus_pairs))+sqrt(Wii.get_ub().from(bus_pairs)))*(sqrt(Wii.get_lb().to(bus_pairs))+sqrt(Wii.get_ub().to(bus_pairs)))*(Im_Wij.in(bus_pairs)*sin(phi.in(bus_pairs)) + R_Wij.in(bus_pairs)*cos(phi.in(bus_pairs)));
    
    LNC1 -=sqrt(Wii.get_ub().to(bus_pairs))*cos(del.in(bus_pairs))*(sqrt(Wii.get_lb().to(bus_pairs))+sqrt(Wii.get_ub().to(bus_pairs)))*Wii.from(bus_pairs);
    
    LNC1 -=sqrt(Wii.get_ub().from(bus_pairs))*cos(del.in(bus_pairs))*(sqrt(Wii.get_lb().from(bus_pairs))+sqrt(Wii.get_ub().from(bus_pairs)))*Wii.to(bus_pairs);
    
    LNC1-=sqrt(Wii.get_ub().from(bus_pairs))*sqrt(Wii.get_ub().to(bus_pairs))*cos(del)*(sqrt(Wii.get_lb().from(bus_pairs))*
                                                                                        sqrt(Wii.get_lb().to(bus_pairs)) - sqrt(Wii.get_ub().from(bus_pairs))*sqrt(Wii.get_ub().to(bus_pairs)));
    SDP.add(LNC1.in(bus_pairs) >= 0);
  //  SDPOA.add(LNC1.in(bus_pairs) >= 0);
    
    Constraint<> LNC2("LNC2");
    LNC2 += (sqrt(Wii.get_lb().from(bus_pairs))+sqrt(Wii.get_ub().from(bus_pairs)))*(sqrt(Wii.get_lb().to(bus_pairs))+sqrt(Wii.get_ub().to(bus_pairs)))*(sin(phi.in(bus_pairs))*Im_Wij.in(bus_pairs) + cos(phi.in(bus_pairs))*R_Wij.in(bus_pairs));
    LNC2 -=sqrt(Wii.get_lb().to(bus_pairs))*cos(del.in(bus_pairs))*(sqrt(Wii.get_lb().to(bus_pairs))+sqrt(Wii.get_ub().to(bus_pairs)))*Wii.from(bus_pairs);
    LNC2 -=sqrt(Wii.get_lb().from(bus_pairs))*cos(del.in(bus_pairs))*(sqrt(Wii.get_lb().from(bus_pairs))+sqrt(Wii.get_ub().from(bus_pairs)))*Wii.to(bus_pairs);
    LNC2 -=sqrt(Wii.get_lb().from(bus_pairs))*sqrt(Wii.get_lb().to(bus_pairs))*cos(del.in(bus_pairs))*(sqrt(Wii.get_ub().from(bus_pairs))*
                                                                                                       sqrt(Wii.get_ub().to(bus_pairs))-sqrt(Wii.get_lb().from(bus_pairs))*sqrt(Wii.get_lb().to(bus_pairs)));
    SDP.add(LNC2.in(bus_pairs) >= 0);
   // SDPOA.add(LNC2.in(bus_pairs) >= 0);
    
    if(current){
        param<Cpx> T("T"), Y("Y"), Ych("Ych");
        var<Cpx> L_from("L_from"), Wij("Wij");
        T.real_imag(cc.in(arcs), dd.in(arcs));
        Y.real_imag(g.in(arcs), b.in(arcs));
        Ych.set_imag(ch_half.in(arcs));
        
        
        L_from.set_real(lij.in(arcs));
        Wij.real_imag(R_Wij.in_pairs(arcs), Im_Wij.in_pairs(arcs));
        var<Cpx> Sij("Sij"), Sji("Sji");
        Sij.real_imag(Pf_from.in(arcs), Qf_from.in(arcs));
        Sji.real_imag(Pf_to.in(arcs), Qf_to.in(arcs));
        
        //        Constraint<> PLoss("PLoss");
        //        PLoss = pow(Pf_from,2);
        //        PLoss -= pow((g_ff*Wii.from(arcs) + g_ft*R_Wij.in(arcs) + b_ft*Im_Wij.in(arcs)),2);
        //        SDP.add(PLoss.in(arcs)==0,true);
        //        Constraint<> PLoss2("PLoss2");
        //        PLoss2 = pow(Pf_to,2);
        //        PLoss2 -= pow((g_tt*Wii.to(arcs) + g_tf*R_Wij.in(arcs) - b_tf*Im_Wij.in(arcs)),2);
        //        SDP.add(PLoss2.in(arcs)==0,true);
        //        Constraint<> PLoss("PLoss");
        //        PLoss = pow(Pf_from,2) - pow(Pf_to,2);
        //        PLoss -= pow((g_ff*Wii.from(arcs) + g_ft*R_Wij.in(arcs) + b_ft*Im_Wij.in(arcs)),2);
        //        PLoss += pow((g_tt*Wii.to(arcs) + g_tf*R_Wij.in(arcs) - b_tf*Im_Wij.in(arcs)),2);
        //        SDP.add(PLoss.in(arcs)==0,true);
        
        //        Constraint<> QLoss("QLoss");
        //        QLoss = pow(Qf_from,2) - pow(Qf_to,2);
        //        QLoss -= pow((g_ft*Im_Wij.in(arcs) - b_ff*Wii.from(arcs) - b_ft*R_Wij.in(arcs)),2);
        //        QLoss += pow(-1*(b_tt*Wii.to(arcs) + b_tf*R_Wij.in(arcs) + g_tf*Im_Wij.in(arcs)),2);
        //        SDP.add(QLoss.in(arcs)==0,true);
        //        SDP.print();
        
        Constraint<Cpx> I_from("I_from");

        I_from=(Y+Ych)*(conj(Y)+conj(Ych))*Wii.from(arcs)-T*Y*(conj(Y)+conj(Ych))*conj(Wij)-conj(T)*conj(Y)*(Y+Ych)*Wij+pow(tr,2)*Y*conj(Y)*Wii.to(arcs)-pow(tr,2)*L_from;
        SDP.add_real(I_from.in(arcs)==0);
//        SDPOA.add_real(I_from.in(arcs)-pow(tr,2)*L_from==0);



        

        var<Cpx> L_to("L_to");
        L_to.set_real(lji.in(arcs));
        
        Constraint<Cpx> I_to("I_to");
        I_to=pow(tr,2)*(Y+Ych)*(conj(Y)+conj(Ych))*Wii.to(arcs)-conj(T)*Y*(conj(Y)+conj(Ych))*Wij-T*conj(Y)*(Y+Ych)*conj(Wij)+Y*conj(Y)*Wii.from(arcs);
        //SDP.add_real(I_to.in(arcs)==pow(tr,2)*L_to);
        
        Constraint<> I_from_Pf("I_from_Pf");
        I_from_Pf=lij*Wii.from(arcs)-pow(tr,2)*(pow(Pf_from,2) + pow(Qf_from,2));
        SDP.add(I_from_Pf.in(arcs)==0, true);
        //SDPOA.add(I_from_Pf.in(arcs)==0, true);

        
        Constraint<> I_to_Pf("I_to_Pf");
        I_to_Pf=lji*Wii.to(arcs)-(pow(Pf_to,2) + pow(Qf_to, 2));
        //    SDP.add(I_to_Pf.in(arcs)==0, true);
        
    }
    
    
    
    total_time_start = get_wall_time();
    /* Solver selection */
    solver<> SDPOPF(SDP,solv_type);
    double solver_time_start = get_wall_time();
    
    //    SDP.print();
    SDPOPF.run(output = 5, tol = 1e-7, "ma27");
    //    SDP.print_solution();
    SDP.print_constraints_stats(tol);
    SDP.print_nonzero_constraints(tol,true);
    auto lower_bound = SDP.get_obj_val()*upper_bound;
    SDP.print_solution();
    
    
    
    
    
    
    //First iteration
    //Possible improvements: Find interior point via optimization when point is an active point
    //Best way to do this is to rewrite the model as g(x)<=\eta y and set y (a flag) for each con
    //wait for Hassan to do this
    //generate as many OA iterative cuts as given by num_iter_cuts
    
        
    
    
    

    SDP.print();


//    bool interior=false;
//    pair<vector<double>,bool> xactive;
//
//    vector<vector<double>> xouter_array, xactive_array;
//    vector<double> xsolution;
//    int counter;
//    double xv;
//    const double active_tol=1e-6;
//    string cname;
//
//    for (auto &it: SDP._vars)
//    {
//        auto v = it.second;
//        if(!SDPOA.has_var(*v)){
//            SDPOA.add_var(v);
//        }
//    }
//
//    for (auto &con: SDP._cons_vec)
//    {
//        cname=con->_name;
//        if(!con->is_linear())
//        {
//           // DebugOn("Nonlinear "<< cname<<endl);
//            for(auto i=0;i<con->get_nb_inst();i++)
//
//            {
//
//                con->uneval();
//
//                if(abs(con->eval(i))<=active_tol ||( con->is_convex() && !con->is_rotated_soc() && !con->check_soc()))
//                {
//                    con->uneval();
//                    func<> oacon=con->get_outer_app_insti(i);
//                    oacon.eval_all();
//                    Constraint<> OA_sol("OA_cuts_solution"+cname+to_string(i));
//                    OA_sol=oacon;
////                    for (auto &it: *oacon._vars)
////                    {
////                        auto v = it.second.first;
////                        if(!SDPOA.has_var(*v)){
////                            SDPOA.add_var(v);
////                        }
////                    }
//                    //Assuming no nonlinear equality constraints
//                    if(con->_ctype==leq)
//                        SDPOA.add(OA_sol<=0);
//                    else if(con->_ctype==geq)
//                        SDPOA.add(OA_sol>=0);
//
//                    oacon.uneval();
//
//
//
//                }
//                else //If constraint is not active xsolution is an interior point
//                {
//                    xsolution.clear();
//                    for (auto &it: *con->_vars)
//                    {
//                        auto v = it.second.first;
//                        size_t posv=v->get_id_inst(i);
//                        v->get_double_val(posv, xv);
//                        xsolution.push_back(xv);
//                    }
//
//                    xactive_array= con->get_active_point(i,  con->_ctype);
//
//                    for(auto j=0;j<xactive_array.size();j++)
//                        //                    //for(auto j=0;j<1;j++)
//                    {
//                        if(xactive_array[j].size()>0)
//                        {
//                            con->uneval();
//
//                            counter=0;
//                            for (auto &it: *con->_vars)
//                            {
//                                auto v = it.second.first;
//                                size_t posv=v->get_id_inst(i);
//                                v->set_double_val(posv,xactive_array[j][counter++]);
//                            }
//                            con->uneval();
//                            func<> oa_iter=con->get_outer_app_insti(i);
//                            oa_iter.eval_all();
//                            Constraint<> OA_itercon("OA_cuts_iterative "+cname+to_string(i)+","+to_string(j));
//                            OA_itercon=oa_iter;
////                            for (auto &it: *oa_iter._vars)
////                            {
////                                auto v = it.second.first;
////                                if(!SDPOA.has_var(*v)){
////                                    SDPOA.add_var(v);
////                                }
////                            }
//                            if(con->_ctype==leq)
//                                SDPOA.add(OA_itercon<=0);
//                            else if(con->_ctype==geq)
//                                SDPOA.add(OA_itercon>=0);
//
//                        }
//                    }
//
//
//
////                    SDPOA.add(OA_itercon>=0);
//                    counter=0;
//                    for (auto &it: *con->_vars)
//                    {
//                        auto v = it.second.first;
//                        size_t posv=v->get_id_inst(i);
//                        v->set_double_val(posv, xsolution[counter++]);
//                    }
//                    //                            oas.uneval();
//                    //                            DebugOn("oas.eval(0)\t"<<oas.eval(0)<<endl)
//
//
//
//
//
//                }
//            }
//        }
//        else
//        {
////            Constraint<> lin(cname);
////                        lin=*con;
////                        if(con->_ctype==leq)
////            for (auto &it: *con->_vars)
////            {
////                auto v = it.second.first;
////                if(!SDPOA.has_var(*v)){
////                    SDPOA.add_var(v);
////                }
////            }
//            SDPOA.add(*con);
////                        else if(con->_ctype==geq)
////                            SDPOA.add(lin>=0);
////                        else
////                            SDPOA.add(lin==0);
//        }
//    }
//
//
//
    
//    auto SDPOA=SDP.buildOA();
//    SDPOA->print();
//
//
//    solver<> SDPOPFA(SDPOA,cplex);
//    solver_time_start = get_wall_time();
//
//    SDPOPFA.run(output = 5, tol = 1e-6);
//    SDPOA->print_solution();
//     SDPOA->print_constraints_stats(tol);
    

    
    
    
    
  



    

    
    double gap = 100*(upper_bound - lower_bound)/upper_bound;
    double solver_time_end = get_wall_time();
    double total_time_end = get_wall_time();
    auto solve_time = solver_time_end - solver_time_start;
    auto total_time = total_time_end - total_time_start;
    string out = "\nDATA_OPF, " + grid._name + ", " + to_string(nb_buses) + ", " + to_string(nb_lines) +", " + to_string(lower_bound) + ", " + to_string(-numeric_limits<double>::infinity()) + ", " + to_string(solve_time) + ", LocalOptimal, " + to_string(total_time);
    DebugOn(out <<endl);
    DebugOn("Final Gap = " << to_string(gap) << "%."<<endl);
    DebugOn("Upper bound = " << to_string(upper_bound) << "."<<endl);
    DebugOn("Lower bound = " << to_string(lower_bound) << "."<<endl);
   // lower_bound = SDPOA->get_obj_val()*upper_bound;
    gap = 100*(upper_bound - lower_bound)/upper_bound;
    DebugOn("Final Gap with OA model = " << to_string(gap) << "%."<<endl);
    DebugOn("Upper bound = " << to_string(upper_bound) << "."<<endl);
    DebugOn("Lower bound = " << to_string(lower_bound) << "."<<endl);
    DebugOn("\nResults: " << grid._name << " " << to_string(lower_bound) << " " << to_string(total_time)<<endl);
    
    // solver<> SDPOAS(SDPOA,solv_type);
    
    // SDP.print();
    //    SDPOAS.run(output = 5, tol = 1e-6);
    //    SDPOA.print_constraints_stats(tol);
    //    SDPOA.print_nonzero_constraints(tol,true);
    //    upper_bound = SDPOA.get_obj_val();
    //    gap = 100*(upper_bound - lower_bound)/upper_bound;
    //    DebugOn("Final Gap = " << to_string(gap) << "%."<<endl);
    //    DebugOn("Upper bound = " << to_string(upper_bound) << "."<<endl);
    //    DebugOn("Lower bound = " << to_string(lower_bound) << "."<<endl);
    //
    //        string result_name="/Users/smitha/Desktop/Results/SDPCUTS.txt";
    //        ofstream fout(result_name.c_str(), ios_base::app);
    //    fout<<grid._name<<"\t"<<std::fixed<<std::setprecision(5)<<gap<<"\t"<<std::setprecision(5)<<upper_bound<<"\t"<<std::setprecision(5)<<SDP.get_obj_val()<<"\t"<<std::setprecision(5)<<solve_time<<endl;
    
     SDP.print_solution();
    
    
    auto SDPOA= SDP.buildOA(4, 4);
    
//    auto con=SDP.get_constraint("Im_Wij,Im_Wij_McCormick_squared");
//    
//    con->get_outer_app_squared();
//    
//    SDPOA->get_outer_app_uniform(10, *con);
//    
//    solver<> SDPOPFA(SDPOA,ipopt);
//    SDPOPFA.run(output = 5, tol = 1e-6, "ma27");
  
    
    SDPOA->print();
    
   // con->get_outer_app_uniform(4);
    
    
//    DebugOn("SOC or not "<< con->check_soc());
//    int nb_discr=4;
//    func<> res;
//    bool flag;
//    vector<double> d;
//
//        for(auto i=0;i<con->get_nb_instances();i++)
//        {
//            for (auto d1=0;d1<nb_discr;d1++)
//            {d.clear();
//                d.push_back(d1);
//            flag=con->get_grid_discretize(nb_discr, i, d);
//            if(flag){
//            res=con->get_outer_app_insti(i);
//            res.print();
//            }
//        }
//    }
    
   
    
    return 0;
    
}
