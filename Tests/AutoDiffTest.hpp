/* 
 * File:   AutoDiffTests.hpp
 * Author: matthewsupernaw
 *
 * Created on June 15, 2015, 8:08 AM
 */
/*
 * File:   AutoDiffTests.hpp
 * Author: matthewsupernaw
 *
 * Created on June 15, 2015, 8:08 AM
 */

#ifndef HESSIANTESTS_HPP
#define HESSIANTESTS_HPP

#include <valarray>
#include <fstream>
#include <sstream>
#include <iostream>
#include <random>
#include "../ATL.hpp"

#define HESSIAN_USE_AD_GRADIENT

namespace atl {
    namespace tests {
        namespace auto_diff {
            
            int fail = 0;
            int tests = 0;
            
            template<class T>
            class AutoDiffTest {
                typedef atl::Variable<T> var;
                std::vector<var*> active_parameters_m;
                std::vector<uint32_t> pids;
                static T tolerance;
                static T second_tolerance;
                static T third_tolerance;
            public:
                
                operator int() {
                    return fail;
                }
                
                AutoDiffTest() {
                }
                
                AutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                static T GetTolerance() {
                    return tolerance;
                }
                
                static void SetTolerance(T tolerance) {
                    AutoDiffTest::tolerance = tolerance;
                }
                
                static T GetSecondOrderTolerance() {
                    return AutoDiffTest::second_tolerance;
                }
                
                static void SetSecondOrderTolerance(T tolerance) {
                    AutoDiffTest::second_tolerance = tolerance;
                }
                
                static T GetThirdOrderTolerance() {
                    return AutoDiffTest::third_tolerance;
                }
                
                static void SetThirdOrderTolerance(T tolerance) {
                    AutoDiffTest::third_tolerance = tolerance;
                }
                
                void Register(var& v) {
                    this->active_parameters_m.push_back(&v);
                    this->pids.push_back(v.info->id);
                }
                
                virtual void Initialize() {
                    
                }
                
                virtual void ObjectiveFunction(atl::Variable<T>& f) {
                    
                }
                
                
                virtual void Description(std::stringstream& out) {
                    
                }
                
                void RunTestToFile(std::ofstream& out) {
                    
                    tests += 2;
                    var::gradient_structure_g.Reset();
                    var::SetRecording(true);
                    var::gradient_structure_g.derivative_trace_level = FIRST_ORDER;
                    this->Initialize();
                    var f;
                    //
                    T hmin;
                    T hmax;
                    T gmin;
                    T gmax;
                    //                    //                    //
                    std::stringstream ss;
                    this->Description(ss);
                    out << "\n\n" << ss.str() << "\n";
                    
                    std::cout << "\n\n" << ss.str() << "\n";
                    T fval;
                    //                std::cout.precision(50);
                    std::vector<T> gradient(this->active_parameters_m.size());
                    
                    out << "Number of Parameters: " << this->active_parameters_m.size() << "\n";
                    
                    
                    std::cout << "evaluating..." << std::flush;
                    
                    
                    
                    auto eval_gstart = std::chrono::steady_clock::now();
                    this->ObjectiveFunction(f);
                    auto eval_gend = std::chrono::steady_clock::now();
                    std::chrono::duration<double> eval_gtime = eval_gend - eval_gstart;
                    
                    fval = f.GetValue();
                    
                    std::cout << "computing exact gradient..." << std::flush;
                    auto exact_gstart = std::chrono::steady_clock::now();
                    var::gradient_structure_g.Accumulate();
                    auto exact_gend = std::chrono::steady_clock::now();
                    std::chrono::duration<double> exact_gtime = (exact_gend - exact_gstart);
                    
                    for (int i = 0; i < this->active_parameters_m.size(); i++) {
                        gradient[i] = this->active_parameters_m[i]->info->dvalue;
                    }
                    
                    
                    var::gradient_structure_g.Reset();
                    std::cout << "done!\nevaluating..." << std::flush;
                    
                    f = 0.0;
                    var::gradient_structure_g.derivative_trace_level = atl::SECOND_ORDER_MIXED_PARTIALS;
                    
                    //                    //run hessian twice to make sure everything resets properly
                    //                    this->ObjectiveFunction(f);
                    //                    var::gradient_structure_g.HessianAndGradientAccumulate();
                    //                    var::gradient_structure_g.Reset();
                    
                    f = 0.0;
                    auto eval_start = std::chrono::steady_clock::now();
                    this->ObjectiveFunction(f);
                    auto eval_end = std::chrono::steady_clock::now();
                    std::chrono::duration<double> eval_time = eval_end - eval_start;
                    
                    fval = f.GetValue();
                    
                    std::cout << "computing exact hessian..." << std::flush;
                    auto exact_start = std::chrono::steady_clock::now();
                    var::gradient_structure_g.AccumulateSecondOrderMixed();
                    auto exact_end = std::chrono::steady_clock::now();
                    std::chrono::duration<double> exact_time = (exact_end - exact_start);
                    std::vector<std::vector<T> > exact_hessian(this->active_parameters_m.size(), std::vector<T> (this->active_parameters_m.size()));
                    for (int i = 0; i < this->active_parameters_m.size(); i++) {
                        //                        gradient[i] = this->active_parameters_m[i]->info->dvalue;
                        for (int j = 0; j < this->active_parameters_m.size(); j++) {
#ifdef USE_EIGEN
                            
                            exact_hessian[i][j] = var::gradient_structure_g.Value(this->active_parameters_m[i]->info->id, this->active_parameters_m[j]->info->id); //this->active_parameters_m[i]->info->hessian_row(this->active_parameters_m[j]->info->id);
#else
                            exact_hessian[i][j] = var::gradient_structure_g.Value(this->active_parameters_m[i]->info->id, this->active_parameters_m[j]->info->id); //this->active_parameters_m[i]->info->hessian_row(this->active_parameters_m[j]->info->id);this->active_parameters_m[i]->info->hessian_row[this->active_parameters_m[j]->info->id];
#endif
                        }
                    }
                    std::cout << "done!\n";
                    std::cout << std::scientific;
                    
                    var::gradient_structure_g.Reset();
                    std::cout << "estimating hessian..." << std::flush;
                    auto estimated_start = std::chrono::steady_clock::now();
                    std::valarray<std::valarray<T> > estimated_hessian = this->EstimatedHessian();
                    auto estimated_end = std::chrono::steady_clock::now();
                    std::chrono::duration<double> estimated_time = (estimated_end - estimated_start);
                    
                    std::cout << "done\n";
                    T sum = 0;
                    
                    for (int i = 0; i < exact_hessian.size(); i++) {
                        for (int j = 0; j < exact_hessian[0].size(); j++) {
                            T diff = (exact_hessian[i][j] - estimated_hessian[i][j]);
                            if (i == 0 && j == 0) {
                                hmax = diff;
                                hmin = diff;
                            } else {
                                if (diff > hmax) {
                                    hmax = diff;
                                }
                                if (diff < hmin) {
                                    hmin = diff;
                                }
                            }
                            
                            sum += (exact_hessian[i][j] - estimated_hessian[i][j])* (exact_hessian[i][j] - estimated_hessian[i][j]);
                        }
                    }
                    
                    T mse = sum / (exact_hessian.size() * exact_hessian.size());
                    
                    var::gradient_structure_g.Reset();
                    auto estimatedg_start = std::chrono::steady_clock::now();
                    std::valarray<T> estimated_gradient = this->EstimateGradient();
                    auto estimatedg_end = std::chrono::steady_clock::now();
                    std::chrono::duration<double> estimatedg_time = (estimatedg_end - estimatedg_start);
                    
                    T gsum = 0;
                    for (int i = 0; i < gradient.size(); i++) {
                        T diff = (gradient[i] - estimated_gradient[i]);
                        if (i == 0) {
                            gmax = diff;
                            gmin = diff;
                        } else {
                            if (diff > gmax) {
                                gmax = diff;
                            }
                            if (diff < gmin) {
                                gmin = diff;
                            }
                        }
                        
                        gsum += (gradient[i] - estimated_gradient[i])*(gradient[i] - estimated_gradient[i]);
                    }
                    T gmse = gsum / gradient.size();
                    
                    
                    
                    
                    atl::Variable<T> tf;
                    atl::Variable<T>::gradient_structure_g.recording = true;
                    atl::Variable<T>::gradient_structure_g.derivative_trace_level = atl::THIRD_ORDER_MIXED_PARTIALS;
                    atl::Variable<T>::gradient_structure_g.Reset();
                    std::cout << "evaluating..." << std::flush;
                    
                    auto eval_to_start = std::chrono::steady_clock::now();
                    this->ObjectiveFunction(tf);
                    auto eval_to_end = std::chrono::steady_clock::now();
                    std::chrono::duration<double> eval_to_time = (eval_to_end - eval_to_start);
                    
                    std::cout << "computing exact third order mixed..." << std::flush;
                    
                    auto exact_to_start = std::chrono::steady_clock::now();
                    atl::Variable<T>::gradient_structure_g.AccumulateThirdOrderMixed();
                    auto exact_to_end = std::chrono::steady_clock::now();
                    std::chrono::duration<double> exact_to_time = (exact_to_end - exact_to_start);
                    std::cout << "done.\n" << std::flush;
                    
                    std::valarray< std::valarray<std::valarray < T> > > third_mixed_exact(std::valarray<std::valarray < T> > (std::valarray<T > (active_parameters_m.size()), active_parameters_m.size()), active_parameters_m.size());
                    
                    for (int i = 0; i < this->active_parameters_m.size(); i++) {
                        for (int j = 0; j < this->active_parameters_m.size(); j++) {
                            for (int k = 0; k < this->active_parameters_m.size(); k++) {
                                third_mixed_exact[i][j][k] = atl::Variable<T>::gradient_structure_g.Value(this->active_parameters_m[i]->info->id, this->active_parameters_m[j]->info->id, this->active_parameters_m[k]->info->id);
                            }
                        }
                    }
                    
                    std::valarray< std::valarray<std::valarray < T> > > third_mixed_estimated(std::valarray<std::valarray < T> > (std::valarray<T > (active_parameters_m.size()), active_parameters_m.size()), active_parameters_m.size());
                    std::cout << "estimating third order mixed..." << std::flush;
                    
                    auto estimated_to_start = std::chrono::steady_clock::now();
                    third_mixed_estimated = this->EstimateThirdOrderMixed();
                    auto estimated_to_end = std::chrono::steady_clock::now();
                    std::chrono::duration<double> estimated_to_time = (estimated_to_end - estimated_to_start);
                    std::cout << "done.\n" << std::flush;
                    
                    T to_sum = 0;
                    for (int i = 0; i < this->active_parameters_m.size(); i++) {
                        for (int j = 0; j < this->active_parameters_m.size(); j++) {
                            for (int k = 0; k < this->active_parameters_m.size(); k++) {
                                to_sum += third_mixed_exact[i][j][k] - third_mixed_estimated[i][j][k];
                                //                                if (std::fabs(third_mixed_exact[i][j][k] - third_mixed_estimated[i][j][k]) > 1e-2) {
                                //                                    std::cout << "{" << this->active_parameters_m[i]->info->id << "," << this->active_parameters_m[j]->info->id << "," << this->active_parameters_m[k]->info->id << "}" << "third order failed " << third_mixed_exact[i][j][k] << " != " << third_mixed_estimated[i][j][k] << " <-----\n";
                                //                                }
                                //                                    else {
                                //                                        std::cout << "{" << this->active_parameters_m[i]->info->id << "," << this->active_parameters_m[j]->info->id << "," << this->active_parameters_m[k]->info->id << "}" << "third order PASSED " << third_mixed_exact[i][j][k] << " == " << third_mixed_estimated[i][j][k] << "\n";
                                //                                    }
                            }
                        }
                    }
                    
                    T to_mse = to_sum / (exact_hessian.size() * exact_hessian.size() * exact_hessian.size());
                    
                    
                    
                    if (gmse <= AutoDiffTest<T>::tolerance) {
                        std::cout << "Gradient Test Passed.\n";
                        out << "Gradient  Test Passed.\n";
                    } else {
                        fail++;
                        std::cout << "Gradient  Test Failed(" << gmse << ">" << AutoDiffTest<T>::tolerance << ")\n";
                        out << "Gradient Test Failed.\n";
                    }
                    
                    if (mse <= AutoDiffTest<T>::second_tolerance) {
                        std::cout << "Hessian Test Passed.\n";
                        out << "Hessian  Test Passed.\n";
                    } else {
                        fail++;
                        std::cout << "Hessian  Test Failed(" << mse << ">" << AutoDiffTest<T>::tolerance << ")\n";
                        out << "Hessian Test Failed.\n";
                    }
                    
                    
                    if (to_mse <= AutoDiffTest<T>::third_tolerance) {
                        std::cout << "Third-Order Test Passed.\n";
                        out << "Third-Order Test Passed.\n";
                    } else {
                        fail++;
                        std::cout << "Third-Order Test Failed(" << to_mse << ">" << AutoDiffTest<T>::tolerance << ")\n";
                        out << "Third-Order Test Failed.\n";
                    }
                    
                    out << "Function value: " << fval << "\n";
                    out << "Gradient mse = " << gmse << ", Error Range{" << gmin << " - " << gmax << "}\n";
                    out << "Hessian mse = " << mse << ", Error Range{" << hmin << " - " << hmax << "}\n";
                    out << "Third-Order mse = " << to_mse << "\n"; // ", Error Range{" << hmin << " - " << hmax << "}\n";
                    out << std::fixed;
                    out << "Time to evaluate objective function with only gradient info: " << eval_gtime.count() << " sec\n";
                    out << "Time to evaluate objective function with Hessian info: " << eval_time.count() << " sec\n";
                    out << "Time to evaluate objective function with Third-Order info: " << eval_to_time.count() << " sec\n";
                    out << "Time to compute exact gradient: " << exact_gtime.count() << " sec\n";
                    out << "Time to compute exact gradient and Hessian: " << exact_time.count() << " sec\n";
                    out << "Time to compute exact gradient, Hessian, and Third-Order: " << exact_to_time.count() << " sec\n";
                    out << "Time to estimate gradient: " << estimatedg_time.count() << " sec\n";
                    out << "Time to estimate Hessian: " << estimated_time.count() << " sec\n";
                    out << "Time to estimate Third-Order: " << estimated_to_time.count() << " sec\n";
                    out << "Gradient Speed up = " << estimatedg_time / exact_gtime << "\n";
                    out << "Hessain Speed up = " << estimated_time / exact_time << "\n\n";
                    out << "Third-Order Speed up = " << estimated_to_time / exact_to_time << "\n\n";
                    std::cout << std::fixed;
                    std::cout << "Gradient Speed up = " << estimatedg_time / exact_gtime << "\n";
                    std::cout << "Hessain Speed up = " << estimated_time / exact_time << "\n";
                    std::cout << "Third-Order Speed up = " << estimated_to_time / exact_to_time << "\n\n";
                    
                    out << "Estimated gradient:\n";
                    for (int i = 0; i < gradient.size(); i++) {
                        out << estimated_gradient[i] << " ";
                    }
                    out << "\n\n";
                    
                    out << "Exact gradient:\n";
                    for (int i = 0; i < gradient.size(); i++) {
                        out << gradient[i] << " ";
                    }
                    out << "\n\n";
                    
                    
                    out << "Gradient difference:\n";
                    for (int i = 0; i < gradient.size(); i++) {
                        out << gradient[i] - estimated_gradient[i] << " ";
                    }
                    out << "\n\n";
                    
                    out << "Estimated Hessian: \n";
                    for (int i = 0; i < estimated_hessian.size(); i++) {
                        for (int j = 0; j < estimated_hessian[0].size(); j++) {
                            out << estimated_hessian[i][j] << " ";
                        }
                        out << std::endl;
                    }
                    
                    
                    //
                    out << "\nExact Hessian:\n";
                    for (int i = 0; i < exact_hessian.size(); i++) {
                        for (int j = 0; j < exact_hessian[0].size(); j++) {
                            out << exact_hessian[i][j] << " ";
                        }
                        out << std::endl;
                        
                    }
                    
                    out << "\nHessian difference:\n";
                    for (int i = 0; i < exact_hessian.size(); i++) {
                        for (int j = 0; j < exact_hessian[0].size(); j++) {
                            out << (exact_hessian[i][j] - estimated_hessian[i][j]) << " ";
                        }
                        out << std::endl;
                    }
                    
                    out << "\nEstimated Third-Order:\n";
                    for (int i = 0; i < this->active_parameters_m.size(); i++) {
                        
                        for (int j = 0; j < this->active_parameters_m.size(); j++) {
                            out << "[" << i << "] ";
                            for (int k = 0; k < this->active_parameters_m.size(); k++) {
                                out << third_mixed_estimated[i][j][k] << " ";
                            }
                            out << "\n";
                        }
                    }
                    
                    out << "\nExact Third-Order:\n";
                    for (int i = 0; i < this->active_parameters_m.size(); i++) {
                        
                        for (int j = 0; j < this->active_parameters_m.size(); j++) {
                            out << "[" << i << "] ";
                            for (int k = 0; k < this->active_parameters_m.size(); k++) {
                                out << third_mixed_exact[i][j][k] << " ";
                            }
                            out << "\n";
                        }
                    }
                    
                    
                    out << "\nThird-Order difference:\n";
                    for (int i = 0; i < this->active_parameters_m.size(); i++) {
                        
                        for (int j = 0; j < this->active_parameters_m.size(); j++) {
                            out << "[" << i << "] ";
                            for (int k = 0; k < this->active_parameters_m.size(); k++) {
                                out << (third_mixed_exact[i][j][k] - third_mixed_estimated[i][j][k]) << " ";
                            }
                            out << "\n";
                        }
                    }
                    
                    
                }
                
                const std::valarray<T> EstimateGradient() {
                    var::gradient_structure_g.Reset();
                    atl::Variable<T>::SetRecording(false);
                    std::valarray<T> gradient(active_parameters_m.size());
                    atl::Variable<T> f;
                    this->ObjectiveFunction(f);
                    atl::Variable<T> fh;
                    T fv;
                    T delta = 1.e-5;
                    for (int i = 0; i < active_parameters_m.size(); i++) {
                        active_parameters_m[i]->SetValue(active_parameters_m[i]->GetValue() + delta);
                        this->ObjectiveFunction(fh);
                        fv = fh.GetValue();
                        active_parameters_m[i]->SetValue(active_parameters_m[i]->GetValue() - 2 * delta);
                        this->ObjectiveFunction(fh);
                        gradient[i] = (fv - fh.GetValue()) / (2.0 * delta);
                        active_parameters_m[i]->SetValue(active_parameters_m[i]->GetValue() + delta);
                    }
                    return gradient;
                }
                
                /**
                 * This function is a port of from admb.
                 * @return
                 */
                const std::valarray<std::valarray<T> > EstimatedHessian() {
                    
                    atl::Variable<T>::SetRecording(true);
                    atl::Variable<T>::gradient_structure_g.derivative_trace_level = GRADIENT;
                    atl::Variable<T>::gradient_structure_g.Reset();
                    std::valarray<std::valarray<T> > hessian(
                                                             std::valarray<T > (active_parameters_m.size()), active_parameters_m.size());
                    T delta = 1.e-3;
                    std::valarray < T> g1(active_parameters_m.size());
                    std::valarray < T> g2(active_parameters_m.size());
                    //            std::valarray < T> hess(active_parameters_m.size());
                    std::valarray < T> hess1(active_parameters_m.size());
                    std::valarray < T> hess2(active_parameters_m.size());
                    T eps = .1;
                    T sdelta1;
                    T sdelta2;
                    
                    atl::Variable<T> f;
                    
                    atl::Variable<T>::gradient_structure_g.Reset();
                    for (int i = 0; i < active_parameters_m.size(); i++) {
                        
                        
                        
                        T xsave = active_parameters_m[i]->GetValue();
                        sdelta1 = /*active_parameters_m[i]->GetValue()*/ +delta;
                        //                sdelta1 -= active_parameters_m[i]->GetValue();
                        active_parameters_m[i]->SetValue(xsave + sdelta1);
#ifdef HESSIAN_USE_AD_GRADIENT
                        ObjectiveFunction(f);
                        atl::Variable<T>::gradient_structure_g.Accumulate();
                        for (int j = 0; j < active_parameters_m.size(); j++) {
                            g1[j] = active_parameters_m[j]->info->dvalue;
                        }
                        
                        atl::Variable<T>::gradient_structure_g.Reset();
#else
                        g1 = this->EstimateGradient();
#endif
                        f = 0.0;
                        sdelta2 = /*active_parameters_m[i]->GetValue()*/ -delta;
                        //                sdelta2 -= active_parameters_m[i]->GetValue();
                        active_parameters_m[i]->SetValue(xsave + sdelta2);
#ifdef HESSIAN_USE_AD_GRADIENT
                        ObjectiveFunction(f);
                        atl::Variable<T>::gradient_structure_g.Accumulate();
                        for (int j = 0; j < active_parameters_m.size(); j++) {
                            g2[j] = active_parameters_m[j]->info->dvalue;
                        }
                        atl::Variable<T>::gradient_structure_g.Reset();
#else
                        g2 = this->EstimateGradient();
#endif
                        active_parameters_m[i]->SetValue(xsave);
                        
                        hess1 = (g1 - g2) / (sdelta1 - sdelta2);
                        
                        
                        sdelta1 = /*active_parameters_m[i]->GetValue() +*/ eps*delta;
                        //                sdelta1 -= active_parameters_m[i]->GetValue();
                        active_parameters_m[i]->SetValue(xsave + sdelta1);
#ifdef HESSIAN_USE_AD_GRADIENT
                        f = 0.0;
                        ObjectiveFunction(f);
                        atl::Variable<T>::gradient_structure_g.Accumulate();
                        for (int j = 0; j < active_parameters_m.size(); j++) {
                            g1[j] = active_parameters_m[j]->info->dvalue;
                        }
                        atl::Variable<T>::gradient_structure_g.Reset();
#else
                        g1 = this->EstimateGradient();
#endif
                        
                        active_parameters_m[i]->SetValue(xsave - eps * delta);
                        sdelta2 = /*active_parameters_m[i]->GetValue()*/ -eps*delta;
                        //                sdelta2 -= active_parameters_m[i]->GetValue();
                        active_parameters_m[i]->SetValue(xsave + sdelta2);
#ifdef HESSIAN_USE_AD_GRADIENT
                        f = 0.0;
                        ObjectiveFunction(f);
                        atl::Variable<T>::gradient_structure_g.Accumulate();
                        
                        
                        for (int j = 0; j < active_parameters_m.size(); j++) {
                            g2[j] = active_parameters_m[j]->info->dvalue;
                        }
                        atl::Variable<T>::gradient_structure_g.Reset();
#else
                        g2 = this->EstimateGradient();
#endif
                        active_parameters_m[i]->SetValue(xsave);
                        
                        
                        T eps2 = eps*eps;
                        hess2 = (g1 - g2) / (sdelta1 - sdelta2);
                        hessian[i] = (eps2 * hess1 - hess2) / (eps2 - 1.);
                    }
                    
                    return hessian;
                    
                }
                
                const std::valarray<std::valarray<std::valarray<T> > > EstimateThirdOrderMixed() {
                    atl::Variable<T>::SetRecording(true);
                    atl::Variable<T>::gradient_structure_g.derivative_trace_level = atl::SECOND_ORDER_MIXED_PARTIALS;
                    atl::Variable<T>::gradient_structure_g.Reset();
                    
                    std::valarray< std::valarray<std::valarray < T> > > third_mixed(std::valarray<std::valarray < T> > (std::valarray<T > (active_parameters_m.size()), active_parameters_m.size()), active_parameters_m.size());
                    
                    T delta = 1.e-2;
                    T eps = .1;
                    T sdelta1;
                    T sdelta2;
                    atl::Variable<T> f;
                    
                    atl::Variable<T>::gradient_structure_g.Reset();
                    std::valarray < std::valarray < T> > h1(std::valarray<T > (active_parameters_m.size()), active_parameters_m.size());
                    std::valarray < std::valarray < T >> h2(std::valarray<T > (active_parameters_m.size()), active_parameters_m.size());
                    //            std::valarray < T> hess(active_parameters_m.size());
                    std::valarray < std::valarray < T >> t1(std::valarray<T > (active_parameters_m.size()), active_parameters_m.size());
                    std::valarray < std::valarray < T >> t2(std::valarray<T > (active_parameters_m.size()), active_parameters_m.size());
                    
                    std::valarray < std::valarray < T> > sd1(std::valarray<T > (active_parameters_m.size()), active_parameters_m.size());
                    std::valarray < std::valarray < T >> sd2(std::valarray<T > (active_parameters_m.size()), active_parameters_m.size());
                    std::valarray < std::valarray < T> > sd3(std::valarray<T > (active_parameters_m.size()), active_parameters_m.size());
                    std::valarray < std::valarray < T >> sd4(std::valarray<T > (active_parameters_m.size()), active_parameters_m.size());
                    std::valarray < std::valarray < T >> eps_squared(std::valarray<T > (active_parameters_m.size()), active_parameters_m.size());
                    std::valarray < std::valarray < T >> eps_squared_minus_one(std::valarray<T > (active_parameters_m.size()), active_parameters_m.size());
                    for (int i = 0; i < active_parameters_m.size(); i++) {
                        for (int j = 0; j < active_parameters_m.size(); j++) {
                            sd1[i][j] = delta;
                            sd2[i][j] = -1.0 * delta;
                            sd3[i][j] = eps*delta;
                            sd4[i][j] = -1.0 * eps*delta;
                            eps_squared[i][j] = eps*eps;
                            eps_squared_minus_one[i][j] = eps_squared[i][j] - 1.0;
                        }
                    }
                    
                    
                    for (int i = 0; i < this->active_parameters_m.size(); i++) {
                        
                        
                        T xsave = active_parameters_m[i]->GetValue();
                        sdelta1 = /*active_parameters_m[i]->GetValue()*/ +delta;
                        //                sdelta1 -= active_parameters_m[i]->GetValue();
                        active_parameters_m[i]->SetValue(xsave + sdelta1);
#ifdef HESSIAN_USE_AD_GRADIENT
                        ObjectiveFunction(f);
                        atl::Variable<T>::gradient_structure_g.AccumulateSecondOrderMixed();
                        for (int j = 0; j < active_parameters_m.size(); j++) {
                            for (int k = 0; k < active_parameters_m.size(); k++) {
                                h1[j][k] = atl::Variable<T>::gradient_structure_g.Value(active_parameters_m[j]->info->id, active_parameters_m[k]->info->id); //active_parameters_m[j]->info->hessian_row[active_parameters_m[k]->info->id];
                            }
                        }
                        
                        atl::Variable<T>::gradient_structure_g.Reset();
#else
                        h1 = this->EstimatedHessian();
#endif
                        f = 0.0;
                        sdelta2 = /*active_parameters_m[i]->GetValue()*/ -delta;
                        //                sdelta2 -= active_parameters_m[i]->GetValue();
                        active_parameters_m[i]->SetValue(xsave + sdelta2);
#ifdef HESSIAN_USE_AD_GRADIENT
                        ObjectiveFunction(f);
                        atl::Variable<T>::gradient_structure_g.AccumulateSecondOrderMixed();
                        for (int j = 0; j < active_parameters_m.size(); j++) {
                            for (int k = 0; k < active_parameters_m.size(); k++) {
                                h2[j][k] = atl::Variable<T>::gradient_structure_g.Value(active_parameters_m[j]->info->id, active_parameters_m[k]->info->id); //active_parameters_m[j]->info->hessian_row[active_parameters_m[k]->info->id];
                            }
                        }
                        atl::Variable<T>::gradient_structure_g.Reset();
#else
                        h2 = this->EstimatedHessian();
#endif
                        active_parameters_m[i]->SetValue(xsave);
                        
                        
                        t1 = (h1 - h2) / (sd1 - sd2);
                        
                        
                        sdelta1 = /*active_parameters_m[i]->GetValue() +*/ eps*delta;
                        //                sdelta1 -= active_parameters_m[i]->GetValue();
                        active_parameters_m[i]->SetValue(xsave + sdelta1);
#ifdef HESSIAN_USE_AD_GRADIENT
                        f = 0.0;
                        ObjectiveFunction(f);
                        atl::Variable<T>::gradient_structure_g.AccumulateSecondOrderMixed();
                        for (int j = 0; j < active_parameters_m.size(); j++) {
                            for (int k = 0; k < active_parameters_m.size(); k++) {
                                h1[j][k] = atl::Variable<T>::gradient_structure_g.Value(active_parameters_m[j]->info->id, active_parameters_m[k]->info->id); //active_parameters_m[j]->info->hessian_row[active_parameters_m[k]->info->id];
                            }
                        }
                        atl::Variable<T>::gradient_structure_g.Reset();
#else
                        h1 = this->EstimatedHessian();
#endif
                        
                        active_parameters_m[i]->SetValue(xsave - eps * delta);
                        sdelta2 = /*active_parameters_m[i]->GetValue()*/ -eps*delta;
                        //                sdelta2 -= active_parameters_m[i]->GetValue();
                        active_parameters_m[i]->SetValue(xsave + sdelta2);
#ifdef HESSIAN_USE_AD_GRADIENT
                        f = 0.0;
                        ObjectiveFunction(f);
                        atl::Variable<T>::gradient_structure_g.AccumulateSecondOrderMixed();
                        
                        for (int j = 0; j < active_parameters_m.size(); j++) {
                            for (int k = 0; k < active_parameters_m.size(); k++) {
                                h2[j][k] = atl::Variable<T>::gradient_structure_g.Value(active_parameters_m[j]->info->id, active_parameters_m[k]->info->id); //active_parameters_m[j]->info->hessian_row[active_parameters_m[k]->info->id];
                            }
                        }
                        atl::Variable<T>::gradient_structure_g.Reset();
#else
                        h2 = this->EstimatedHessian();
#endif
                        active_parameters_m[i]->SetValue(xsave);
                        
                        
                        T eps2 = eps*eps;
                        t2 = (h1 - h2) / (sd3 - sd4);
                        third_mixed[i] = (eps_squared * t1 - t2) / (eps_squared_minus_one);
                    }
                    
                    
                    
                    
                    return third_mixed;
                    
                }
                
            };
            
            template<class T>
            T AutoDiffTest<T>::tolerance = T(1e-5);
            
            template<class T>
            T AutoDiffTest<T>::second_tolerance = T(1e-3);
            
            template<class T>
            T AutoDiffTest<T>::third_tolerance = T(1e-2);
            
            template<class T>
            class AddAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                AddAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = a + b" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = a + b;
                }
                
                
            };
            
            template<class T>
            class Add1AutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                Add1AutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    //                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = a + 3.1459" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = a + static_cast<T>(3.1459);
                }
                
                
            };
            
            template<class T>
            class Add2AutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                Add2AutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    //                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    //                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = 3.1459 + b" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = static_cast<T>(3.1459) + b;
                }
                
                
            };
            
            template<class T>
            class SubtractAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                SubtractAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = a - b" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = a - b;
                }
                
                
            };
            
            template<class T>
            class Subtract1AutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                Subtract1AutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    //                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = a - 3.1459" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = a - static_cast<T>(3.1459);
                }
                
                
            };
            
            template<class T>
            class Subtract2AutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                Subtract2AutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    //                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = 3.1459 - b" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = static_cast<T>(3.1459) - b;
                }
                
                
            };
            
            template<class T>
            class MultiplyAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                MultiplyAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = a * b" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = a * b;
                }
                
                
            };
            
            template<class T>
            class Multiply1AutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                Multiply1AutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    //                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = a * 3.1459" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = a * static_cast<T>(3.1459);
                }
                
                
            };
            
            template<class T>
            class Multiply2AutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                Multiply2AutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    //                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = 3.1459 * b" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = static_cast<T>(3.1459) * b;
                }
                
                
            };
            
            template<class T>
            class DivideAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                DivideAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = a / b" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = a / b;
                }
                
                
            };
            
            template<class T>
            class Divide1AutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                Divide1AutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    //                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = a / 3.1459" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = a / static_cast<T>(3.1459);
                }
                
                
            };
            
            template<class T>
            class Divide2AutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                Divide2AutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    //                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = 3.1459 / b" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = static_cast<T>(3.1459) / b;
                }
                
                
            };
            
            template<class T>
            class ACosAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                ACosAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::acos((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::acos((a * b));
                }
                
                
            };
            
            template<class T>
            class ASinAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                ASinAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::asin((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::asin((a * b));
                }
                
                
            };
            
            template<class T>
            class ATanAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                ATanAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::atan((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::atan((a * b));
                }
                
                
            };
            
            template<class T>
            class CeilAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                CeilAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::ceil((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::ceil((a * b));
                }
                
                
            };
            
            template<class T>
            class CosAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                CosAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::cos((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::cos((a * b));
                }
                
                
            };
            
            template<class T>
            class CoshAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                CoshAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::cosh((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::cosh((a * b));
                }
                
                
            };
            
            template<class T>
            class ExpAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                ExpAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::exp((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::exp((a * b));
                }
                
                
            };
            
            template<class T>
            class FabsAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                FabsAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::fabs((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::fabs((a * b));
                }
                
                
            };
            
            template<class T>
            class FloorAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                FloorAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::floor((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::floor((a * b));
                }
                
                
            };
            
            template<class T>
            class LogAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                LogAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .3434;
                    b = .34230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::log((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::log((a + b));
                }
                
                
            };
            
            template<class T>
            class Log10AutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                Log10AutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .3434;
                    b = .34230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::log10((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::log10((a * b));
                }
                
                
            };
            
            template<class T>
            class PowAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                PowAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::pow((a * b),(a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::pow((a * b), (a * b));
                }
                
                
            };
            
            template<class T>
            class PowCAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                PowCAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::pow((a * b),2.0)" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::pow(a, static_cast<T>(2.0));
                }
                
                
            };
            
            template<class T>
            class PowC2AutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                PowC2AutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::pow(2.0,(a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::pow(static_cast<T>(2.0), (a * b));
                }
                
                
            };
            
            template<class T>
            class SinAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                SinAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::sin((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::sin((a * b));
                }
                
                
            };
            
            template<class T>
            class SinhAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                SinhAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::sinh((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::sinh((a * b));
                }
                
                
            };
            
            template<class T>
            class SqrtAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                SqrtAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::sqrt((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::sqrt((a * b));
                }
                
                
            };
            
            template<class T>
            class TanAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                TanAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::tan((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::tan((a * b));
                }
                
                
            };
            
            template<class T>
            class TanhAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                
                TanhAutoDiffTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = .03434;
                    b = .034230;
                    this->Register(a);
                    this->Register(b);
                    
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << "\n";
                    out << "Variable b = " << b << "\n";
                    out << "f = atl::tanh((a * b))" << std::endl;
                    
                }
                
                void ObjectiveFunction(var& f) {
                    f = atl::tanh((a * b));
                }
                
                
            };
            
            template<class T>
            class SumAlotOfParametersAutoDiffTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                int nobs;
                std::vector<var> v;
                
                SumAlotOfParametersAutoDiffTest(std::ofstream& out, int n) : nobs(n) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = 1.03434;
                    b = 2.034230;
                    this->Register(a);
                    this->Register(b);
                    //                int nobs = 50;
                    
                    T small = 1.00001212;
                    for (int i = 0; i < nobs; i++) {
                        small += .01;
                        v.push_back(var(small));
                        //
                    }
                    
                    for (int i = 0; i < nobs; i++) {
                        this->Register(v[i]);
                    }
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << ";\n";
                    out << "Variable b = " << b << ";\n";
                    out << "Variable v[" << nobs << "]\n\n";
                    out << "Variable ff = 0;\n";
                    out << "ff = (a / b); \n"; //// (a * b+a/b+a*a) - (a*(a * b+a/b+a*a));
                    out << "for (int i = 0; i < nobs; i++) {\n";
                    out << "    for (int j = 0; j < nobs; j++) {\n";
                    out << "         ff += atl::log(v[j] * v[i] * atl::tanh(a / b * v[j] * v[i]) * atl::tanh(a / b) * v[j] * v[i] * atl::tanh(a / b));\n";
                    out << "     }\n";
                    out << "}\n";
                    out << "f.assign(Variable::gradient_structure_g, ff);\n";
                    
                }
                
                void ObjectiveFunction(var& f) {
                    //                    var ff;
                    //                    std::cout << "B id:" << b.info->id << "\n";
                    //                    var aa = a+b;
                    f = atl::sin(a * b); //atl::tanh((a / b)+(v[0]*v[1])+(v[0]*v[0])+(v[1]*v[1]));// * (v[0] * v[0]); // (a * b+a/b+a*a) - (a*(a * b+a/b+a*a));
                    //                    f = a*b*v[0];
                    //                    std::cout<<"d(a,a,a) = "<<( a*b*v[0]).EvaluateDerivative(b.info->id,b.info->id,b.info->id)<<"\n\n";
                    for (int i = 0; i < nobs; i++) {
                        for (int j = 0; j < nobs; j++) {
                            //                            int i = (nobs-1)-j;
                            f += (v[j] * v[j]) * (atl::log(a / b * v[j] * v[i]) * atl::log(a / b) * v[j] * v[i] * atl::log(a / b));
                        }
                    }
                    //                    ff*=ff;
                    //                    f = (nobs / 2.0) * (f);
                    //                    f.Assign(var::gradient_structure_g, f);
                }
                
                
            };
            
            template<class T>
            class ThirdTest : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                var a;
                var b;
                var c;
                var d;
                var g;
                
                ThirdTest(std::ofstream& out) {
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    a = 1.0; //.03434;
                    b = 2.0; //.034230;
                    c = 3.0; //1.0213213;
                    d = 4.0; //3.0123123;
                    //                    g = 5.0; //3.0123123;
                    this->Register(a);
                    this->Register(b);
                    this->Register(c);
                    this->Register(d);
                    //                    this->Register(g);
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << ";\n";
                    out << "Variable b = " << b << ";\n";
                    //                    out << "Variable v[" << nobs << "]\n\n";
                    out << "Variable ff = 0;\n";
                    out << "ff = (a / b); \n"; //// (a * b+a/b+a*a) - (a*(a * b+a/b+a*a));
                    out << "for (int i = 0; i < nobs; i++) {\n";
                    out << "    for (int j = 0; j < nobs; j++) {\n";
                    out << "         ff += atl::log(v[j] * v[i] * atl::tanh(a / b * v[j] * v[i]) * atl::tanh(a / b) * v[j] * v[i] * atl::tanh(a / b));\n";
                    out << "     }\n";
                    out << "}\n";
                    out << "f.assign(Variable::gradient_structure_g, ff);\n";
                    std::cout << "a " << a.info->id << "\n";
                    std::cout << "b " << b.info->id << "\n";
                    std::cout << "c " << c.info->id << "\n";
                    std::cout << "d " << d.info->id << "\n";
                }
                
                void ObjectiveFunction(var& f) {
                    //                    var ff;
                    f = (a / b); //+c * d; //+c+d; //+c*d;
                    //                    f += c * d;// * atl::sin(g);
                    //                    f += c * d;// * atl::sin(g);
                    f *= c / d;
                    //                    f = atl::log((a / b)+(c*d));
                    //                    f = 2.0 * f*c*d;
                    //                    std::cout << "d3f = " << (2.0 * atl::pow(f,2.0)).EvaluateDerivative(f.info->id,f.info->id,f.info->id) << "\n";
                }
                
                
            };
            
            
            template<class T>
            class Ackley : public AutoDiffTest<T> {
            public:
                typedef atl::Variable<T> var;
                typedef atl::Variable<T> variable;
                std::vector<variable > x;
                
                variable a;
                variable b;
                variable c;
                variable d;
                
                atl::Variable<T> f;
                std::mt19937 generator; //// mt19937 is a standard mersenne_twister_engine
                std::normal_distribution<double> distribution;
                
                Ackley(std::ofstream& out, int dd) {
                    d =(T)dd;
                    this->RunTestToFile(out);
                }
                
                void Initialize() {
                    int dimensions = (int)d.GetValue();
                    a = 20;
                    b = .2;
                    c = 2.0 * M_PI;
//                    d = dimensions;
                    //random seed
                    generator.seed(40909);
                    //set some random starting values
                    for (int i = 0; i < dimensions; i++) {
                        double r;
                        r = distribution(generator);
                        this->x.push_back(variable(r));
                    }
                    for (int i = 0; i < x.size(); i++) {
                        this->Register(x[i]);
                    }
                    
                    
                }
                
                void Description(std::stringstream& out) {
                    out << "Test Problem:\n";
                    out << "Parameters:\n";
                    out << "Variable a = " << a << ";\n";
                    out << "Variable b = " << b << ";\n";
                    out << "Variable c = " <<c << ";\n";
                    out << "Variable x["<<(int)d.GetValue()<< "];\n";
                    out<<"f = 0;"<<"\n";
                    out<<"variable sum1;"<< ";\n";
                    out<<"variable sum2;"<< ";\n";
                    out<<"variable term1;"<< ";\n";
                    out<<"variable term2;"<< ";\n\n";
                    
                    out<<"for (int i = 0; i < x.size(); i++) {"<< "\n";
                    out<<"    sum1 += atl::pow(x[i], 2.0);"<< "\n";
                    out<<"    sum2 += atl::cos(x[i] * c);"<< "\n";
                    out<<"}\n\n";
                    
                    out<<"term1 = -a * atl::exp(-b * atl::sqrt(sum1 / d));"<< "\n";
                    out<<"term2 = -1.0 * atl::exp(sum2 / d);"<< "\n\n";
                    
                    out<<"f = term1 + term2 + a + exp(1);"<< "\n";
                }
                
                void ObjectiveFunction(var& f) {
                    f = 0;
                    variable sum1;
                    variable sum2;
                    variable term1;
                    variable term2;
                    
                    for (int i = 0; i < x.size(); i++) {
                        sum1 += atl::pow(x[i], 2.0);
                        sum2 += atl::cos(x[i] * c);
                    }
                    
                    term1 = -a * atl::exp(-b * atl::sqrt(sum1 / d));
                    term2 = -1.0 * atl::exp(sum2 / d);
                    
                    f = term1 + term2 + a + std::exp(1);
                }
                
                
            };
            
            
            
            template<typename T>
            struct TestTypeTrait{
                static std::string type;
            };
            template<typename T>
            std::string TestTypeTrait<T>::type = "unknown precision";
            
            template<>
            std::string TestTypeTrait<double>::type = "double";
            
            template<>
            std::string TestTypeTrait<long double>::type = "long double";
            
            void Run() {
                typedef double real_t;
                std::ofstream out;
                out.open("autodiff_tests.txt");
                out << "This test compares the computed gradient, second-order, "
                "and third-order mixed \npartial derivatives against "
                "those computed using the differences method.\n\n";
                
                out<<"Data type: "<<TestTypeTrait<real_t>::type<<"\n\n";
                out << "Test Gradient Tolerance: " << atl::tests::auto_diff::AutoDiffTest<real_t>::GetTolerance() << "\n\n";
                out << "Test Second-Order Tolerance: " << atl::tests::auto_diff::AutoDiffTest<real_t>::GetSecondOrderTolerance() << "\n\n";
                out << "Test Second-Order Tolerance: " << atl::tests::auto_diff::AutoDiffTest<real_t>::GetThirdOrderTolerance() << "\n\n";
                
                std::cout << "running...\n";
                //
//                atl::tests::auto_diff::AddAutoDiffTest<real_t> add(out);
//                atl::tests::auto_diff::Add1AutoDiffTest<real_t> add1(out);
//                atl::tests::auto_diff::Add2AutoDiffTest<real_t> add2(out);
//                atl::tests::auto_diff::SubtractAutoDiffTest<real_t> subtract1(out);
//                atl::tests::auto_diff::Subtract1AutoDiffTest<real_t> subtract(out);
//                atl::tests::auto_diff::Subtract2AutoDiffTest<real_t> subtract2(out);
//                atl::tests::auto_diff::MultiplyAutoDiffTest<real_t> multiply(out);
//                atl::tests::auto_diff::Multiply1AutoDiffTest<real_t> multiply1(out);
//                atl::tests::auto_diff::Multiply2AutoDiffTest<real_t> multiply2(out);
//                atl::tests::auto_diff::DivideAutoDiffTest<real_t> divide(out);
//                atl::tests::auto_diff::Divide1AutoDiffTest<real_t> divide1(out);
//                atl::tests::auto_diff::Divide2AutoDiffTest<real_t> divide2(out);
//                atl::tests::auto_diff::CosAutoDiffTest<real_t> cos(out);
//                atl::tests::auto_diff::ACosAutoDiffTest<real_t> acos(out);
//                atl::tests::auto_diff::SinAutoDiffTest<real_t> sin(out);
//                atl::tests::auto_diff::ASinAutoDiffTest<real_t> asin(out);
//                atl::tests::auto_diff::TanAutoDiffTest<real_t> tan(out);
//                atl::tests::auto_diff::ATanAutoDiffTest<real_t> atan(out);
//                atl::tests::auto_diff::CoshAutoDiffTest<real_t> cosh(out);
//                atl::tests::auto_diff::SinhAutoDiffTest<real_t> sinh(out);
//                atl::tests::auto_diff::TanhAutoDiffTest<real_t> tanh(out);
//                atl::tests::auto_diff::ExpAutoDiffTest<real_t> exp(out);
//                atl::tests::auto_diff::LogAutoDiffTest<real_t> log(out);
//                atl::tests::auto_diff::Log10AutoDiffTest<real_t> log10(out);
//                atl::tests::auto_diff::FabsAutoDiffTest<real_t> fabs(out);
//                atl::tests::auto_diff::SqrtAutoDiffTest<real_t> sqrt(out);
//                atl::tests::auto_diff::PowAutoDiffTest<real_t> pow(out);
//                atl::tests::auto_diff::PowCAutoDiffTest<real_t> pow2(out);
//                atl::tests::auto_diff::PowC2AutoDiffTest<real_t> pow3(out);
//                atl::tests::auto_diff::CeilAutoDiffTest<real_t> ceil(out);
//                atl::tests::auto_diff::FloorAutoDiffTest<real_t> floor(out);
//                atl::tests::auto_diff::SumAlotOfParametersAutoDiffTest<real_t> s1(out, 10);
//                atl::tests::auto_diff::SumAlotOfParametersAutoDiffTest<real_t> s2(out, 20);
                atl::tests::auto_diff::SumAlotOfParametersAutoDiffTest<real_t> s3(out, 50);
                atl::tests::auto_diff::Ackley<real_t>  ackley10(out,10);
                atl::tests::auto_diff::Ackley<real_t>  ackley20(out,20);
                atl::tests::auto_diff::Ackley<real_t>  ackley30(out,30);
                atl::tests::auto_diff::Ackley<real_t>  ackley40(out,40);
                atl::tests::auto_diff::Ackley<real_t>  ackley50(out,50);
                atl::tests::auto_diff::Ackley<real_t>  ackley100(out,100);
                atl::tests::auto_diff::Ackley<real_t>  ackley200(out,200);
                //                atl::tests::auto_diff::SumAlotOfParametersAutoDiffTest<real_t> s4(out, 200);
                //                atl::tests::auto_diff::SumAlotOfParametersAutoDiffTest<real_t> s5(out, 500);
                
                //                                atl::tests::auto_diff::ThirdTest<real_t> tt(out);
                std::cout << "Test complete.\n";
                if (atl::tests::auto_diff::fail == 0) {
                    std::cout << "All tests passed, review file \"autodiff_test.txt\" for details." << std::endl;
                    
                } else {
                    std::cout << atl::tests::auto_diff::fail << " of " << atl::tests::auto_diff::tests << " tests did not agree within a tolerance of " << atl::tests::auto_diff::AutoDiffTest<real_t>::GetTolerance() << ", review file \"autodiff_test.txt\" for details." << std::endl;
                }
                
            }
            
        }
    }
}




#endif	/* HESSIANTESTS_HPP */