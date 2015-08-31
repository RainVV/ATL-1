/*
    Copyright 2005-2014 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

#include "harness.h"
#define TBB_PREVIEW_GRAPH_NODES 1
#include "tbb/flow_graph.h"

//
// Tests
//

 #if defined(_MSC_VER) && _MSC_VER < 1600
    #pragma warning (disable : 4503) //disabling the "decorated name length exceeded" warning for VS2008 and earlier
#endif

const int Count = 150;
const int MaxPorts = 10;
const int MaxNSources = 5; // max # of source_nodes to register for each indexer_node input in parallel test
bool outputCheck[MaxPorts][Count];  // for checking output

void
check_outputCheck( int nUsed, int maxCnt) {
    for(int i=0; i < nUsed; ++i) {
        for( int j = 0; j < maxCnt; ++j) {
            ASSERT(outputCheck[i][j], NULL);
        }
    }
}

void
reset_outputCheck( int nUsed, int maxCnt) {
    for(int i=0; i < nUsed; ++i) {
        for( int j = 0; j < maxCnt; ++j) {
            outputCheck[i][j] = false;
        }
    }
}

class test_class {
    public:
        test_class() { my_val = 0; }
        test_class(int i) { my_val = i; }
        operator int() { return my_val; }
    private:
        int my_val;
};

template<typename T>
class name_of {
public:
    static const char* name() { return  "Unknown"; }
};
template<>
class name_of<int> {
public:
    static const char* name() { return  "int"; }
};
template<>
class name_of<float> {
public:
    static const char* name() { return  "float"; }
};
template<>
class name_of<double> {
public:
    static const char* name() { return  "double"; }
};
template<>
class name_of<long> {
public:
    static const char* name() { return  "long"; }
};
template<>
class name_of<short> {
public:
    static const char* name() { return  "short"; }
};
template<>
class name_of<test_class> {
public:
    static const char* name() { return  "test_class"; }
};

// TT must be arithmetic, and shouldn't wrap around for reasonable sizes of Count (which is now 150, and maxPorts is 10,
// so the max number generated right now is 1500 or so.)  Source will generate a series of TT with value
// (init_val + (i-1)*addend) * my_mult, where i is the i-th invocation of the body.  We are attaching addend
// source nodes to a indexer_port, and each will generate part of the numerical series the port is expecting
// to receive.  If there is only one source node, the series order will be maintained; if more than one,
// this is not guaranteed.
template<typename TT>
class source_body {
    const TT my_mult;
    int my_count;
    const int addend;
    source_body& operator=( const source_body& other);
public:
    source_body(TT multiplier, int init_val, int addto) : my_mult(multiplier), my_count(init_val), addend(addto) { }
    bool operator()( TT &v) {
        int lc = my_count;
        v = my_mult * (TT)my_count;
        my_count += addend;
        return lc < Count;
    }
};

// allocator for indexer_node.

template<typename IType>
class makeIndexer {
public:
    static IType *create() {
        IType *temp = new IType();
        return temp;
    }
    static void destroy(IType *p) { delete p; }
};

template<int ELEM, typename INT>
struct getval_helper {

    typedef typename INT::output_type OT;
    typedef typename tbb::flow::tuple_element<ELEM-1, typename INT::tuple_types>::type stored_type;

    static int get_integer_val(OT const &o) {
        stored_type res = tbb::flow::cast_to<stored_type>(o);
        return (int)res;
    }
};

// holder for source_node pointers for eventual deletion

static void* all_source_nodes[MaxPorts][MaxNSources];

template<int ELEM, typename INT>
class source_node_helper {
public:
    typedef INT indexer_node_type;
    typedef typename indexer_node_type::output_type TT;
    typedef typename tbb::flow::tuple_element<ELEM-1,typename INT::tuple_types>::type IT;
    typedef typename tbb::flow::source_node<IT> my_source_node_type;
    static void print_remark() {
        source_node_helper<ELEM-1,INT>::print_remark();
        REMARK(", %s", name_of<IT>::name());
    }
    static void add_source_nodes(indexer_node_type &my_indexer, tbb::flow::graph &g, int nInputs) {
        for(int i=0; i < nInputs; ++i) {
            my_source_node_type *new_node = new my_source_node_type(g, source_body<IT>((IT)(ELEM+1), i, nInputs));
            ASSERT(new_node->register_successor(tbb::flow::input_port<ELEM-1>(my_indexer)), NULL);
            all_source_nodes[ELEM-1][i] = (void *)new_node;
        }
        // add the next source_node
        source_node_helper<ELEM-1, INT>::add_source_nodes(my_indexer, g, nInputs);
    }
    static void check_value(TT &v) {
        if(v.tag() == ELEM-1) {
            int ival = getval_helper<ELEM,INT>::get_integer_val(v);
            ASSERT(!(ival%(ELEM+1)), NULL);
            ival /= (ELEM+1);
            ASSERT(!outputCheck[ELEM-1][ival], NULL);
            outputCheck[ELEM-1][ival] = true;
        }
        else {
            source_node_helper<ELEM-1,INT>::check_value(v);
        }
    }

    static void remove_source_nodes(indexer_node_type& my_indexer, int nInputs) {
        for(int i=0; i< nInputs; ++i) {
            my_source_node_type *dp = reinterpret_cast<my_source_node_type *>(all_source_nodes[ELEM-1][i]);
            dp->remove_successor(tbb::flow::input_port<ELEM-1>(my_indexer));
            delete dp;
        }
        source_node_helper<ELEM-1, INT>::remove_source_nodes(my_indexer, nInputs);
    }
};

template<typename INT>
class source_node_helper<1, INT> {
    typedef INT indexer_node_type;
    typedef typename indexer_node_type::output_type TT;
    typedef typename tbb::flow::tuple_element<0, typename INT::tuple_types>::type IT;
    typedef typename tbb::flow::source_node<IT> my_source_node_type;
public:
    static void print_remark() {
        REMARK("Parallel test of indexer_node< %s", name_of<IT>::name());
    }
    static void add_source_nodes(indexer_node_type &my_indexer, tbb::flow::graph &g, int nInputs) {
        for(int i=0; i < nInputs; ++i) {
            my_source_node_type *new_node = new my_source_node_type(g, source_body<IT>((IT)2, i, nInputs));
            ASSERT(new_node->register_successor(tbb::flow::input_port<0>(my_indexer)), NULL);
            all_source_nodes[0][i] = (void *)new_node;
        }
    }
    static void check_value(TT &v) {
        int ival = getval_helper<1,INT>::get_integer_val(v);
        ASSERT(!(ival%2), NULL);
        ival /= 2;
        ASSERT(!outputCheck[0][ival], NULL);
        outputCheck[0][ival] = true;
    }
    static void remove_source_nodes(indexer_node_type& my_indexer, int nInputs) {
        for(int i=0; i < nInputs; ++i) {
            my_source_node_type *dp = reinterpret_cast<my_source_node_type *>(all_source_nodes[0][i]);
            dp->remove_successor(tbb::flow::input_port<0>(my_indexer));
            delete dp;
        }
    }
};

template<typename IType>
class parallel_test {
public:
    typedef typename IType::output_type TType;
    typedef typename IType::tuple_types union_types;
    static const int SIZE = tbb::flow::tuple_size<union_types>::value;
    static void test() {
        TType v;
        source_node_helper<SIZE,IType>::print_remark();
        REMARK(" >\n");
        for(int i=0; i < MaxPorts; ++i) {
            for(int j=0; j < MaxNSources; ++j) {
                all_source_nodes[i][j] = NULL;
            }
        }
        for(int nInputs = 1; nInputs <= MaxNSources; ++nInputs) {
            tbb::flow::graph g;
            IType* my_indexer = new IType(g); //makeIndexer<IType>::create(); 
            tbb::flow::queue_node<TType> outq1(g);
            tbb::flow::queue_node<TType> outq2(g);

            ASSERT((*my_indexer).register_successor(outq1), NULL);  // register outputs first, so they both get all
            ASSERT((*my_indexer).register_successor(outq2), NULL);  // the results

            source_node_helper<SIZE, IType>::add_source_nodes((*my_indexer), g, nInputs);

            g.wait_for_all();

            reset_outputCheck(SIZE, Count);
            for(int i=0; i < Count*SIZE; ++i) {
                ASSERT(outq1.try_get(v), NULL);
                source_node_helper<SIZE, IType>::check_value(v);
            }

            check_outputCheck(SIZE, Count);
            reset_outputCheck(SIZE, Count);

            for(int i=0; i < Count*SIZE; i++) {
                ASSERT(outq2.try_get(v), NULL);;
                source_node_helper<SIZE, IType>::check_value(v);
            }
            check_outputCheck(SIZE, Count);

            ASSERT(!outq1.try_get(v), NULL);
            ASSERT(!outq2.try_get(v), NULL);

            source_node_helper<SIZE, IType>::remove_source_nodes((*my_indexer), nInputs);
            (*my_indexer).remove_successor(outq1);
            (*my_indexer).remove_successor(outq2);
            makeIndexer<IType>::destroy(my_indexer);
        }
    }
};

std::vector<int> last_index_seen;

template<int ELEM, typename IType>
class serial_queue_helper {
public:
    typedef typename IType::output_type OT;
    typedef typename IType::tuple_types TT;
    typedef typename tbb::flow::tuple_element<ELEM-1,TT>::type IT;
    static void print_remark() {
        serial_queue_helper<ELEM-1,IType>::print_remark();
        REMARK(", %s", name_of<IT>::name());
    }
    static void fill_one_queue(int maxVal, IType &my_indexer) {
        // fill queue to "left" of me
        serial_queue_helper<ELEM-1,IType>::fill_one_queue(maxVal,my_indexer);
        for(int i = 0; i < maxVal; ++i) {
            ASSERT(tbb::flow::input_port<ELEM-1>(my_indexer).try_put((IT)(i*(ELEM+1))), NULL);
        }
    }
    static void put_one_queue_val(int myVal, IType &my_indexer) {
        // put this val to my "left".
        serial_queue_helper<ELEM-1,IType>::put_one_queue_val(myVal, my_indexer);
        ASSERT(tbb::flow::input_port<ELEM-1>(my_indexer).try_put((IT)(myVal*(ELEM+1))), NULL);
    }
    static void check_queue_value(OT &v) {
        if(ELEM - 1 == v.tag()) {
            // this assumes each or node input is queueing.
            int rval = getval_helper<ELEM,IType>::get_integer_val(v);
            ASSERT( rval == (last_index_seen[ELEM-1]+1)*(ELEM+1), NULL);
            last_index_seen[ELEM-1] = rval / (ELEM+1);
        }
        else {
            serial_queue_helper<ELEM-1,IType>::check_queue_value(v);
        }
    }
};

template<typename IType>
class serial_queue_helper<1, IType> {
public:
    typedef typename IType::output_type OT;
    typedef typename IType::tuple_types TT;
    typedef typename tbb::flow::tuple_element<0,TT>::type IT;
    static void print_remark() {
        REMARK("Serial test of indexer_node< %s", name_of<IT>::name());
    }
    static void fill_one_queue(int maxVal, IType &my_indexer) {
        for(int i = 0; i < maxVal; ++i) {
            ASSERT(tbb::flow::input_port<0>(my_indexer).try_put((IT)(i*2)), NULL);
        }
    }
    static void put_one_queue_val(int myVal, IType &my_indexer) {
        ASSERT(tbb::flow::input_port<0>(my_indexer).try_put((IT)(myVal*2)), NULL);
    }
    static void check_queue_value(OT &v) {
        ASSERT(v.tag() == 0, NULL);  // won't get here unless true
        int rval = getval_helper<1,IType>::get_integer_val(v);
        ASSERT( rval == (last_index_seen[0]+1)*2, NULL);
        last_index_seen[0] = rval / 2;
    }
};

template<typename IType, typename TType, int SIZE>
void test_one_serial( IType &my_indexer, tbb::flow::graph &g) {
    last_index_seen.clear();
    for(int ii=0; ii < SIZE; ++ii) last_index_seen.push_back(-1);

    typedef TType q3_input_type;
    tbb::flow::queue_node< q3_input_type >  q3(g);
    q3_input_type v;

    ASSERT((my_indexer).register_successor( q3 ), NULL);

    // fill each queue with its value one-at-a-time
    for (int i = 0; i < Count; ++i ) {
        serial_queue_helper<SIZE,IType>::put_one_queue_val(i,my_indexer);
    }

    g.wait_for_all();
    for (int i = 0; i < Count * SIZE; ++i ) {
        g.wait_for_all();
        ASSERT(q3.try_get( v ), "Error in try_get()");
        {
            serial_queue_helper<SIZE,IType>::check_queue_value(v);
        }
    }
    ASSERT(!q3.try_get( v ), "extra values in output queue");
    for(int ii=0; ii < SIZE; ++ii) last_index_seen[ii] = -1;

    // fill each queue completely before filling the next.
    serial_queue_helper<SIZE, IType>::fill_one_queue(Count,my_indexer);

    g.wait_for_all();
    for (int i = 0; i < Count*SIZE; ++i ) {
        g.wait_for_all();
        ASSERT(q3.try_get( v ), "Error in try_get()");
        {
            serial_queue_helper<SIZE,IType>::check_queue_value(v);
        }
    }
    ASSERT(!q3.try_get( v ), "extra values in output queue");
}

//
// Single predecessor at each port, single accepting successor
//   * put to buffer before port0, then put to buffer before port1, ...
//   * fill buffer before port0 then fill buffer before port1, ...

template<typename IType>
class serial_test {
    typedef typename IType::output_type TType;  // this is the union
    typedef typename IType::tuple_types union_types;
    static const int SIZE = tbb::flow::tuple_size<union_types>::value;
public:
static void test() {
    tbb::flow::graph g;
    static const int ELEMS = 3;
    IType* my_indexer = new IType(g); //makeIndexer<IType>::create(g);

    serial_queue_helper<SIZE, IType>::print_remark(); REMARK(" >\n");

    test_one_serial<IType,TType,SIZE>(*my_indexer, g);

    std::vector<IType> indexer_vector(ELEMS,*my_indexer);

    makeIndexer<IType>::destroy(my_indexer);

    for(int e = 0; e < ELEMS; ++e) {
        test_one_serial<IType,TType,SIZE>(indexer_vector[e], g);
    }
}

}; // serial_test

template<
      template<typename> class TestType,  // serial_test or parallel_test
      typename T0, typename T1=void, typename T2=void, typename T3=void, typename T4=void, 
      typename T5=void, typename T6=void, typename T7=void, typename T8=void, typename T9=void> // type of the inputs to the indexer_node
class generate_test {
public:
    typedef tbb::flow::indexer_node<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>  indexer_node_type;
    static void do_test() {
        TestType<indexer_node_type>::test();
    }
};

//specializations for indexer node inputs
template<
      template<typename> class TestType,  
      typename T0, typename T1, typename T2, typename T3, typename T4, 
      typename T5, typename T6, typename T7, typename T8> 
class generate_test<TestType, T0, T1, T2, T3, T4, T5, T6, T7, T8> {
public:
    typedef tbb::flow::indexer_node<T0, T1, T2, T3, T4, T5, T6, T7, T8>  indexer_node_type;
    static void do_test() {
        TestType<indexer_node_type>::test();
    }
};

template<
      template<typename> class TestType, 
      typename T0, typename T1, typename T2, typename T3, typename T4, 
      typename T5, typename T6, typename T7> 
class generate_test<TestType, T0, T1, T2, T3, T4, T5, T6, T7> {
public:
    typedef tbb::flow::indexer_node<T0, T1, T2, T3, T4, T5, T6, T7>  indexer_node_type;
    static void do_test() {
        TestType<indexer_node_type>::test();
    }
};

template<
      template<typename> class TestType, 
      typename T0, typename T1, typename T2, typename T3, typename T4, 
      typename T5, typename T6> 
class generate_test<TestType, T0, T1, T2, T3, T4, T5, T6> {
public:
    typedef tbb::flow::indexer_node<T0, T1, T2, T3, T4, T5, T6>  indexer_node_type;
    static void do_test() {
        TestType<indexer_node_type>::test();
    }
};

template<
      template<typename> class TestType, 
      typename T0, typename T1, typename T2, typename T3, typename T4, 
      typename T5> 
class generate_test<TestType, T0, T1, T2, T3, T4, T5>  {
public:
    typedef tbb::flow::indexer_node<T0, T1, T2, T3, T4, T5>  indexer_node_type;
    static void do_test() {
        TestType<indexer_node_type>::test();
    }
};

template<
      template<typename> class TestType, 
      typename T0, typename T1, typename T2, typename T3, typename T4> 
class generate_test<TestType, T0, T1, T2, T3, T4>  {
public:
    typedef tbb::flow::indexer_node<T0, T1, T2, T3, T4>  indexer_node_type;
    static void do_test() {
        TestType<indexer_node_type>::test();
    }
};

template<
      template<typename> class TestType,  
      typename T0, typename T1, typename T2, typename T3>  
class generate_test<TestType, T0, T1, T2, T3> {
public:
    typedef tbb::flow::indexer_node<T0, T1, T2, T3>  indexer_node_type;
    static void do_test() {
        TestType<indexer_node_type>::test();
    }
};

template<
      template<typename> class TestType,  
      typename T0, typename T1, typename T2> 
class generate_test<TestType, T0, T1, T2> {
public:
    typedef tbb::flow::indexer_node<T0, T1, T2>  indexer_node_type;
    static void do_test() {
        TestType<indexer_node_type>::test();
    }
};

template<
      template<typename> class TestType,  
      typename T0, typename T1>           
class generate_test<TestType, T0, T1> {
public:
    typedef tbb::flow::indexer_node<T0, T1>  indexer_node_type;
    static void do_test() {
        TestType<indexer_node_type>::test();
    }
};

template<
      template<typename> class TestType,  
      typename T0>           
class generate_test<TestType, T0> {
public:
    typedef tbb::flow::indexer_node<T0>  indexer_node_type;
    static void do_test() {
        TestType<indexer_node_type>::test();
    }
};

int TestMain() {
    REMARK("Testing indexer_node, ");
#if __TBB_USE_TBB_TUPLE
    REMARK("using TBB tuple\n");
#else
    REMARK("using platform tuple\n");
#endif

   for (int p = 0; p < 2; ++p) {
       generate_test<serial_test, float>::do_test();
#if MAX_TUPLE_TEST_SIZE >= 4
       generate_test<serial_test, float, double, int>::do_test();
#endif
#if MAX_TUPLE_TEST_SIZE >= 6
       generate_test<serial_test, double, double, int, long, int, short>::do_test();
#endif
#if MAX_TUPLE_TEST_SIZE >= 8
       generate_test<serial_test, float, double, double, double, float, int, float, long>::do_test();
#endif
#if MAX_TUPLE_TEST_SIZE >= 10
       generate_test<serial_test, float, double, int, double, double, float, long, int, float, long>::do_test();
#endif
       generate_test<parallel_test, float, double>::do_test();
#if MAX_TUPLE_TEST_SIZE >= 3
       generate_test<parallel_test, float, int, long>::do_test();
#endif
#if MAX_TUPLE_TEST_SIZE >= 5
       generate_test<parallel_test, double, double, int, int, short>::do_test();
#endif
#if MAX_TUPLE_TEST_SIZE >= 7
       generate_test<parallel_test, float, int, double, float, long, float, long>::do_test();
#endif
#if MAX_TUPLE_TEST_SIZE >= 9
       generate_test<parallel_test, float, double, int, double, double, long, int, float, long>::do_test();
#endif
   }   
   return Harness::Done;
}
