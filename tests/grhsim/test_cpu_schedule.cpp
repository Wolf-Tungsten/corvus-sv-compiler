#include "grhsim/backend/cpu.hpp"
#include "grhsim/dialect/registry.hpp"
#include "grhsim/io/json.hpp"
#include "grhsim/ir/verifier.hpp"
#include "grhsim/pass/pass.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

void cpuScheduleTraceTests();

namespace
{
    using namespace wolvrix::lib;
    using namespace grhsim;

    void require(bool condition, std::string_view message)
    {
        if (!condition) throw std::runtime_error(std::string(message));
    }

    void run(GrhSimModel &model, std::string_view name, std::span<const std::string_view> args = {})
    {
        std::string error;
        PassManager manager(defaultDialectRegistry());
        auto pass = defaultPassRegistry().create(name, args, error);
        require(bool(pass), error);
        manager.addPass(std::move(pass));
        diag::Diagnostics diagnostics;
        const auto result = manager.run(model, diagnostics);
        for (const auto &message : diagnostics.messages()) std::cout << message.context << ": " << message.message << '\n';
        require(result.success && result.changed && !model.poisoned(), "CPU schedule pipeline failed");
    }

    void layout(GrhSimModel &model)
    {
        run(model, "cpu.st.split-phase"); run(model, "cpu.st.form-event-domains");
        const std::array<std::string_view, 2> nodes{"--max-op-in-compute-node", "1"};
        const std::array<std::string_view, 2> supernodes{"--max-op-in-compute-supernode", "1"};
        run(model, "cpu.st.build-compute-nodes", nodes); run(model, "cpu.st.merge-compute-supernodes", supernodes);
        run(model, "cpu.st.pack-active-words"); run(model, "cpu.st.pack-emit-functions");
        run(model, "cpu.st.layout-data");
    }

    void roundTrip(const GrhSimModel &model)
    {
        diag::Diagnostics diagnostics;
        std::ostringstream first, second;
        require(writeGrhSimJson(model, first, defaultDialectRegistry(), diagnostics), "schedule JSON store failed");
        std::istringstream input(first.str());
        auto loaded = readGrhSimJson(input, defaultDialectRegistry(), diagnostics);
        for (const auto &message : diagnostics.messages()) std::cerr << message.message << '\n';
        require(bool(loaded), "schedule JSON load failed");
        require(writeGrhSimJson(*loaded, second, defaultDialectRegistry(), diagnostics) && first.str() == second.str(),
                "schedule JSON roundtrip differs");
    }

    struct Fixture
    {
        GrhSimModel model{"schedule"};
        TypeId bit;
        ValueId clkA, clkB, data, unused, duplicateClock, derived, deadRead, time;
        StateId q1, q2, q3, upstream, other, dead, offData;
        std::vector<StateId> histories;

        Fixture()
        {
            model.addDialect("core", "1", "wolvrix.grhsim.core.v1");
            bit = model.logicType(1, false, LogicDomain::TwoState);
            const auto clockInput = model.addInput("clk_a", bit);
            clkA = readInput(clockInput); duplicateClock = readInput(clockInput);
            clkB = readInput(model.addInput("clk_b", bit));
            data = readInput(model.addInput("data", bit));
            unused = readInput(model.addInput("unused", bit));
            q1 = addState(); q2 = addState(); q3 = addState(); upstream = addState(); other = addState();
            dead = addState(); offData = addState();
            auto a = readState(q1), b = readState(q2), c = readState(q3);
            auto up = readState(upstream), extra = readState(other), hidden = readState(offData);
            deadRead = readState(dead);
            const auto next = compute("core.compute.xor", {data, up});
            write(q1, next, {clkA}); write(q2, a, {clkA});
            derived = compute("core.compute.and", {b, clkB});
            write(q3, b, {derived, clkA}); write(q3, extra, {clkB});
            write(dead, hidden, {clkB});
            write(dead, hidden, {});
            output(c); output(duplicateClock);
            compute("core.compute.xor", {data, data});
            time = model.addValue(bit);
            const std::array results{time};
            const std::array parameters{Parameter{model.intern("name"), std::string("time")},
                                       Parameter{model.intern("has_side_effects"), false},
                                       Parameter{model.intern("proc_kind"), std::string("always_comb")},
                                       Parameter{model.intern("has_timing"), false}};
            model.addOperation("core.system.function", {}, results, {}, parameters);
        }

        StateId addState(TypeId type = {})
        {
            const auto id = model.addState("s" + std::to_string(model.states().size()), type ? type : bit);
            const std::array parameters{Parameter{model.intern("value"), std::string("1'b0")}};
            const std::array steps{InitStep{model.intern("core.init.const"), {0, 1}}};
            model.addInit(id, steps, parameters);
            return id;
        }

        ValueId readInput(InputId input)
        {
            const auto value = model.addValue(bit);
            const std::array results{value}; const std::array refs{ObjectRef::input(input)};
            model.addOperation("core.input.read", {}, results, refs); return value;
        }

        ValueId readState(StateId state)
        {
            const auto value = model.addValue(bit);
            const std::array results{value}; const std::array refs{ObjectRef::state(state)};
            model.addOperation("core.state.read", {}, results, refs); return value;
        }

        ValueId compute(std::string_view op, std::initializer_list<ValueId> operands)
        {
            const auto value = model.addValue(bit); const std::array results{value};
            model.addOperation(op, {operands.begin(), operands.size()}, results); return value;
        }

        void write(StateId target, ValueId value, std::initializer_list<ValueId> events)
        {
            std::vector<ValueId> operands{data, value, data};
            operands.insert(operands.end(), events.begin(), events.end());
            std::vector<ObjectRef> refs{ObjectRef::state(target)};
            for (auto event : events)
            {
                (void)event;
                histories.push_back(addState()); refs.push_back(ObjectRef::state(histories.back()));
            }
            const std::array parameters{Parameter{model.intern("event_edges"), std::vector<std::string>(events.size(), "posedge")}};
            model.addOperation(events.size() ? "core.state.regWrite" : "core.state.latchWrite", operands, {}, refs, parameters);
        }

        void output(ValueId value)
        {
            const auto id = model.addOutput("out" + std::to_string(model.outputs().size()), bit);
            const std::array operands{value}; const std::array refs{ObjectRef::output(id)};
            model.addOperation("core.output.write", operands, {}, refs);
        }
    };

    template <typename Id>
    const CpuActivationTargets *targets(const std::vector<CpuFanoutEntry<Id>> &rows, Id id)
    {
        for (const auto &row : rows) if (row.source == id) return &row.targets;
        return nullptr;
    }

    bool contains(const std::vector<PartitionId> &ids, PartitionId id)
    {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    void sharedHistoryTests()
    {
        for (unsigned mode = 0; mode < 3; ++mode)
        {
            Fixture fixture;
            auto &model = fixture.model;
            const auto history = fixture.histories.front();
            if (mode < 2)
            {
                const std::array operands{fixture.data, fixture.data, fixture.data, mode ? fixture.clkB : fixture.clkA};
                const std::array refs{ObjectRef::state(fixture.addState()), ObjectRef::state(history)};
                const std::array params{Parameter{model.intern("event_edges"), std::vector<std::string>{"posedge"}}};
                model.addOperation("core.state.regWrite", operands, {}, refs, params);
            }
            else
            {
                const std::array operands{fixture.data, fixture.data, fixture.data};
                const std::array refs{ObjectRef::state(history)};
                model.addOperation("core.state.latchWrite", operands, {}, refs);
            }
            layout(model); run(model, "cpu.st.build-schedule");
            const auto &mapping = *model.cpuMapping();
            unsigned fallbacks = 0;
            for (const auto &task : mapping.schedule->numaNodes[0].cores[0].tasks)
            {
                const auto &function = mapping.partitionTree.partitions[task.partition.index - 1];
                if (task.execution != CpuExecution::AlwaysScanCommit ||
                    !mapping.partitionTree.partitions[function.parent.index - 1].attrs.eventGate) continue;
                ++fallbacks;
                auto broken = model.clone(); auto bad = mapping;
                bad.schedule->numaNodes[0].cores[0].tasks[task.id.index - 1].execution = CpuExecution::DomainGatedCommit;
                broken.setCpuMapping(std::move(bad)); diag::Diagnostics diagnostics;
                require(!verifyGrhSimModel(broken, defaultDialectRegistry(), diagnostics),
                        "verifier accepted gated conflicting history");
            }
            require(fallbacks == (mode == 0 ? 0u : mode == 1 ? 2u : 1u),
                    "same-event/shared-clock/ordinary-writer history fallback classification differs");
            roundTrip(model);
        }
    }

    void unitTests()
    {
        Fixture fixture;
        auto &model = fixture.model;
        layout(model);
        const auto revision = model.semanticRevision();
        run(model, "cpu.st.build-schedule");
        const auto &mapping = *model.cpuMapping();
        const auto &schedule = *mapping.schedule;
        require(model.semanticRevision() == revision && model.mappings().front().complete, "schedule completion/revision wrong");
        require(schedule.numaNodes.size() == 1 && schedule.numaNodes[0].cores.size() == 1, "wrong hardware slots");
        std::set<uint32_t> expected{fixture.q1.index, fixture.q2.index, fixture.q3.index, fixture.upstream.index, fixture.other.index};
        for (auto state : fixture.histories) expected.insert(state.index);
        std::set<uint32_t> actual;
        for (std::size_t index = 0; index < schedule.quiescenceProjection.size(); ++index)
            if (schedule.quiescenceProjection[index]) actual.insert(index);
        require(actual == expected, "E omitted transitive writer/history state or included dead state/data");
        std::set<uint32_t> tracked = expected;
        tracked.insert(fixture.dead.index); tracked.insert(fixture.offData.index);
        std::set<uint32_t> fanoutKeys;
        for (const auto &row : schedule.commitStateFanout) fanoutKeys.insert(row.source.index);
        require(fanoutKeys == tracked, "commit fanout omitted a read state or tracked an unread one");
        const auto owner = [&](ValueId id) { return mapping.dataLayout->values[id.index - 1].owner; };
        require(targets(schedule.commitStateFanout, fixture.dead) &&
                contains(targets(schedule.commitStateFanout, fixture.dead)->activate, owner(fixture.deadRead)),
                "nonprojected state reader was not armed by commit fanout");
        require(!contains(schedule.roundSeeds, owner(fixture.deadRead)) && contains(schedule.roundSeeds, owner(fixture.time)),
                "unobserved state reader kept its seed or external time source was not seeded");
        require(!targets(schedule.inputFanout, fixture.unused), "unused input got a shadow");
        require(targets(schedule.inputFanout, fixture.duplicateClock) &&
                contains(targets(schedule.inputFanout, fixture.clkA)->activate, owner(fixture.clkA)),
                "input.read producer or repeated input read lost activation");
        PartitionId mixed;
        for (const auto &partition : mapping.partitionTree.partitions)
            if (partition.attrs.eventGate && partition.attrs.eventGate->source == CpuEventSource::Derived)
                mixed = partition.id;
        require(mixed && contains(targets(schedule.inputFanout, fixture.clkA)->arm, mixed) &&
                contains(targets(schedule.computeSupernodeFanout, fixture.derived)->arm, mixed),
                "mixed event domain lost an input or derived arm source");
        require(schedule.inputShadows.size() == schedule.inputFanout.size() && schedule.inputShadowBytes % 8 == 0,
                "shadow key set/alignment differs from inputs");
        for (const auto &row : schedule.computeSupernodeFanout)
        {
            require(!contains(row.targets.activate, owner(row.source)), "same-supernode activation edge retained");
            std::set<uint32_t> unique;
            for (auto id : row.targets.activate) unique.insert(id.index);
            require(unique.size() == row.targets.activate.size(), "duplicate activation target retained");
        }
        require(std::count_if(schedule.numaNodes[0].cores[0].tasks.begin(), schedule.numaNodes[0].cores[0].tasks.end(),
                              [](const auto &task) { return task.execution == CpuExecution::AlwaysScanCommit; }) == 1,
                "general domain did not get an always-scan task");
        roundTrip(model);
        const auto reject = [&](CpuBackendMapping bad) {
            auto broken = model.clone(); broken.setCpuMapping(std::move(bad));
            diag::Diagnostics diagnostics;
            require(!verifyGrhSimModel(broken, defaultDialectRegistry(), diagnostics), "accepted corrupt schedule");
        };
        auto bad = mapping; bad.schedule->numaNodes.push_back(bad.schedule->numaNodes.front()); reject(bad);
        bad = mapping; bad.schedule->numaNodes[0].cores[0].tasks[0].execution = CpuExecution::AlwaysScanCommit; reject(bad);
        bad = mapping; bad.schedule->numaNodes[0].cores[0].tasks[0].waitsFor.push_back({1, 0}); reject(bad);
        bad = mapping; bad.schedule->numaNodes[0].cores[0].tasks.pop_back(); reject(bad);
        bad = mapping; bad.schedule->computeSupernodeFanout.pop_back(); reject(bad);
        bad = mapping; bad.schedule->inputFanout.front().targets.activate.clear(); reject(bad);
        bad = mapping; bad.schedule->commitStateFanout.pop_back(); reject(bad);
        bad = mapping; bad.schedule->commitStateFanout.push_back({fixture.dead, {}}); reject(bad);
        bad = mapping; bad.schedule->roundSeeds.clear(); reject(bad);
        bad = mapping; bad.schedule->inputShadows.front().offset++; reject(bad);
        bad = mapping; bad.schedule->inputShadowBytes++; reject(bad);
        bad = mapping; bad.schedule.reset(); reject(bad);
        bad = mapping; bad.stage = CpuMappingStage::DataLayout; reject(bad);
        auto clone = model.clone();
        diag::Diagnostics cloneDiagnostics;
        require(verifyGrhSimModel(clone, defaultDialectRegistry(), cloneDiagnostics), "schedule clone did not rebind identity");
        clone.commitSemanticMutation(); require(!clone.cpuMapping(), "semantic mutation retained complete schedule");
        run(model, "cpu.st.split-phase"); require(!model.cpuMapping()->schedule, "upstream rerun retained stale schedule");

        GrhSimModel empty("empty"); empty.addDialect("core", "1", "wolvrix.grhsim.core.v1");
        std::string error;
        const std::array<std::string_view, 1> invalid{"--unknown"};
        require(!defaultPassRegistry().create("cpu.st.build-schedule", invalid, error), "schedule accepted unknown option");
        auto pass = defaultPassRegistry().create("cpu.st.build-schedule", {}, error);
        diag::Diagnostics missingDiagnostics;
        require(!pass->run(empty, missingDiagnostics).success && !empty.cpuMapping(), "schedule accepted missing layout");
        layout(empty); run(empty, "cpu.st.build-schedule"); roundTrip(empty);
        require(empty.cpuMapping()->schedule->numaNodes[0].cores[0].tasks.empty(), "empty graph got tasks");
    }

    void memoryAndCallTests()
    {
        Fixture fixture;
        auto &model = fixture.model;
        const auto memory = fixture.addState(model.arrayType(fixture.bit, 4));
        const auto addressState = fixture.addState(), dataState = fixture.addState();
        const auto address = fixture.readState(addressState), data = fixture.readState(dataState);
        const auto value = model.addValue(fixture.bit);
        const std::array readOperands{address}, readResults{value};
        const std::array readRefs{ObjectRef::state(memory)};
        model.addOperation("core.state.memRead", readOperands, readResults, readRefs);
        fixture.output(value);
        const auto memoryHistory = fixture.addState();
        const std::array writeOperands{fixture.data, address, data, fixture.data, fixture.clkA};
        const std::array writeRefs{ObjectRef::state(memory), ObjectRef::state(memoryHistory)};
        const std::array edges{Parameter{model.intern("event_edges"), std::vector<std::string>{"negedge"}}};
        model.addOperation("core.state.memWrite", writeOperands, {}, writeRefs, edges);

        const auto dpiHistory = fixture.addState(), taskHistory = fixture.addState();
        const auto function = model.addExternFunction("dpi", "core.dpi", "dpi", {}, fixture.bit);
        const auto result = model.addValue(fixture.bit);
        const std::array callOperands{fixture.data, fixture.clkB};
        const std::array callResults{result};
        const std::array callRefs{ObjectRef::function(function), ObjectRef::state(dpiHistory)};
        model.addOperation("core.dpi.call", callOperands, callResults, callRefs, edges);
        const std::array taskRefs{ObjectRef::state(taskHistory)};
        const auto task = model.addOperation("core.system.task", callOperands, {}, taskRefs, edges);
        layout(model); run(model, "cpu.st.build-schedule");
        const auto &mapping = *model.cpuMapping(); const auto &schedule = *mapping.schedule;
        for (auto state : {memory, addressState, dataState, memoryHistory, dpiHistory, taskHistory})
            require(targets(schedule.commitStateFanout, state), "memory/call history E dependency omitted");
        const auto dpiOwner = mapping.dataLayout->values[result.index - 1].owner;
        require(contains(schedule.roundSeeds, dpiOwner) && contains(targets(schedule.commitStateFanout, dpiHistory)->activate, dpiOwner),
                "DPI source/history activation omitted");
        PartitionId taskOwner;
        for (const auto &partition : mapping.partitionTree.partitions)
            if (std::find(partition.ops.begin(), partition.ops.end(), task) != partition.ops.end()) taskOwner = partition.parent;
        require(contains(schedule.roundSeeds, taskOwner) && contains(targets(schedule.commitStateFanout, taskHistory)->activate, taskOwner),
                "task source/history activation omitted");
        roundTrip(model);
    }

    void printStats(const GrhSimModel &model)
    {
        const auto &schedule = *model.cpuMapping()->schedule;
        const auto printFanout = [](auto name, const auto &rows) {
            uint64_t activate = 0, arm = 0;
            for (const auto &row : rows) { activate += row.targets.activate.size(); arm += row.targets.arm.size(); }
            std::cout << name << " sources=" << rows.size() << " activate=" << activate << " arm=" << arm << '\n';
        };
        printFanout("input", schedule.inputFanout); printFanout("compute", schedule.computeSupernodeFanout);
        printFanout("state", schedule.commitStateFanout);
        std::cout << "tasks=" << schedule.numaNodes[0].cores[0].tasks.size() << " round_seeds=" << schedule.roundSeeds.size()
                  << " input_shadow_bytes=" << schedule.inputShadowBytes << '\n';
    }
}

int main(int argc, char **argv)
{
    try
    {
        unitTests();
        sharedHistoryTests();
        memoryAndCallTests();
        cpuScheduleTraceTests();
        if (argc == 3 || argc == 4)
        {
            diag::Diagnostics diagnostics;
            auto model = loadGrhSimModel(argv[2], defaultDialectRegistry(), diagnostics);
            require(bool(model), "schedule checkpoint load failed");
            if (argc == 4 && std::string_view(argv[1]) == "--schedule")
            {
                run(*model, "cpu.st.build-schedule");
                require(storeGrhSimModel(*model, argv[3], defaultDialectRegistry(), diagnostics), "schedule store failed");
                auto loaded = loadGrhSimModel(argv[3], defaultDialectRegistry(), diagnostics);
                require(bool(loaded) && loaded->cpuMapping()->schedule == model->cpuMapping()->schedule, "schedule reload differs");
                require(storeGrhSimModel(*loaded, std::string(argv[3]) + ".roundtrip.json", defaultDialectRegistry(), diagnostics),
                        "schedule roundtrip store failed");
            }
            else require(argc == 3 && std::string_view(argv[1]) == "--inspect", "invalid arguments");
            require(model->cpuMapping() && model->cpuMapping()->schedule, "checkpoint has no schedule");
            printStats(*model);
        }
        else require(argc == 1, "usage: grhsim-cpu-schedule-tests [--schedule input output | --inspect input]");
        std::cout << "CPU schedule tests passed\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
