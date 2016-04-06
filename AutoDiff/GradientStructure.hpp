/*
 * File:   GradientStructure.hpp
 * Author: matthewsupernaw
 *
 * Created on April 7, 2015, 4:05 PM
 */

#ifndef GRADIENTSTRUCTURE_HPP
#define GRADIENTSTRUCTURE_HPP


#include "Config.hpp"
#include <memory>
#include <vector>
#include <algorithm>
#include <functional>
#include <atomic>
#include <iostream>
#include "VariableInfo.hpp"
#include <fstream>
#include <cmath>
#include "../Utilities/Combinations.hpp"
#include "../Utilities/flat_map.hpp"
#include "DynamicExpression.hpp"

#ifdef ATL_USE_SMID
#include "../Utilities/SIMD.hpp"
#endif

#define ATL_ENABLE_BOUNDS_CHECKING


#include "Variable.hpp"


#define Entry StackEntry<REAL_T>



namespace atl {

    template<typename REAL_T>
    struct StackEntry {
        VariableInfo<REAL_T>* w; //function or dependent variable.
        atl::DynamicExpression<REAL_T>* exp;
        IDSet<atl::VariableInfo<REAL_T>* > ids;
        typedef typename IDSet<atl::VariableInfo<REAL_T>* >::iterator id_itereator;
        std::vector<VariableInfo<REAL_T>* > live_ids; //live variables used in reverse accumulation
        std::vector<atl::VariableInfo<REAL_T>* >id_list;
        std::vector<REAL_T> first;
        std::vector<REAL_T> second;
        std::vector<REAL_T> second_mixed;
        std::vector<REAL_T> third;
        std::vector<REAL_T> third_mixed;
        uint32_t max_id = std::numeric_limits<uint32_t>::min();
        uint32_t min_id = std::numeric_limits<uint32_t>::max();

        StackEntry() : w(NULL), exp(NULL) {

        }

        StackEntry(const StackEntry<REAL_T>& orig) {
            this->w = orig.w;
            if (orig.exp)
                this->exp = orig.exp->Clone();
            typename IDSet<atl::VariableInfo<REAL_T>* >::const_iterator it;
            for (it = orig.ids.begin(); it != orig.ids.end(); ++it) {
                this->ids.insert((*it));
            }

            this->first.insert(this->first.begin(), orig.first.begin(), orig.first.end());
            this->second.insert(this->second.begin(), orig.second.begin(), orig.second.end());
        }

        const std::pair<uint32_t, uint32_t> FindMinMax() {
            std::pair<uint32_t, uint32_t> p;
            p.first = this->w->id;
            p.second = this->w->id;
            typename IDSet<atl::VariableInfo<REAL_T>* >::iterator it;
            for (it = this->ids.begin(); it != ids.end(); ++it) {
                if ((*it)->id < p.first) {
                    p.first = (*it)->id;
                }
                if ((*it)->id > p.second) {
                    p.second = (*it)->id;
                }
            }
            return p;
        }

        inline void PushVariable(VariableInfo<REAL_T>* v) {
            if (v != w) {

                id_itereator it = ids.find(v);

                if (it == ids.end()) {
                    live_ids.push_back(v);
                }
            }
        }

        inline void PushVariables(const std::vector<VariableInfo<REAL_T>* >& v) {
            for (size_t i = 0; i < v.size(); i++) {
                if (v[i] != w && v[i]->is_dependent == 0) {
                    id_itereator it = ids.find(v[i]);

                    if (it == ids.end()) {
                        live_ids.push_back(v[i]);
                    }
                }
            }
        }

        inline void Prepare() {
            id_list.resize(0);
            typename IDSet<atl::VariableInfo<REAL_T>* >::iterator it;
            typename IDSet<atl::VariableInfo<REAL_T>* >::iterator e;
            e = ids.end();
            for (it = ids.begin(); it != e; ++it) {
                id_list.push_back((*it));
            }

            typename std::vector<atl::VariableInfo<REAL_T>* >::iterator ee;
            ee = live_ids.end();
            typename std::vector<atl::VariableInfo<REAL_T>* >::iterator jt;
            for (jt = live_ids.begin(); jt != ee; ++jt) {
                id_list.push_back((*jt));
            }


        }

        inline void SoftReset() {
            first.resize(0);
            second_mixed.resize(0);
            third_mixed.resize(0);
            typename IDSet<atl::VariableInfo<REAL_T>* >::iterator it;
            typename IDSet<atl::VariableInfo<REAL_T>* >::iterator end = ids.end();
            for (it = ids.begin(); it != end; ++it) {
                (*it)->Reset();
            }

            w->Reset();
            live_ids.resize(0);
        }

        inline void Reset() {

            max_id = std::numeric_limits<uint32_t>::min();
            min_id = std::numeric_limits<uint32_t>::max();
            first.resize(0);
            second_mixed.resize(0);
            third_mixed.resize(0);
            typename IDSet<atl::VariableInfo<REAL_T>* >::iterator it;
            typename IDSet<atl::VariableInfo<REAL_T>* >::iterator end = ids.end();
            for (it = ids.begin(); it != end; ++it) {
                (*it)->Reset();
            }

            w->Reset();
            w = NULL;
            live_ids.resize(0);

            if (exp) {
                delete exp;
                exp = NULL;
            }
            ids.clear();

        }


    };

    enum DerivativeTraceLevel {
        FIRST_ORDER = 0, //SAME AS GRADIENT
        SECOND_ORDER, // SECOND ORDER PER VARIABLE ONLY
        THIRD_ORDER, // THIRD ORDER PER VARIABLE ONLY
        SECOND_ORDER_MIXED_PARTIALS, //SAME AS HESSIAN_AND_GRADIENT
        THIRD_ORDER_MIXED_PARTIALS,
        GRADIENT,
        GRADIENT_AND_HESSIAN,
        DYNAMIC_RECORD,
    };

    template<class T>
    class DerivativeMatrix {
        size_t rows;
        size_t columns;
        std::vector<flat_map<size_t, size_t> > row_indices;

        std::vector<T> data;
    public:

        DerivativeMatrix(size_t rows = 1, size_t columns = 1) :
        rows(rows), columns(columns) {
            row_indices.resize(rows);
            data.reserve(rows);
        }

        inline T& operator()(const size_t& r, const size_t& c) {

            flat_map<size_t, size_t>::iterator it = row_indices[r].find(c);

            if (it != row_indices[r].end()) {
                return data[(*it).second];
            } else {
                data.push_back(T());
                row_indices[r][c] = data.size() - 1;
                return data[data.size() - 1];
            }

        }

        inline const T value(const size_t& r, const size_t& c) {
            flat_map<size_t, size_t>::iterator it = row_indices[r].find(c);
            return it != row_indices[r].end() ? data[(*it).second] : T();
        }

        void zero() {
            for (size_t i = 0; i < data.size(); i++) {
                data[i] = T(0);
            }
        }

        void resize(size_t r, size_t c) {
            row_indices.resize(r);
            rows = r;
            columns = c;
        }
    };

    /**
     * Class to record operations. Often refered to as a "Tape". Holds a stack of
     * first and second order partial derivatives used in adjoint accumulation of
     * gradients and Hessian matrices.
     */
    template<typename REAL_T>
    class GradientStructure {
        DerivativeMatrix<REAL_T> somp; //second-order mixed
        std::vector<DerivativeMatrix<REAL_T> > tomp; //third-order mixed
        std::vector<std::vector<bool> > tompv; // third-order look up


    public:
        uint32_t max_id = std::numeric_limits<uint32_t>::min();
        uint32_t min_id = std::numeric_limits<uint32_t>::max();
        size_t range;
        DerivativeTraceLevel derivative_trace_level;
        std::vector<StackEntry<REAL_T> > gradient_stack;
#ifdef ATL_THREAD_SAFE
        std::mutex stack_lock;
#endif
        size_t stack_current;
        size_t stack_begin;

        bool recording;
        size_t max_stack_size;
        size_t max_initialized_size;

        bool gradient_computed;
        std::mutex stack_lock;

        GradientStructure(uint32_t size = 10000)
        : recording(true), stack_current(0), stack_begin(0),
        gradient_computed(false),
        derivative_trace_level(GRADIENT_AND_HESSIAN) {
            gradient_stack.resize(size);
            max_stack_size = size;
            max_initialized_size = 0;
        }

        GradientStructure(const GradientStructure<REAL_T>& other) :
        derivative_trace_level(other.derivative_trace_level),
        stack_current(other.stack_current),
        recording(other.recording),
        max_stack_size(other.max_stack_size),
        max_initialized_size(other.max_initialized_size),
        gradient_computed(other.gradient_computed) {

            for (int i = 0; i < other.stack_current; i++) {
                this->gradient_stack.push_back(other.gradient_stack[i]);
            }
        }

        void Begin() {
            this->stack_begin = stack_current + 1;
        }

        /**
         * Sets the size of the stack.
         * @param size
         */
        void SetSize(size_t size) {
            gradient_stack.resize(size);
        }

        virtual ~GradientStructure() {

        }

        inline void SetRecording(bool recording) {
            this->recording = recording;
        }

        inline REAL_T& Reference(uint32_t i) {
            //            return first_order[i];
        }

        inline REAL_T& Reference(uint32_t i, uint32_t j) {

            if (j < i) {
                std::swap(i, j);
            }
            size_t ind = static_cast<size_t> (i - this->min_id);
            size_t jnd = static_cast<size_t> (j - this->min_id);
            return this->somp(ind, jnd);

        }

        inline REAL_T& Reference(uint32_t i, uint32_t j, uint32_t k) {
            if (i < j) {
                if (k < i) std::swap(i, k);
            } else {
                if (j < k) std::swap(i, j);
                else std::swap(i, k);
            }
            if (k < j) std::swap(j, k);
            size_t ind = static_cast<size_t> (i - this->min_id);
            return this->tomp[ind]((j - this->min_id), (k - this->min_id)); //[j][k];

        }

        inline const REAL_T Value(uint32_t i) {
            //            return this->first_order.get(i);
        }

        inline const REAL_T Value(uint32_t i, uint32_t j) {
            if (j < i) {
                std::swap(i, j);
            }

            return somp.value((i - this->min_id), (j - this->min_id));

        }

        inline const REAL_T Value(uint32_t i, uint32_t j, uint32_t k) {

            if (i < j) {
                if (k < i) std::swap(i, k);
            } else {
                if (j < k) std::swap(i, j);
                else std::swap(i, k);
            }

            if (k < j) std::swap(j, k);

            return this->tomp[(i - this->min_id)].value((j - this->min_id), (k - this->min_id));

        }

        /**
         * Atomic operation. Gets the next available index in the stack.
         *
         * @return
         */
        inline const size_t NextIndex() {
#ifdef ATL_THREAD_SAFE
            stack_lock.lock();
#endif
            if (stack_current + 1 >= this->gradient_stack.size()) {
//                std::cout<<"Resizing Tape structure...\n";
                this->gradient_stack.resize(this->gradient_stack.size() + 100);
            }

#ifdef ATL_THREAD_SAFE
            stack_lock.unlock();
#endif       
            return this->stack_current++;
        }

        inline StackEntry<REAL_T>& NextEntry() {
            return this->gradient_stack[this->NextIndex()];
        }

        /**
         * Accumulates derivatives in reverse mode according to the member <i>derivative_trace</i>.
         *<br><br> <b>Reverse mode accumulation equations for each <i>derivative_trace</i> flag:</b><br>
         * <br><br>For <b><i>GRADIENT</i></b> or <b><i>FIRST_ORDER</i></b> 
         * \image html gradient.png
         * For <b><i>HESSIAN</i></b> or <b><i>SECOND_ORDER_MIXED_PARTIALS</i></b> 
         * \image html hessian.png
         * For <b><i>THIRD_ORDER_MIXED_PARTIALS</i></b>
         * \image html third_order.png
         * 
         */
        inline void Accumulate() {
            gradient_computed = true;

            if (recording) {
                REAL_T w = 0.0;
                typename IDSet<atl::VariableInfo<REAL_T>* >::iterator it;
                typename IDSet<atl::VariableInfo<REAL_T>* >::iterator end;

                int j = 0;
                switch (this->derivative_trace_level) {

                    case GRADIENT:
                        this->AccumulateFirstOrder();
                        break;
                    case DYNAMIC_RECORD:
                        break;
                    case FIRST_ORDER:
                        this->AccumulateFirstOrder();
                        break;

                    case SECOND_ORDER:
                        std::cout << "SECOND_ORDER not yet implemented!";
                        exit(0);
                        break;
                    case THIRD_ORDER:
                        std::cout << "THIRD_ORDER not yet implemented!";
                        exit(0);
                        break;
                    case GRADIENT_AND_HESSIAN:
                        this->AccumulateSecondOrderMixed();
                        break;
                    case SECOND_ORDER_MIXED_PARTIALS:
                        this->AccumulateSecondOrderMixed();
                        break;
                    case THIRD_ORDER_MIXED_PARTIALS:
                        this->AccumulateThirdOrderMixed();
                        break;
                    default:
                        std::cout << __func__ << "unknown trace level...\n";
                }
            }
        }

        void AccumulateFirstOrder() {

            this->PrepareDerivativeTables(1);
#ifdef ATL_USE_SMID
            REAL_T w;
            REAL_T adj[2];
            typedef typename simd::simd_traits<REAL_T>::type sse_t;
            sse_t sse_w;
            sse_t sse_d;
            sse_t sse_result;
            size_t sse_size = simd::simd_traits<REAL_T>::size;

#else
            REAL_T w = 0.0;
            int j = 0;
            typename IDSet<atl::VariableInfo<REAL_T>* >::iterator it;
#endif
            gradient_stack[stack_current - 1].w->dvalue = 1.0;
#pragma unroll
            for (int i = (stack_current - 1); i >= 0; i--) {
#ifdef ATL_USE_SMID

                w = gradient_stack[i].w->dvalue; //gradient_stack[i].w->dvalue; //set w
                //                w[1] = w[0];
                sse_w.load1(&w);



                if (w != static_cast<REAL_T> (0)) {
                    gradient_stack[i].w->dvalue = 0;

                    int j = 0;
                    size_t size = gradient_stack[i].ids.data().size();

                    for (; (j + sse_size) < size; j += sse_size) {
                        sse_d.load_u(&gradient_stack[i].first[j]);
                        sse_result = sse_d*sse_w;
                        sse_result.store_u(adj);

                        if (adj[0] != static_cast<REAL_T> (0)) {
                            gradient_stack[i].ids.data()[j]->dvalue += adj[0];
                        }

                        if (adj[1] != static_cast<REAL_T> (0)) {
                            gradient_stack[i].ids.data()[j + 1]->dvalue += adj[1];
                        }
                    }

                    for (; j < size; j++) {
                        gradient_stack[i].ids.data()[j]->dvalue += w * gradient_stack[i].first[j];
                    }

                }
#else
                w = gradient_stack[i].w->dvalue;
                //                std::cout<<"I = "<<i<<"\n"<<gradient_stack[i].w->dependence_level<<"\n";
                if (w != static_cast<REAL_T> (0.0)) {
                    gradient_stack[i].w->dvalue = 0.0;
                    j = 0;
                    for (it = gradient_stack[i].ids.begin(); it != gradient_stack[i].ids.end(); ++it) {
                        //                        std::cout<< (*it)->dvalue<<"+="<<w<<"*"<<gradient_stack[i].first[j]<<"\n";
                        (*it)->dvalue += w * gradient_stack[i].first[j];

                        j++;
                    }
                }
#endif
            }
        }

        void AccumulateFirstOrderDynamic() {


            REAL_T w = 0.0;
            int j = 0;
            typename IDSet<atl::VariableInfo<REAL_T>* >::iterator it;

            gradient_stack[stack_current - 1].w->dvalue = 1.0;
#pragma unroll
            for (int i = (stack_current - 1); i >= 0; i--) {
                gradient_stack[i].w->vvalue = gradient_stack[i].exp->Evaluate();
                w = gradient_stack[i].w->dvalue;

                if (w != static_cast<REAL_T> (0.0)) {
                    gradient_stack[i].w->dvalue = 0.0;
                    j = 0;
                    for (it = gradient_stack[i].ids.begin(); it != gradient_stack[i].ids.end(); ++it) {
                        (*it)->dvalue += w * gradient_stack[i].exp->EvaluateDerivative((*it)->id);
                        j++;
                    }
                }

            }
        }

        /**
         * Computes the gradient and Hessian matrix via reverse mode accumulation.
         * \f[
            \begin{equation}
        \begin{split}
        \frac{\hat{\partial}}{\hat{\partial} v_b}\left[\frac{\hat{\partial} f_i}{\hat{\partial} v_c}\right] &=
        \frac{\partial^2 f_{i+1}}{\partial v_b \partial v_c} +
        \left(\frac{\partial^2 \phi_i}{\partial v_b \partial v_c} * \frac{\partial f_{i+1}}{\partial v_i} \right) +
        \left(\frac{\partial\phi_i}{\partial v_c} * \frac{\partial^2 f_{i+1}}{\partial v_b \partial v_i}\right)
        \\
        &+ \left(\frac{\partial\phi_i}{\partial v_b} * \frac{\partial^2 f_{i+1}}{\partial v_i \partial v_c}\right) + 
        \left(\frac{\partial\phi_i}{\partial v_b} * \frac{\partial\phi_i}{\partial v_c} * \frac{\partial^2 f_{i+1}}{\partial v_i \partial v_i}\right)
        \end{split}
        \end{equation}
	
        \begin{equation}
        \begin{split}
        \frac{\hat{\partial}}{\hat{\partial}  v_a}\left[\frac{\hat{\partial}}{\hat{\partial} v_b}\left(\frac{\hat{\partial} f_i}{\hat{\partial} v_c}\right)\right] &= 
        \frac{\partial^3 f_{i+1}}{\partial v_a \partial v_b \partial v_c} + 
        \left(\frac{\partial^3 \phi_i}{\partial v_a \partial v_b \partial v_c} * \frac{\partial f_{i+1}}{\partial v_i}\right) + 
        \left(\frac{\partial^2 \phi_i}{\partial v_b \partial v_c} * \frac{\partial^2 f_{i+1}}{\partial v_a \partial v_i}\right) 
        \\
        &+ \left(\frac{\partial^2 \phi_i}{\partial v_a \partial v_c} * \frac{\partial^2 f_{i+1}}{\partial v_b \partial v_i}\right) + 
        \left(\frac{\partial \phi_i}{\partial v_c} * \frac{\partial^3 f_{i+1}}{\partial v_a \partial v_b \partial v_i}\right) 
        \\
        &+ \left(\frac{\partial^2 \phi_i}{\partial v_a \partial v_b} * \frac{\partial^2 f_{i+1}}{\partial v_i \partial v_c}\right) + 
        \left(\frac{\partial \phi_i}{\partial v_b} * \frac{\partial^3 f_{i+1}}{\partial v_a \partial v_i \partial v_c}\right) 
        \\
        &+ \left(\frac{\partial^2 \phi_i}{\partial v_a \partial v_b} * \frac{\partial \phi_i}{\partial v_c} * \frac{\partial^2 f_{i+1}}{\partial v_i \partial v_i}\right) + 
        \left(\frac{\partial \phi_i}{\partial v_b} * \frac{\partial^2 \phi_i}{\partial v_a \partial v_c} * \frac{\partial^2 f_{i+1}}{\partial v_i \partial v_i}\right) + 
        \\
        &+ \left(\frac{\partial \phi_i}{\partial v_b} * \frac{\partial \phi_i}{\partial v_c} * \frac{\partial^3 f_{i+1}}{\partial v_a \partial v_i \partial v_i}\right) 
        \\
        &+ \frac{\partial \phi_i}{\partial v_a} * 
        \left[\frac{\partial^3 f_{i+1}}{\partial v_i \partial v_b \partial v_c} + 
        \left(\frac{\partial^3 \phi_i}{\partial v_i \partial v_b \partial v_c} * \frac{\partial f_{i+1}}{\partial v_i}\right) + 
        \left(\frac{\partial^2 \phi_i}{\partial v_b \partial v_c} * \frac{\partial^2 f_{i+1}}{\partial v_i \partial v_i}\right) 
        \right.
        \\
        &\left.
        + \left(\frac{\partial^2 \phi_i}{\partial v_i \partial v_c} * \frac{\partial^2 f_{i+1}}{\partial v_b \partial v_i}\right) + 
        \left(\frac{\partial \phi_i}{\partial v_c} * \frac{\partial^3 f_{i+1}}{\partial v_i \partial v_b \partial v_i}\right) 
        \right.
        \\
        &\left.
        + \left(\frac{\partial^2 \phi_i}{\partial v_i \partial v_b} * \frac{\partial^2 f_{i+1}}{\partial v_i \partial v_c}\right) + 
        \left(\frac{\partial \phi_i}{\partial v_b} * \frac{\partial^3 f_{i+1}}{\partial v_i \partial v_i \partial v_c}\right) 
        \right.
        \\
        &\left.
        + \left(\frac{\partial^2 \phi_i}{\partial v_i \partial v_b} * \frac{\partial \phi_i}{\partial v_c} * \frac{\partial^2 f_{i+1}}{\partial v_i \partial v_i}\right) + 
        \left(\frac{\partial \phi_i}{\partial v_b} * \frac{\partial^2 \phi_i}{\partial v_i \partial v_c} * \frac{\partial^2 f_{i+1}}{\partial v_i \partial v_i}\right) 
        \right.
        \\
        &\left.
        + \left(\frac{\partial \phi_i}{\partial v_b} * \frac{\partial \phi_i}{\partial v_c} * \frac{\partial^3 f_{i+1}}{\partial v_i \partial v_i \partial v_i}\right)\right]
        \end{split}
         * ]
         * 
         * \image html hessian.png
         */

        void AccumulateSecondOrderMixed() {

            if (recording) {
                this->PrepareDerivativeTables(2);
                //                this->second_order_mixed = SecondOrderMixed(this->min_id, this->max_id);
                std::vector<std::vector<int> > combinations;

                REAL_T w;
                REAL_T w2;


                //initialize w
                this->gradient_stack[stack_current - 1].w->dvalue = 1.0;


                unsigned rows = 0; //the size of the local derivatives, anything higher was pushed from previous calculation

                std::vector<REAL_T> vij; //holds current second order derivative for i wrt j



                REAL_T hii = 0.0;
                REAL_T hij = 0.0;
                REAL_T hjk = 0;
                REAL_T dj = 0;
                REAL_T dk = 0;



                util::CombinationsWithRepetition combos(10, 2);

                for (int i = (stack_current - 1); i >= 0; i--) {
#ifdef HESSIAN_TRACE
                    std::cout << "\n\n\n";
                    std::stringstream ss;
                    int pushed_count = 0;
#endif
                    atl::VariableInfo<REAL_T>* vi = gradient_stack[i].w; //variable info for i
                    w = gradient_stack[i].w->dvalue; //gradient_stack[i].w->dvalue; //set w
                    gradient_stack[i].w->dvalue = 0; //cancel out derivative for i

                    rows = gradient_stack[i].first.size();

                    //get h[i][i]
                    hii = this->Value(vi->id, vi->id);
                    if (hii != 0) {
                        this->Reference(vi->id, vi->id) = 0.0;
                    }


                    //prepare for the hessian calculation. 
                    //builds a list of variables to use, statement level variables come first,
                    //then any pushed variables are after.
                    gradient_stack[i].Prepare();

                    size_t ID_LIST_SIZE = gradient_stack[i].id_list.size();

                    //resize second order derivative for i wrt j
                    vij.resize(ID_LIST_SIZE);
                    std::vector<bool> needs_push(ID_LIST_SIZE, false);
                    for (int j = 0; j < rows; j++) {
                        needs_push[j] = true;
                    }

#pragma unroll
                    for (unsigned j = 0; j < ID_LIST_SIZE; j++) {
                        //                        std::cout << "push " << j << std::endl;
                        atl::VariableInfo<REAL_T>* vj = gradient_stack[i].id_list[j];

                        //compute gradient
                        if (j < rows && w != REAL_T(0.0)) {
                            vj->dvalue += w * gradient_stack[i].first[j];
                            //                            vj->dvalue += w * gradient_stack[i].first[j];
                            //                            if (dv != 0.0) {
                            //                                this->Reference(vj->id) += dv;
                            //                            }
                        }

                        //load second order partial derivative for i wrt j and k
                        hij = this->Value(vi->id, vj->id);

                        if (hij != 0) {
                            this->Reference(vi->id, vj->id) = 0.0;
                        }


                        vij[j] = (hij);

                    }
#pragma unroll
                    for (int j = 0; j < rows; j++) {
                        atl::VariableInfo<REAL_T>* vj = gradient_stack[i].id_list[j];
                        dj = gradient_stack[i].first[j];
                        REAL_T hij = vij[j]; //h[i][j]
#pragma unroll
                        for (int k = j; k < rows; k++) {

                            atl::VariableInfo<REAL_T>* vk = gradient_stack[i].id_list[k];

                            REAL_T entry = 0.0; //the entry value for h[j][k]


                            dk = gradient_stack[i].first[k];






                            entry += vij[k] * dj + (hij * dk) + hii * dj*dk;


                            entry += w * gradient_stack[i].second_mixed[j * rows + k];
                            if (gradient_stack[i].second_mixed[j * rows + k] != 0.0) {
                                vj->push_count = 1;
                                vk->push_count = 1;
                            }



                            if (/*std::fabs(entry)*/entry != REAL_T(0.0)) {//h[j][k] needs to be updated
                                this->Reference(vj->id, vk->id) += entry;
                                needs_push[k] = true;
                                needs_push[j] = true;
                            }

                        }

                        for (int k = rows; k < ID_LIST_SIZE; k++) {

                            atl::VariableInfo<REAL_T>* vk = gradient_stack[i].id_list[k];

                            REAL_T entry = 0.0; //the entry value for h[j][k]

                            dk = 0;





                            entry += vij[k] * dj; // + (hij * dk) + hii * dj*dk;


                            if (j < rows && k < rows) {

                                entry += w * gradient_stack[i].second_mixed[j * rows + k];
                                if (gradient_stack[i].second_mixed[j * rows + k] != 0.0) {
                                    vj->push_count = 1;
                                    vk->push_count = 1;
                                }
                            }


                            if (/*std::fabs(entry)*/entry != REAL_T(0.0) && entry == entry) {//h[j][k] needs to be updated
                                this->Reference(vj->id, vk->id) += entry;
                                needs_push[k] = true;
                                needs_push[j] = true;
                            }

                        }
                    }
                    if (gradient_stack[i].w->dependence_level > 0) {//this was a compound assignment and its dependencies must be pushed
                        if (i > 0) {

                            for (int ii = 0; ii < rows; ii++) {
                                gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                            }

#pragma unroll

                            for (int ii = rows; ii < ID_LIST_SIZE; ii++) {
                                if ((gradient_stack[i].id_list[ii]->has_nl_interaction
                                        && gradient_stack[i].id_list[ii]->push_start >= i)) {
                                    gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                                }

                                if ((!gradient_stack[i].id_list[ii]->is_dependent && needs_push[ii])) {
                                    gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                                }
                                if ((gradient_stack[i].id_list[ii]->is_nl && gradient_stack[i].id_list[ii]->push_count >= 1)) {
                                    gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                                }
                            }
                        }
                        gradient_stack[i].w->dependence_level--;
                    }
                }
            }
        }

        void AccumulateSecondOrderMixedDynamic() {

            if (recording) {
                this->PrepareDerivativeTables(2);
                //                this->second_order_mixed = SecondOrderMixed(this->min_id, this->max_id);
                std::vector<std::vector<int> > combinations;

                REAL_T w;
                REAL_T w2;


                //initialize w
                this->gradient_stack[stack_current - 1].w->dvalue = 1.0;


                unsigned rows = 0; //the size of the local derivatives, anything higher was pushed from previous calculation

                std::vector<REAL_T> vij; //holds current second order derivative for i wrt j



                REAL_T hii = 0.0;
                REAL_T hij = 0.0;
                REAL_T hjk = 0;
                REAL_T dj = 0;
                REAL_T dk = 0;



                util::CombinationsWithRepetition combos(10, 2);

                for (int i = (stack_current - 1); i >= 0; i--) {
#ifdef HESSIAN_TRACE
                    std::cout << "\n\n\n";
                    std::stringstream ss;
                    int pushed_count = 0;
#endif
                    atl::VariableInfo<REAL_T>* vi = gradient_stack[i].w; //variable info for i
                    w = gradient_stack[i].w->dvalue; //gradient_stack[i].w->dvalue; //set w
                    gradient_stack[i].w->dvalue = 0; //cancel out derivative for i

                    rows = gradient_stack[i].ids.size();

                    //get h[i][i]
                    hii = this->Value(vi->id, vi->id);
                    if (hii != 0) {
                        this->Reference(vi->id, vi->id) = 0.0;
                    }


                    //prepare for the hessian calculation. 
                    //builds a list of variables to use, statement level variables come first,
                    //then any pushed variables are after.
                    gradient_stack[i].Prepare();

                    size_t ID_LIST_SIZE = gradient_stack[i].id_list.size();

                    //resize second order derivative for i wrt j
                    vij.resize(ID_LIST_SIZE);
                    std::vector<bool> needs_push(ID_LIST_SIZE, false);
                    for (int j = 0; j < rows; j++) {
                        needs_push[j] = true;
                    }

#pragma unroll
                    for (unsigned j = 0; j < ID_LIST_SIZE; j++) {
                        //                        std::cout << "push " << j << std::endl;
                        atl::VariableInfo<REAL_T>* vj = gradient_stack[i].id_list[j];

                        //compute gradient
                        if (j < rows && w != REAL_T(0.0)) {
                            vj->dvalue += w * gradient_stack[i].exp->EvaluateDerivative(vj->id);

                        }

                        //load second order partial derivative for i wrt j and k
                        hij = this->Value(vi->id, vj->id);

                        if (hij != 0) {
                            this->Reference(vi->id, vj->id) = 0.0;
                        }


                        vij[j] = (hij);

                    }
#pragma unroll
                    for (int j = 0; j < rows; j++) {
                        atl::VariableInfo<REAL_T>* vj = gradient_stack[i].id_list[j];
                        dj = gradient_stack[i].first[j];
                        REAL_T hij = vij[j]; //h[i][j]
#pragma unroll
                        for (int k = j; k < rows; k++) {

                            atl::VariableInfo<REAL_T>* vk = gradient_stack[i].id_list[k];

                            REAL_T entry = 0.0; //the entry value for h[j][k]


                            dk = gradient_stack[i].exp->EvaluateDerivative(vk->id);






                            entry += vij[k] * dj + (hij * dk) + hii * dj*dk;


                            entry += w * gradient_stack[i].second_mixed[j * rows + k];
                            if (gradient_stack[i].second_mixed[j * rows + k] != 0.0) {
                                vj->push_count = 1;
                                vk->push_count = 1;
                            }



                            if (/*std::fabs(entry)*/entry != REAL_T(0.0)) {//h[j][k] needs to be updated
                                this->Reference(vj->id, vk->id) += entry;
                                needs_push[k] = true;
                                needs_push[j] = true;
                            }

                        }

                        for (int k = rows; k < ID_LIST_SIZE; k++) {

                            atl::VariableInfo<REAL_T>* vk = gradient_stack[i].id_list[k];

                            REAL_T entry = 0.0; //the entry value for h[j][k]

                            dk = 0;





                            entry += vij[k] * dj; // + (hij * dk) + hii * dj*dk;


                            if (j < rows && k < rows) {
                                REAL_T d2 = gradient_stack[i].exp->EvaluateDerivative(vj->id, vk->id);
                                entry += w * gradient_stack[i].second_mixed[j * rows + k];
                                if (d2 != 0.0) {
                                    vj->push_count = 1;
                                    vk->push_count = 1;
                                }
                            }


                            if (/*std::fabs(entry)*/entry != REAL_T(0.0) && entry == entry) {//h[j][k] needs to be updated
                                this->Reference(vj->id, vk->id) += entry;
                                needs_push[k] = true;
                                needs_push[j] = true;
                            }

                        }
                    }
                    if (gradient_stack[i].w->dependence_level > 0) {//this was a compound assignment and its dependencies must be pushed
                        if (i > 0) {

                            for (int ii = 0; ii < rows; ii++) {
                                gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                            }

#pragma unroll

                            for (int ii = rows; ii < ID_LIST_SIZE; ii++) {
                                if ((gradient_stack[i].id_list[ii]->has_nl_interaction
                                        && gradient_stack[i].id_list[ii]->push_start >= i)) {
                                    gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                                }

                                if ((!gradient_stack[i].id_list[ii]->is_dependent && needs_push[ii])) {
                                    gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                                }
                                if ((gradient_stack[i].id_list[ii]->is_nl && gradient_stack[i].id_list[ii]->push_count >= 1)) {
                                    gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                                }
                            }
                        }
                        gradient_stack[i].w->dependence_level--;
                    }
                }
            }
        }

        void AccumulateThirdOrderMixed() {


            if (recording) {

                this->PrepareDerivativeTables(3);


                REAL_T w;

                //initialize w
                gradient_stack[stack_current - 1].w->dvalue = 1.0;
                unsigned rows = 0; //the size of the local derivatives, anything higher was pushed from previous calculation

                std::vector<REAL_T> vij; //holds current second order derivative for i wrt j
                std::vector<REAL_T> viij_;
                std::vector<REAL_T> vijk_;


                REAL_T hii = 0.0;
                REAL_T diii = 0.0;
                REAL_T dijk = 0.0;
                REAL_T diil = 0.0;
                REAL_T dj = 0.0;
                REAL_T dk = 0.0;
                REAL_T dl = 0.0;
                REAL_T pjl = 0.0;
                REAL_T pkl = 0.0;
                REAL_T entry_3 = 0;
                REAL_T d3 = 0.0;
                REAL_T hij = 0.0;
                REAL_T pjk = 0.0;




                for (int i = (stack_current - 1); i >= 0; i--) {

                    atl::VariableInfo<REAL_T>* vi = gradient_stack[i].w; //variable info for i
                    w = gradient_stack[i].w->dvalue; //set w
                    gradient_stack[i].w->dvalue = 0; //cancel out derivative for i

                    rows = gradient_stack[i].first.size();

                    //get h[i][i]
                    hii = Value(vi->id, vi->id);

                    if (hii != 0.0) {
                        Reference(vi->id, vi->id) = 0.0;
                    }
                    diii = 0.0;

                    diii = Value(vi->id, vi->id, vi->id);

                    if (diii != 0.0) {
                        Reference(vi->id, vi->id, vi->id) = 0.0;
                    }


                    //prepare for the hessian calculation. 
                    //builds a list of variables to use, statement level variables come first,
                    //then any pushed "independent" variables are after.
                    gradient_stack[i].Prepare();

                    size_t ID_LIST_SIZE = gradient_stack[i].id_list.size();

                    //resize second order derivative for i wrt j
                    vij.resize(ID_LIST_SIZE);
                    viij_.resize(ID_LIST_SIZE);
                    vijk_.resize(ID_LIST_SIZE * ID_LIST_SIZE);

                    std::vector<bool> needs_push(ID_LIST_SIZE, false);
                    for (int j = 0; j < rows; j++) {
                        needs_push[j] = true;
                    }



                    //compute gradient
                    if (w != REAL_T(0.0)) {
                        for (unsigned j = 0; j < rows; j++) {
                            gradient_stack[i].id_list[j]->dvalue += w * gradient_stack[i].first[j];
                        }
                    }
                    atl::VariableInfo<REAL_T>* vk;
                    atl::VariableInfo<REAL_T>* vj;
                    atl::VariableInfo<REAL_T>* vl;
                    //prepare higher order stuff
#pragma unroll
                    for (unsigned j = 0; j < ID_LIST_SIZE; j++) {

                        vj = gradient_stack[i].id_list[j];

                        //load second order partial derivative for i wrt j and k
                        vij[j] = Value(vi->id, vj->id);

                        if (std::fabs(vij[j]) > 0.0) {
                            Reference(vi->id, vj->id) = 0.0;
                        }

                        viij_[j] = Value(vi->id, vi->id, vj->id);

                        if (std::fabs(viij_[j]) > 0.0) {
                            Reference(vi->id, vi->id, vj->id) = 0.0;
                        }


#pragma unroll
                        for (unsigned k = j; k < ID_LIST_SIZE; k++) {
                            vk = gradient_stack[i].id_list[k];

                            vijk_[(j * ID_LIST_SIZE) + k] = Value(vi->id, vj->id, vk->id);

                            if (std::fabs(vijk_[(j * ID_LIST_SIZE) + k]) > 0.0) {
                                Reference(vi->id, vj->id, vk->id) = 0.0;

                            }
                            vijk_[(k * ID_LIST_SIZE) + j] = vijk_[(j * ID_LIST_SIZE) + k]; // dijk;
                        }
                    }


                    REAL_T entry;
                    REAL_T hdk;
                    REAL_T hdj;
#pragma unroll
                    for (int j = 0; j < rows; j++) {
                        vj = gradient_stack[i].id_list[j];
                        dj = gradient_stack[i].first[j];

                        if (j == 0) {
#pragma unroll
                            for (int k = j; k < rows; k++) {
                                hdj = 0;
                                hdj = gradient_stack[i].first[k];
                                 atl::VariableInfo<REAL_T>* vk = gradient_stack[i].id_list[k];
#pragma unroll
                               
                                for (int l = k; l < rows; l++) {

                                    vl = gradient_stack[i].id_list[l];
                                    hdk = 0;

                                    entry = 0.0; //the entry value for h[j][k]

                                    hdk = gradient_stack[i].first[l];


                                    entry += vij[l] * hdj + (vij[k] * hdk) + hii * hdj*hdk;


                                    entry += w * gradient_stack[i].second_mixed[k * rows + l];
                                    if (gradient_stack[i].second_mixed[k * rows + l] != 0.0) {
                                        vk->push_count++;
                                    }


                                    if (/*std::fabs(entry)*/entry != REAL_T(0.0)) {//h[j][k] needs to be updated
                                        this->Reference(vk->id, vl->id) += entry;
                                        needs_push[k] = true;
                                        needs_push[l] = true;
                                    }
                                }

                                for (int l = rows; l < ID_LIST_SIZE; l++) {

                                    vl = gradient_stack[i].id_list[l];
                                    hdk = 0;

                                    entry = 0.0; //the entry value for h[j][k]

                                    entry += vij[l] * hdj; // + (vij[k] * hdk) + hii * hdj*hdk;


                                    if (entry != REAL_T(0.0)) {//h[j][k] needs to be updated
                                        Reference(vk->id, vl->id) += entry;
                                        vl->push_count++;
                                        needs_push[k] = true;
                                        needs_push[l] = true;

                                    }
                                }
                            }
                        }

#pragma unroll
                        for (int k = j; k < rows; k++) {
                            vk = gradient_stack[i].id_list[k];;
                            dk = gradient_stack[i].first[k];
                            pjk = gradient_stack[i].second_mixed[j * rows + k];

                            for (int l = k; l < rows; l++) {
                                vl = gradient_stack[i].id_list[l];
                                entry_3 = 0;

                                dl = gradient_stack[i].first[l];
                                pjl = gradient_stack[i].second_mixed[j * rows + l];
                                pkl = gradient_stack[i].second_mixed[k * rows + l];

                                d3 = gradient_stack[i].third_mixed[(j * rows * rows) + (k * rows) + l];

                                entry_3 += (d3 * w)
                                        +(pjl * vij[k])
                                        + (dk * pjl * hii)
                                        + (dl * vijk_[(j * ID_LIST_SIZE + k)])
                                        + (pkl * vij[j])
                                        + (dk * dl * viij_[j])
                                        + (pjk * dl * hii);
                                if (d3 != 0.0) {
                                    vl->push_count++;
                                }


                                entry_3 += (pjk * vij[l])
                                        + (dk * vijk_[(j * ID_LIST_SIZE + l)]);





                                entry_3 += dj * (vijk_[(k * ID_LIST_SIZE + l)] + (pkl * hii)+(dl * viij_[k]) + (dk * viij_[l])
                                        +(dk * dl * diii));

                                if (entry_3 != 0.0) {
                                    Reference(vj->id, vk->id, vl->id) += entry_3;
                                }

                            }

                            for (int l = rows; l < ID_LIST_SIZE; l++) {

                                vl = gradient_stack[i].id_list[l];
                                entry_3 = 0;

                                dl = 0.0;
                                pjl = 0.0;
                                pkl = 0.0;


                                entry_3 += (pjk * vij[l])
                                        + (dk * vijk_[(j * ID_LIST_SIZE + l)]);





                                entry_3 += dj * (vijk_[(k * ID_LIST_SIZE + l)] + (dk * viij_[l]));

                                if (entry_3 != 0.0) {
                                    Reference(vj->id, vk->id, vl->id) += entry_3;
                                }

                            }
                        }

                        if (dj != 0.0) {
#pragma unroll
                            for (int k = rows; k < ID_LIST_SIZE; k++) {
                                atl::VariableInfo<REAL_T>* vk = gradient_stack[i].id_list[k];
#pragma unroll
                                for (int l = k; l < ID_LIST_SIZE; l++) {
                                    atl::VariableInfo<REAL_T>* vl = gradient_stack[i].id_list[l];
                                    entry_3 = dj * (vijk_[(k * ID_LIST_SIZE + l)]);

                                    if (entry_3 != 0.0) {
                                        Reference(vj->id, vk->id, vl->id) += entry_3;
                                        vl->push_count++;
                                    }

                                }
                            }
                        }
                    }
                    




                    if (gradient_stack[i].w->dependence_level > 0) {//this was a compound assignment and its dependencies must be pushed
                        if (i > 0) {

                            for (int ii = 0; ii < rows; ii++) {
                                gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                            }

#pragma unroll

                            for (int ii = rows; ii < ID_LIST_SIZE; ii++) {
                                if ((gradient_stack[i].id_list[ii]->has_nl_interaction
                                        && gradient_stack[i].id_list[ii]->push_start >= i)) {
                                    gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                                }

                                if ((!gradient_stack[i].id_list[ii]->is_dependent && needs_push[ii])) {
                                    gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                                }
                                if ((gradient_stack[i].id_list[ii]->is_nl && gradient_stack[i].id_list[ii]->push_count >= 1)) {
                                    gradient_stack[i - 1].PushVariable(gradient_stack[i].id_list[ii]);
                                }
                            }
                        }
                        gradient_stack[i].w->dependence_level--;
                    }
                }

            }

        }

        void AccumulateThirdOrderMixedDynamic() {

            std::cout << __func__ << "Not yet implemented!\n";
            exit(0);
          
        }

        /**
         * Resets this stack and makes it available for a new recording.
         *
         * @param empty_trash
         */
        inline void Reset(bool empty_trash = true) {
            max_id = std::numeric_limits<uint32_t>::min();
            min_id = std::numeric_limits<uint32_t>::max();
        
            if (max_initialized_size < stack_current) {
                max_initialized_size = stack_current;
            }

#pragma unroll
            for (int i = (stack_current - 1); i >= 0; i--) {
                this->gradient_stack[i].Reset();
            }
            if (empty_trash) {
                VariableInfo<REAL_T>::FreeAll();
            }
            stack_current = 0;

            gradient_computed = false;
        }

        void PrepareDerivativeTables(int level = 2) {
            max_id = atl::VariableIdGenerator::instance()->current(); //std::numeric_limits<uint32_t>::min();
            min_id = 1; //std::numeric_limits<uint32_t>::max();
            range = max_id - min_id;
            
            somp.zero();
            this->somp.resize(range + 1, range + 1);
            this->tomp.resize(range + 1);
            for (int i = 0; i < tomp.size(); i++) {
                tomp[i].zero();
                tomp[i].resize(range + 1, range + 1);
            }

        }

        inline void SoftReset() {
          
            somp.zero();
            this->somp.resize(range + 1, range + 1);
            this->tomp.resize(range + 1);
            for (int i = 0; i < tomp.size(); i++) {
                tomp[i].zero();
                tomp[i].resize(range + 1, range + 1);
            }


            for (int i = (stack_current - 1); i >= 0; i--) {
                this->gradient_stack[i].SoftReset();
            }
        }



    };



}

#endif /* GRADIENTSTRUCTURE_HPP */
