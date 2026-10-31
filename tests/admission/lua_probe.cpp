#include "admission.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace admission {
namespace {

constexpr std::size_t instance_limit = 8U * 1024U * 1024U;
constexpr std::size_t aggregate_limit = 64U * 1024U * 1024U;

struct Totals {
    std::size_t current{0};
    std::size_t peak{0};
    bool accounting_error{false};
};

struct Allocator {
    Totals& totals;
    std::size_t current{0};
    std::size_t limit{instance_limit};
    bool deny{false};
};

void* allocate(void* user, void* pointer, std::size_t old_size, std::size_t new_size) {
    auto& data = *static_cast<Allocator*>(user);
    // Lua uses osize as a type tag when pointer is null, not an allocation size.
    const std::size_t old = pointer != nullptr ? old_size : 0;
    if (old > data.current || old > data.totals.current) {
        data.totals.accounting_error = true;
        return nullptr;
    }
    if (new_size == 0) {
        std::free(pointer);
        data.current -= old;
        data.totals.current -= old;
        return nullptr;
    }
    if (data.deny || new_size > data.limit || data.current - old > data.limit - new_size) {
        return nullptr;
    }
    void* resized = std::realloc(pointer, new_size);
    if (resized != nullptr) {
        data.current = data.current - old + new_size;
        data.totals.current = data.totals.current - old + new_size;
        data.totals.peak = std::max(data.totals.peak, data.totals.current);
    }
    return resized;
}

lua_State* checked_state(lua_State* state) {
    require(state != nullptr, "lua_newstate allocation failed");
    return state;
}

struct Value {
    int number{0};
};

class Vm {
public:
    explicit Vm(Totals& totals)
        : allocator{totals}, state(lua_newstate(allocate, &allocator), lua_close),
          lua(checked_state(state.get())) {
        sol::set_default_state(state.get());
        lua.open_libraries(sol::lib::base, sol::lib::table, sol::lib::string, sol::lib::math);
        for (const auto* name : {"dump", "find", "match", "gmatch", "gsub", "rep"}) {
            lua["string"][name] = sol::nil;
        }
        lua["math"]["random"] = sol::nil;
        lua["math"]["randomseed"] = sol::nil;
        // sol2 3.3.0's direct member-pointer trampoline fails on Clang18.
        // Typed accessors keep field reads/writes and argument checks intact.
        lua.new_usertype<Value>(
            "ProbeValue", sol::constructors<Value()>(), "number",
            sol::property([](const Value& value) { return value.number; },
                          [](Value& value, int number) { value.number = number; }));
    }

    [[nodiscard]] sol::environment environment() {
        sol::environment env(lua, sol::create);
        for (const auto* name :
             {"assert", "error", "ipairs", "pairs", "next", "type", "tostring", "tonumber"}) {
            env[name] = lua[name];
        }
        for (const auto* name : {"table", "string", "math"}) {
            sol::table copy = lua.create_table();
            sol::table original = lua[name];
            for (const auto& item : original) {
                copy[item.first] = item.second;
            }
            env[name] = copy;
        }
        sol::table constructors = lua.create_table();
        constructors["new"] = lua["ProbeValue"]["new"];
        env["ProbeValue"] = constructors;
        env["_G"] = env;
        return env;
    }

    Allocator allocator;
    std::unique_ptr<lua_State, decltype(&lua_close)> state;
    sol::state_view lua;
};

struct Instance {
    sol::environment environment;
    sol::table object;
};

constexpr std::string_view script = R"(
ticks = 0
local value = ProbeValue.new()
value.number = 17
return { value=value, label='admission', update=function()
    ticks = ticks + 1
    value.number = value.number + 1
    return ticks
end }
)";

Instance instantiate(Vm& vm) {
    auto env = vm.environment();
    auto result = vm.lua.safe_script(script, env, sol::script_pass_on_error);
    require(result.valid(), "sandbox script failed to initialize");
    sol::table object = result.get<sol::table>();
    return {std::move(env), std::move(object)};
}

struct Guard {
    int& count;
    ~Guard() {
        ++count;
    }
};

struct InstructionBudget {
    int remaining{5000};
};

void instruction_hook(lua_State* state, lua_Debug*) {
    lua_getfield(state, LUA_REGISTRYINDEX, "m4.budget");
    auto* budget = static_cast<InstructionBudget*>(lua_touserdata(state, -1));
    lua_pop(state, 1);
    budget->remaining -= 100;
    if (budget->remaining <= 0) {
        luaL_error(state, "admission instruction quota exceeded");
    }
}

void check_script(Vm& vm, const sol::environment& environment, const char* text) {
    auto result = vm.lua.safe_script(text, environment, sol::script_pass_on_error);
    require(result.valid(), "admission script assertion failed");
}

} // namespace

nlohmann::json benchmark_lua(std::string_view mode, std::size_t count) {
    require(mode == "per-instance" || mode == "per-session", "unknown VM model");
    Totals totals;
    std::vector<std::unique_ptr<Vm>> states;
    std::vector<Instance> instances;
    states.reserve(count);
    instances.reserve(count);
    const auto start = std::chrono::steady_clock::now();
    if (mode == "per-session") {
        states.push_back(std::make_unique<Vm>(totals));
        states.back()->allocator.limit = aggregate_limit;
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (mode == "per-instance") {
            states.push_back(std::make_unique<Vm>(totals));
        }
        instances.push_back(instantiate(*states.back()));
    }
    const auto end = std::chrono::steady_clock::now();
    const auto retained = totals.current;
    const auto peak = totals.peak;
    // Equal workload and a positive isolation check, outside startup timing.
    for (auto& instance : instances) {
        sol::protected_function update = instance.object.get<sol::protected_function>("update");
        auto result = update();
        require(result.valid() && result.get<int>() == 1, "instance state is not isolated");
        require(instance.object["value"].get<Value&>().number == 18,
                "bound value changed incorrectly");
    }
    const auto milliseconds = std::chrono::duration<double, std::milli>(end - start).count();
    instances.clear();
    states.clear();
    require(totals.current == 0 && !totals.accounting_error, "Lua allocation accounting leaked");
    return {{"model", mode},
            {"instances", count},
            {"startupMilliseconds", milliseconds},
            {"retainedLuaBytes", retained},
            {"peakLuaBytes", peak},
            {"aggregateRuntimeBudgetBytes", aggregate_limit},
            {"exceedsAggregateRuntimeBudget", retained > aggregate_limit},
            {"diagnosticAggregateBudgetNotEnforced", true},
            {"sourceRelease", "sol2 3.3.0 / Lua 5.4.9 C++"},
            {"solHeaderVersion", SOL_VERSION_STRING},
            {"luaRelease", LUA_RELEASE}};
}

nlohmann::json verify_lua() {
    Totals totals;
    {
        Vm vm(totals);
        auto first = instantiate(vm);
        auto second = instantiate(vm);
        check_script(vm, first.environment,
                     "assert(io==nil and os==nil and debug==nil and package==nil and require==nil);"
                     "assert(load==nil and loadfile==nil and dofile==nil and getmetatable==nil);"
                     "assert(string.dump==nil and string.find==nil and math.random==nil);"
                     "marker=42; math.private_marker=42; ProbeValue.private_marker=42; "
                     "assert(_G.marker==42)");
        check_script(
            vm, second.environment,
            "assert(marker==nil and math.private_marker==nil and ProbeValue.private_marker==nil)");
        auto syntax =
            vm.lua.safe_script("local = broken", first.environment, sol::script_pass_on_error);
        require(!syntax.valid(), "syntax error was accepted");
        int lua_destructors = 0;
        vm.lua.set_function("lua_error_probe", [&lua_destructors](sol::this_state state) {
            Guard guard{lua_destructors};
            luaL_error(state, "admission Lua error");
        });
        first.environment["lua_error_probe"] = vm.lua["lua_error_probe"];
        auto failure =
            vm.lua.safe_script("lua_error_probe()", first.environment, sol::script_pass_on_error);
        require(!failure.valid() && lua_destructors == 1, "Lua error bypassed C++ destructor");
        vm.lua.set_function("typed", [](int value) { return value + 1; });
        first.environment["typed"] = vm.lua["typed"];
        auto wrong_type =
            vm.lua.safe_script("typed({})", first.environment, sol::script_pass_on_error);
        require(!wrong_type.valid(), "unsafe argument coercion");
        int cpp_destructors = 0;
        vm.lua.set_function("cpp_error_probe", [&cpp_destructors]() {
            Guard guard{cpp_destructors};
            throw std::runtime_error("admission host error");
        });
        first.environment["cpp_error_probe"] = vm.lua["cpp_error_probe"];
        auto cpp_failure =
            vm.lua.safe_script("cpp_error_probe()", first.environment, sol::script_pass_on_error);
        require(!cpp_failure.valid() && cpp_destructors == 1,
                "host exception escaped protected call");

        InstructionBudget budget;
        lua_pushlightuserdata(vm.state.get(), &budget);
        lua_setfield(vm.state.get(), LUA_REGISTRYINDEX, "m4.budget");
        lua_sethook(vm.state.get(), instruction_hook, LUA_MASKCOUNT, 100);
        auto loop =
            vm.lua.safe_script("while true do end", first.environment, sol::script_pass_on_error);
        lua_sethook(vm.state.get(), nullptr, 0, 0);
        require(!loop.valid() && budget.remaining <= 0,
                "instruction quota did not stop the script");
        lua_pushnil(vm.state.get());
        lua_setfield(vm.state.get(), LUA_REGISTRYINDEX, "m4.budget");

        auto loaded = vm.lua.load("return function() return {} end");
        require(loaded.valid(), "OOM probe failed to load");
        sol::protected_function factory = loaded.get<sol::protected_function>();
        auto factory_result = factory();
        require(factory_result.valid(), "OOM factory failed");
        sol::protected_function allocate_table = factory_result.get<sol::protected_function>();
        vm.allocator.deny = true;
        auto oom = allocate_table();
        vm.allocator.deny = false;
        require(!oom.valid(), "allocator failure was not protected");
        check_script(vm, second.environment, "assert(1+1==2)");
    }
    require(totals.current == 0 && !totals.accounting_error,
            "Lua VM leaked after protected errors");
    const auto first = benchmark_lua("per-instance", 2);
    const auto shared = benchmark_lua("per-session", 2);
    require(first["instances"] == shared["instances"], "topology workloads differ");
    return {{"luaRelease", LUA_RELEASE},
            {"solSourceTag", "v3.3.0"},
            {"solHeaderVersion", SOL_VERSION_STRING},
            {"cxxLua", true},
            {"checks",
             {"bound-field", "sandbox-globals", "environment-and-library-isolation", "syntax-error",
              "Lua-error-unwinding", "typed-arguments", "host-exception", "instruction-exhaustion",
              "allocator-failure", "VM-cleanup", "both-topologies"}}};
}

} // namespace admission
