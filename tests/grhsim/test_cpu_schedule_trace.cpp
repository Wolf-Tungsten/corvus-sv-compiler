#include "grhsim/backend/cpu.hpp"
#include "grhsim/dialect/registry.hpp"
#include "grhsim/pass/pass.hpp"

#include <array>
#include <random>
#include <stdexcept>

namespace
{
    using namespace wolvrix::lib;
    using namespace grhsim;

    void check(bool condition, const char *message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    GrhSimModel traceModel()
    {
        GrhSimModel model("register_chain_derived_clock");
        model.addDialect("core", "1", "wolvrix.grhsim.core.v1");
        const auto bit = model.logicType(1, false, LogicDomain::TwoState);
        std::vector<ValueId> inputs;
        for (auto name : {"clock", "data", "enable"})
        {
            const auto input = model.addInput(name, bit);
            const auto value = model.addValue(bit); const std::array results{value};
            const std::array refs{ObjectRef::input(input)};
            model.addOperation("core.input.read", {}, results, refs); inputs.push_back(value);
        }
        const auto one = model.addValue(bit); const std::array oneResults{one};
        const std::array oneParams{Parameter{model.intern("value"), std::string("1'b1")}};
        model.addOperation("core.compute.constant", {}, oneResults, {}, oneParams);
        const auto state = [&]() {
            const auto id = model.addState("s" + std::to_string(model.states().size()), bit);
            const std::array params{Parameter{model.intern("value"), std::string("1'b0")}};
            const std::array steps{InitStep{model.intern("core.init.const"), {0, 1}}};
            model.addInit(id, steps, params); return id;
        };
        const std::array registers{state(), state(), state()};
        std::vector<ValueId> reads;
        for (auto reg : registers)
        {
            const auto value = model.addValue(bit); const std::array results{value};
            const std::array refs{ObjectRef::state(reg)};
            model.addOperation("core.state.read", {}, results, refs); reads.push_back(value);
        }
        const auto gated = model.addValue(bit); const std::array gateResults{gated};
        const std::array gateInputs{inputs[0], inputs[2]};
        model.addOperation("core.compute.and", gateInputs, gateResults);
        for (uint32_t i = 0; i < 3; ++i)
        {
            const std::array operands{one, i == 0 ? inputs[1] : reads[i - 1], one, i == 2 ? gated : inputs[0]};
            const std::array refs{ObjectRef::state(registers[i]), ObjectRef::state(state())};
            const std::array params{Parameter{model.intern("event_edges"), std::vector<std::string>{"posedge"}}};
            model.addOperation("core.state.regWrite", operands, {}, refs, params);
            const auto output = model.addOutput("q" + std::to_string(i), bit);
            const std::array outputOperands{reads[i]}; const std::array outputRefs{ObjectRef::output(output)};
            model.addOperation("core.output.write", outputOperands, {}, outputRefs);
        }
        PassManager manager(defaultDialectRegistry()); std::string error;
        for (auto name : {"cpu.st.split-phase", "cpu.st.form-event-domains", "cpu.st.build-compute-nodes",
                          "cpu.st.merge-compute-supernodes", "cpu.st.pack-active-words", "cpu.st.pack-emit-functions",
                          "cpu.st.layout-data", "cpu.st.build-schedule"})
        {
            const std::array<std::string_view, 2> nodes{"--max-op-in-compute-node", "1"};
            const std::array<std::string_view, 2> supernodes{"--max-op-in-compute-supernode", "1"};
            const std::span<const std::string_view> options = std::string_view(name) == "cpu.st.build-compute-nodes" ? nodes :
                std::string_view(name) == "cpu.st.merge-compute-supernodes" ? supernodes : std::span<const std::string_view>{};
            manager.addPass(defaultPassRegistry().create(name, options, error));
        }
        diag::Diagnostics diagnostics;
        check(manager.run(model, diagnostics).success, "trace model mapping failed");
        return model;
    }

    // Test-only two-state subset. Dense G traversal is independent of the sparse dispatch tables.
    class Trace
    {
    public:
        Trace(const GrhSimModel &model, bool sparse)
            : model_(model), sparse_(sparse), values_(model.values().size() + 1), inputs_(model.inputs().size() + 1),
              previousInputs_(values_.size()), active_(model.cpuMapping()->partitionTree.partitions.size() + 1, 1),
              arms_(active_.size(), 1), states(model.states().size() + 1), outputs(model.outputs().size() + 1)
        {
            for (const auto &op : model.operations())
                if (model.text(op.opType) == "core.input.read")
                    inputObjects_.push_back({model.results(op)[0], model.objectRefs(op)[0].index});
        }

        void eval(uint8_t clock, uint8_t data, uint8_t enable)
        {
            inputs_ = {0, clock, data, enable};
            const auto &mapping = *model_.cpuMapping(); const auto &schedule = *mapping.schedule;
            for (const auto &[value, object] : inputObjects_)
                if (previousInputs_[value.index] != inputs_[object])
                {
                    for (const auto &row : schedule.inputFanout)
                        if (row.source == value) apply(row.targets, arms_);
                    previousInputs_[value.index] = inputs_[object];
                }
            for (uint32_t round = 0; round < 16; ++round)
            {
                const auto oldStates = states;
                if (sparse_)
                {
                    for (auto seed : schedule.roundSeeds) active_[seed.index] = 1;
                    for (const auto &task : schedule.numaNodes[0].cores[0].tasks)
                    {
                        if (task.execution != CpuExecution::ActivityDrivenCompute) continue;
                        const auto &tree = mapping.partitionTree;
                        for (auto word : tree.partitions[task.partition.index - 1].children)
                            for (auto unit : tree.partitions[word.index - 1].children)
                            {
                                if (!active_[unit.index]) continue;
                                active_[unit.index] = 0;
                                for (auto node : tree.partitions[unit.index - 1].children)
                                    for (auto op : tree.partitions[node.index - 1].ops) compute(model_.operations()[op.index - 1]);
                            }
                    }
                }
                else for (const auto &op : model_.operations()) if (!isCpuCommitOp(model_.text(op.opType))) compute(op);
                std::vector<uint8_t> nextArms(arms_.size());
                if (sparse_)
                {
                    const auto &tree = mapping.partitionTree;
                    for (const auto &task : schedule.numaNodes[0].cores[0].tasks)
                    {
                        if (task.execution == CpuExecution::ActivityDrivenCompute) continue;
                        const auto &function = tree.partitions[task.partition.index - 1];
                        if (task.execution == CpuExecution::DomainGatedCommit && !arms_[function.parent.index]) continue;
                        for (auto unit : function.children)
                            for (auto op : tree.partitions[unit.index - 1].ops) commit(model_.operations()[op.index - 1], oldStates);
                    }
                }
                else for (const auto &op : model_.operations()) if (isCpuCommitOp(model_.text(op.opType))) commit(op, oldStates);
                bool again = false;
                for (const auto &row : schedule.commitStateFanout)
                    if (oldStates[row.source.index] != states[row.source.index])
                    { again = true; apply(row.targets, nextArms); }
                arms_ = std::move(nextArms);
                if (!again) return;
            }
            throw std::runtime_error("schedule trace did not converge");
        }

    private:
        void apply(const CpuActivationTargets &targets, std::vector<uint8_t> &arms)
        {
            for (auto target : targets.activate) active_[target.index] = 1;
            for (auto target : targets.arm) arms[target.index] = 1;
        }

        void compute(const SimOp &op)
        {
            const auto type = model_.text(op.opType); const auto operands = model_.operands(op);
            const auto refs = model_.objectRefs(op); const auto results = model_.results(op);
            uint8_t value = 0;
            if (type == "core.input.read") value = inputs_[refs[0].index];
            else if (type == "core.state.read") value = states[refs[0].index];
            else if (type == "core.compute.constant") value = 1;
            else if (type == "core.compute.and") value = values_[operands[0].index] & values_[operands[1].index];
            else if (type == "core.output.write") { outputs[refs[0].index] = values_[operands[0].index]; return; }
            else throw std::runtime_error("unsupported trace compute op");
            check(results.size() == 1, "trace result arity");
            const auto result = results[0];
            const bool changed = values_[result.index] != value; values_[result.index] = value;
            if (sparse_ && changed)
                for (const auto &row : model_.cpuMapping()->schedule->computeSupernodeFanout)
                    if (row.source == result) apply(row.targets, arms_);
        }

        void commit(const SimOp &op, const std::vector<uint8_t> &oldStates)
        {
            check(model_.text(op.opType) == "core.state.regWrite", "unsupported trace commit op");
            const auto operands = model_.operands(op); const auto refs = model_.objectRefs(op);
            const auto event = values_[operands[3].index];
            if (event && !oldStates[refs[1].index] && values_[operands[0].index] && values_[operands[2].index])
                states[refs[0].index] = values_[operands[1].index];
            states[refs[1].index] = event;
        }

        const GrhSimModel &model_;
        bool sparse_;
        std::vector<uint8_t> values_, inputs_, previousInputs_, active_, arms_;
        std::vector<std::pair<ValueId, uint32_t>> inputObjects_;

    public:
        std::vector<uint8_t> states, outputs;
    };
}

void cpuScheduleTraceTests()
{
    const auto model = traceModel(); Trace dense(model, false), sparse(model, true);
    const auto step = [&](uint8_t clock, uint8_t data, uint8_t enable) {
        dense.eval(clock, data, enable); sparse.eval(clock, data, enable);
        check(dense.states == sparse.states && dense.outputs == sparse.outputs, "sparse schedule differs from dense G trace");
    };
    step(0, 1, 1); step(1, 1, 1);
    check(sparse.outputs == std::vector<uint8_t>({0, 1, 0, 0}), "one rising edge advanced the register chain twice");
    step(0, 0, 1); step(1, 0, 1);
    check(sparse.outputs == std::vector<uint8_t>({0, 0, 1, 0}), "falling edge history sampling missed next rise");
    step(1, 0, 0); step(1, 0, 1);
    check(sparse.outputs == std::vector<uint8_t>({0, 0, 1, 1}), "derived edge at fixed input clock was lost");
    std::mt19937 generator(17);
    for (uint32_t i = 0; i < 256; ++i)
    {
        const auto sequence = generator();
        step((sequence >> 29) & 1, (sequence >> 30) & 1, (sequence >> 31) & 1);
    }
}
