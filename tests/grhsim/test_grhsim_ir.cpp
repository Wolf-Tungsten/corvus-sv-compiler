#include "core/grh.hpp"
#include "grhsim/convert/grh_to_grhsim.hpp"
#include "grhsim/dialect/registry.hpp"
#include "grhsim/io/json.hpp"
#include "grhsim/ir/verifier.hpp"
#include "grhsim/pass/pass.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    using namespace wolvrix::lib;

    int fail(const std::string &message)
    {
        std::cerr << "[grhsim-ir] " << message << '\n';
        return 1;
    }

    std::string readFile(const std::filesystem::path &path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }

    bool writeFile(const std::filesystem::path &path, const std::string &contents)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << contents;
        return static_cast<bool>(output);
    }

    grh::Design makeFlatDesign()
    {
        grh::Design design;
        auto &graph = design.createGraph("top");

        const auto enable = graph.createValue(graph.internSymbol("enable"), 1, false);
        const auto data = graph.createValue(graph.internSymbol("data"), 8, false);
        const auto clock = graph.createValue(graph.internSymbol("clock"), 1, false);
        graph.bindInputPort("enable", enable);
        graph.bindInputPort("data", data);
        graph.bindInputPort("clock", clock);

        const auto mask = graph.createValue(graph.internSymbol("mask"), 8, false);
        const auto maskOp = graph.createOperation(grh::OperationKind::kConstant,
                                                  graph.internSymbol("mask_const"));
        graph.setAttr(maskOp, "constValue", std::string("8'hff"));
        graph.addResult(maskOp, mask);

        const auto reg = graph.createOperation(grh::OperationKind::kRegister,
                                               graph.internSymbol("q"));
        graph.setAttr(reg, "width", int64_t{8});
        graph.setAttr(reg, "isSigned", false);
        graph.setAttr(reg, "initValue", std::string("8'h01"));

        const auto q = graph.createValue(graph.internSymbol("q_value"), 8, false);
        const auto read = graph.createOperation(grh::OperationKind::kRegisterReadPort,
                                                graph.internSymbol("q_read"));
        graph.setAttr(read, "regSymbol", std::string("q"));
        graph.addResult(read, q);

        const auto write = graph.createOperation(grh::OperationKind::kRegisterWritePort,
                                                 graph.internSymbol("q_write"));
        graph.setAttr(write, "regSymbol", std::string("q"));
        graph.setAttr(write, "eventEdge", std::vector<std::string>{"posedge"});
        graph.addOperand(write, enable);
        graph.addOperand(write, data);
        graph.addOperand(write, mask);
        graph.addOperand(write, clock);

        const auto sum = graph.createValue(graph.internSymbol("sum"), 8, false);
        const auto add = graph.createOperation(grh::OperationKind::kAdd,
                                               graph.internSymbol("sum_add"));
        graph.addOperand(add, q);
        graph.addOperand(add, data);
        graph.addResult(add, sum);
        graph.bindOutputPort("sum", sum);
        design.markAsTop("top");
        return design;
    }

    int runRoundTripTest(const std::filesystem::path &artifactDir)
    {
        auto design = makeFlatDesign();
        diag::Diagnostics diagnostics;
        grhsim::GrhToGrhSimOptions options;
        options.top = "top";
        options.logicDomain = grhsim::LogicDomain::TwoState;
        options.keepOrigins = true;
        auto model = grhsim::lowerGrhToGrhSim(design, options, diagnostics);
        if (!model || diagnostics.hasError()) return fail("flat GRH lowering failed");
        if (model->inputs().size() != 3 || model->outputs().size() != 1)
            return fail("lowered interface object counts are wrong");
        if (model->states().size() != 2 || model->initRecords().size() != 2)
            return fail("register or event-history state was not materialized");
        if (model->operations().size() != 8)
            return fail("unexpected lowered operation count");

        bool foundRegWrite = false;
        for (const auto &op : model->operations())
        {
            if (model->text(op.opType) != "core.state.regWrite") continue;
            foundRegWrite = true;
            const auto refs = model->objectRefs(op);
            if (refs.size() != 2 || refs[0].kind != grhsim::ObjectKind::State ||
                refs[1].kind != grhsim::ObjectKind::State)
                return fail("register write target/event-history refs are wrong");
            bool foundEdges = false;
            for (const auto &parameter : model->parameters(op))
            {
                if (model->text(parameter.name) == "event_edges") foundEdges = true;
            }
            if (!foundEdges) return fail("eventEdge was not normalized to event_edges");
        }
        if (!foundRegWrite) return fail("core.state.regWrite is missing");

        diag::Diagnostics passDiagnostics;
        std::string passError;
        auto pass = grhsim::defaultPassRegistry().create("grhsim.verify", {}, passError);
        if (!pass) return fail("grhsim.verify registry lookup failed: " + passError);
        grhsim::PassManager manager(grhsim::defaultDialectRegistry());
        manager.addPass(std::move(pass));
        const auto passResult = manager.run(*model, passDiagnostics);
        if (!passResult.success || passResult.changed || passDiagnostics.hasError())
            return fail("grhsim.verify pass failed or reported mutation");

        std::filesystem::create_directories(artifactDir);
        const auto firstPath = artifactDir / "grhsim_v1.json";
        const auto secondPath = artifactDir / "grhsim_v1_roundtrip.json";
        diag::Diagnostics storeDiagnostics;
        if (!grhsim::storeGrhSimModel(*model, firstPath, grhsim::defaultDialectRegistry(),
                                      storeDiagnostics))
            return fail("GrhSIM JSON store failed");
        const auto originalIdentity = model->identity();
        diag::Diagnostics loadDiagnostics;
        auto loaded = grhsim::loadGrhSimModel(firstPath, grhsim::defaultDialectRegistry(),
                                              loadDiagnostics);
        if (!loaded || loadDiagnostics.hasError()) return fail("GrhSIM JSON load failed");
        if (loaded->identity() == originalIdentity)
            return fail("load reused serialized/runtime model identity");
        if (loaded->semanticRevision() != 1 || loaded->metadataRevision() != 1)
            return fail("loaded model revisions must restart at one");
        diag::Diagnostics secondStoreDiagnostics;
        if (!grhsim::storeGrhSimModel(*loaded, secondPath, grhsim::defaultDialectRegistry(),
                                      secondStoreDiagnostics))
            return fail("round-trip GrhSIM JSON store failed");
        if (readFile(firstPath) != readFile(secondPath))
            return fail("store/load/store did not produce stable bytes");

        std::string invalidFormat = readFile(firstPath);
        const auto formatPos = invalidFormat.find("wolvrix.grhsim.v1");
        if (formatPos == std::string::npos) return fail("stored format marker is missing");
        invalidFormat.replace(formatPos, std::string("wolvrix.grhsim.v1").size(), "wolvrix.grhsim.v0");
        const auto invalidFormatPath = artifactDir / "grhsim_invalid_format.json";
        if (!writeFile(invalidFormatPath, invalidFormat)) return fail("failed to write bad format fixture");
        diag::Diagnostics invalidFormatDiagnostics;
        if (grhsim::loadGrhSimModel(invalidFormatPath, grhsim::defaultDialectRegistry(),
                                    invalidFormatDiagnostics) || !invalidFormatDiagnostics.hasError())
            return fail("loader accepted an unsupported format");

        std::string invalidCount = readFile(firstPath);
        const std::string operationCount = "\"operations\":8";
        const auto operationCountPos = invalidCount.find(operationCount);
        if (operationCountPos == std::string::npos)
            return fail("could not locate serialized operation count");
        invalidCount.replace(operationCountPos, operationCount.size(), "\"operations\":9");
        const auto invalidCountPath = artifactDir / "grhsim_invalid_count.json";
        if (!writeFile(invalidCountPath, invalidCount))
            return fail("failed to write bad count fixture");
        diag::Diagnostics invalidCountDiagnostics;
        if (grhsim::loadGrhSimModel(invalidCountPath, grhsim::defaultDialectRegistry(),
                                    invalidCountDiagnostics) || !invalidCountDiagnostics.hasError())
            return fail("loader accepted a mismatched table count");

        std::string invalidReference = readFile(firstPath);
        const std::string valuePrefix = "\"values\":[[1,1,";
        const auto valuePos = invalidReference.find(valuePrefix);
        if (valuePos == std::string::npos) return fail("could not locate value reference fixture");
        invalidReference.replace(valuePos, valuePrefix.size(), "\"values\":[[1,999,");
        const auto invalidReferencePath = artifactDir / "grhsim_invalid_reference.json";
        if (!writeFile(invalidReferencePath, invalidReference))
            return fail("failed to write bad reference fixture");
        diag::Diagnostics invalidReferenceDiagnostics;
        if (grhsim::loadGrhSimModel(invalidReferencePath, grhsim::defaultDialectRegistry(),
                                    invalidReferenceDiagnostics) || !invalidReferenceDiagnostics.hasError())
            return fail("loader accepted an invalid TypeId reference");
        return 0;
    }

    int runDetachedValueTest()
    {
        for (unsigned mode = 0; mode < 3; ++mode)
        {
            grh::Design design;
            auto &graph = design.createGraph("top"); design.markAsTop("top");
            const auto input = graph.createValue(graph.internSymbol("unused_input"), 8, false);
            graph.bindInputPort("unused_input", input);
            const auto produced = graph.createValue(graph.internSymbol("produced"), 8, false);
            const auto constant = graph.createOperation(grh::OperationKind::kConstant, graph.internSymbol("constant"));
            graph.setAttr(constant, "constValue", std::string("8'h5a")); graph.addResult(constant, produced);
            const auto detached = graph.createValue(graph.internSymbol("detached"), 8, false);
            if (mode == 1) graph.bindOutputPort("floating", detached);
            if (mode == 2)
            {
                const auto result = graph.createValue(graph.internSymbol("result"), 8, false);
                const auto invert = graph.createOperation(grh::OperationKind::kNot, graph.internSymbol("invert"));
                graph.addOperand(invert, detached); graph.addResult(invert, result);
                graph.bindOutputPort("result", result);
            }
            diag::Diagnostics diagnostics;
            grhsim::GrhToGrhSimOptions options; options.top = "top";
            options.logicDomain = grhsim::LogicDomain::TwoState;
            const auto model = grhsim::lowerGrhToGrhSim(design, options, diagnostics);
            if (mode == 0)
            {
                if (!model || diagnostics.hasError() || model->values().size() != 2 || model->operations().size() != 2)
                    return fail("detached value was not skipped or unused input/producer was removed");
            }
            else if (model || !diagnostics.hasError()) return fail("referenced undriven value was silently dropped");
        }
        return 0;
    }

    int runHierarchyRejectionTest()
    {
        grh::Design design;
        auto &graph = design.createGraph("hier");
        graph.createOperation(grh::OperationKind::kInstance, graph.internSymbol("child"));
        design.markAsTop("hier");
        diag::Diagnostics diagnostics;
        grhsim::GrhToGrhSimOptions options;
        options.top = "hier";
        auto model = grhsim::lowerGrhToGrhSim(design, options, diagnostics);
        if (model || !diagnostics.hasError()) return fail("hierarchical GRH was not rejected");
        return 0;
    }
}

#ifndef WOLVRIX_GRHSIM_TEST_ARTIFACT_DIR
#error "WOLVRIX_GRHSIM_TEST_ARTIFACT_DIR must be defined"
#endif

int main()
{
    try
    {
        if (const int status = runRoundTripTest(WOLVRIX_GRHSIM_TEST_ARTIFACT_DIR); status != 0)
            return status;
        if (const int status = runDetachedValueTest(); status != 0) return status;
        return runHierarchyRejectionTest();
    }
    catch (const std::exception &ex)
    {
        return fail(std::string("unexpected exception: ") + ex.what());
    }
}
