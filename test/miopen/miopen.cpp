
#include <rtg/program.hpp>
#include <rtg/operators.hpp>
#include <rtg/generate.hpp>
#include <rtg/cpu/cpu_target.hpp>
#include <rtg/miopen/miopen_target.hpp>
#include <rtg/miopen/miopen.hpp>
#include <rtg/miopen/hip.hpp>
#include <rtg/manage_ptr.hpp>

#include <miopen/miopen.h>

#include "test.hpp"
#include "verify.hpp"

template<class V>
rtg::argument run_cpu()
{
    V v;
    auto p = v.create_program();
    p.compile(rtg::cpu::cpu_target{});
    return p.eval(v.create_params());
}

template<class V>
rtg::argument run_gpu()
{
    V v;
    auto p = v.create_program();
    p.compile(rtg::miopen::miopen_target{});

    auto m = v.create_params();
    for(auto&& e:m)
    {
        e.second = rtg::miopen::to_gpu(e.second);
    }

    m["output"]      = rtg::miopen::to_gpu(rtg::generate_argument(p.get_parameter_shape("output")));
    auto handle = rtg::miopen::make_obj<rtg::miopen::miopen_handle>(&miopenCreate);
    m["handle"] = {rtg::shape::any_type, handle.get()};

    return rtg::miopen::from_gpu(p.eval(m));
}

template<class V>
void verify_program()
{
    auto cpu_arg = run_cpu<V>();
    auto gpu_arg = run_gpu<V>();
    visit_all(cpu_arg, gpu_arg)([](auto cpu, auto gpu) {
        EXPECT(test::verify_range(cpu, gpu));
    });
}

struct test1 
{
    rtg::program create_program() const
    {
        rtg::program p;
        auto input   = p.add_parameter("x", rtg::shape{rtg::shape::float_type, {4, 3, 3, 3}});
        auto weights = p.add_parameter("w", rtg::shape{rtg::shape::float_type, {4, 3, 3, 3}});
        auto conv    = p.add_instruction(rtg::convolution{}, input, weights);
        p.add_instruction(rtg::activation{"relu"}, conv);
        return p;
    }

    rtg::program::parameter_map create_params() const
    {
        rtg::program::parameter_map m;
        m["x"] = rtg::generate_argument({rtg::shape::float_type, {4, 3, 3, 3}});
        m["w"] = rtg::generate_argument({rtg::shape::float_type, {4, 3, 3, 3}});
        return m;
    }
};

int main() { verify_program<test1>(); }
