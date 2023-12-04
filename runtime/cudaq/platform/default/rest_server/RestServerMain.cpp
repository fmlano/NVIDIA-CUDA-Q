/*******************************************************************************
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/
#if (defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include "crow.h"
#if (defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER))
#pragma GCC diagnostic pop
#endif
#include "JsonConvert.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include "mlir/ExecutionEngine/ExecutionEngine.h"
#include "mlir/ExecutionEngine/OptUtils.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/Verifier.h"
#include "mlir/InitAllDialects.h"
#include "mlir/InitAllPasses.h"
#include "mlir/InitAllTranslations.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Target/LLVMIR/Dialect/LLVMIR/LLVMToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Export.h"
#include "mlir/Tools/mlir-translate/Translation.h"
#include "mlir/Transforms/Passes.h"
#include "cudaq/Optimizer/Builder/Runtime.h"
#include "cudaq/Optimizer/CodeGen/Passes.h"
#include "cudaq/Optimizer/Dialect/CC/CCDialect.h"
#include "cudaq/Optimizer/Dialect/CC/CCOps.h"
#include "cudaq/Optimizer/Dialect/Quake/QuakeDialect.h"
#include "cudaq/Optimizer/Dialect/Quake/QuakeOps.h"
#include "cudaq/Optimizer/Transforms/Passes.h"
#include "mlir/Pass/PassManager.h"
#include "common/Logger.h"
#include "common/RuntimeMLIR.h"
#include "nvqir/CircuitSimulator.h"

namespace nvqir {
CircuitSimulator *getCircuitSimulatorInternal();
}
constexpr static char BOLD[] = "\033[1m";
constexpr static char RED[] = "\033[91m";
constexpr static char CLEAR[] = "\033[0m";

using namespace mlir;

// Debug only
constexpr int port = 3030;

namespace {
std::unique_ptr<ExecutionEngine>
jitCode(ModuleOp currentModule, std::vector<std::string> extraLibPaths = {}) {
  cudaq::info("Running jitCode.");
  auto module = currentModule.clone();
  auto ctx = module.getContext();
  PassManager pm(ctx);
  OpPassManager &optPM = pm.nest<func::FuncOp>();

  optPM.addPass(cudaq::opt::createUnwindLoweringPass());
  cudaq::opt::addAggressiveEarlyInlining(pm);
  pm.addPass(createCanonicalizerPass());
  pm.addPass(cudaq::opt::createApplyOpSpecializationPass());
  optPM.addPass(cudaq::opt::createClassicalMemToReg());
  pm.addPass(createCanonicalizerPass());
  pm.addPass(cudaq::opt::createExpandMeasurementsPass());
  pm.addPass(cudaq::opt::createLoopNormalize());
  pm.addPass(cudaq::opt::createLoopUnroll());
  pm.addPass(createCanonicalizerPass());
  optPM.addPass(cudaq::opt::createQuakeAddDeallocs());
  optPM.addPass(cudaq::opt::createQuakeAddMetadata());
  pm.addPass(createCanonicalizerPass());
  pm.addPass(createCSEPass());
  if (failed(pm.run(module)))
    throw std::runtime_error("Failed to JIT compile the Quake representation.");

  optPM.addPass(cudaq::opt::createLowerToCFGPass());
  optPM.addPass(cudaq::opt::createCombineQuantumAllocations());
  pm.addPass(createCanonicalizerPass());
  pm.addPass(createCSEPass());
  pm.addPass(cudaq::opt::createConvertToQIRPass());


  if (failed(pm.run(module)))
    throw std::runtime_error("Failed to JIT compile the Quake representation.");

  cudaq::info("- Pass manager was applied.");
  ExecutionEngineOptions opts;
  opts.transformer = [](llvm::Module *m) { return llvm::ErrorSuccess(); };
  opts.jitCodeGenOptLevel = llvm::CodeGenOpt::None;
  SmallVector<StringRef, 4> sharedLibs;
  for (auto &lib : extraLibPaths) {
    cudaq::info("Extra library loaded: {}", lib);
    sharedLibs.push_back(lib);
  }
  opts.sharedLibPaths = sharedLibs;
  opts.llvmModuleBuilder =
      [](Operation *module,
         llvm::LLVMContext &llvmContext) -> std::unique_ptr<llvm::Module> {
    llvmContext.setOpaquePointers(false);
    auto llvmModule = translateModuleToLLVMIR(module, llvmContext);
    if (!llvmModule) {
      llvm::errs() << "Failed to emit LLVM IR\n";
      return nullptr;
    }
    ExecutionEngine::setupTargetTriple(llvmModule.get());
    std::cout << "LLVM:\n";
    llvmModule->dump();
    return llvmModule;
  };

  cudaq::info(" - Creating the MLIR ExecutionEngine");
  auto jitOrError = ExecutionEngine::create(module, opts);
  assert(!!jitOrError);

  auto uniqueJit = std::move(jitOrError.get());

  cudaq::info("- JIT Engine created successfully.");

  return uniqueJit;
}
} // namespace
int main(int argc, char **argv) {
  crow::SimpleApp app;
  CROW_ROUTE(app, "/")
  ([]() { return "Hello, world!"; });

  CROW_ROUTE(app, "/job").methods("POST"_method)([](const crow::request &req) {
    CROW_LOG_INFO << "msg from client: " << req.body;
    auto requestJson = json::parse(req.body);
    const std::string quake = requestJson["quake"];
    auto contextPtr = cudaq::initializeMLIR();
    auto fileBuf = llvm::MemoryBuffer::getMemBufferCopy(quake);
    // Parse the input mlir.
    llvm::SourceMgr sourceMgr;
    sourceMgr.AddNewSourceBuffer(std::move(fileBuf), llvm::SMLoc());
    auto module = parseSourceFile<ModuleOp>(sourceMgr, contextPtr.get());
    std::cout << "Done\n";
    module->dump();
    auto engine = jitCode(*module);
    const std::string kernelName = requestJson["kernel-name"];
    const std::string entryPoint =
        std::string(cudaq::runtime::cudaqGenPrefixName) + kernelName;
    //  Extract the entry point
    auto fnPtr = engine->lookup(entryPoint);
    
    if (!fnPtr)
      throw std::runtime_error("Failed to get entry function");

    auto fn = reinterpret_cast<void (*)()>(*fnPtr);
    // const auto simulator = requestJson["simulator"];
    // TODO: proper setting for simulator
    const std::string simulator = "qpp";
    const std::filesystem::path cudaqLibPath{cudaq::getCUDAQLibraryPath()};
#if defined(__APPLE__) && defined(__MACH__)
    const auto libSuffix = "dylib";
#else
    const auto libSuffix = "so";
#endif
    const auto simLibPath = cudaqLibPath.parent_path() /
                            fmt::format("libnvqir-{}.{}", simulator, libSuffix);
    cudaq::info("Request simulator {} at {}", simulator, simLibPath.c_str());
    void *simLibHandle =
        dlopen(simLibPath.c_str(), RTLD_GLOBAL | RTLD_NOW);
    if (!simLibHandle) {
      char *error_msg = dlerror();
      throw std::runtime_error(fmt::format(
          "Failed to open simulator backend library: {}.",
          error_msg ? std::string(error_msg) : std::string("Unknown error")));
    }

    auto *circuitSimulator = nvqir::getCircuitSimulatorInternal();
    auto executionContext = std::make_unique<cudaq::ExecutionContext>(
        requestJson["execution-context"]["name"]);
    json executionContexJs = requestJson["execution-context"];
    cudaq::from_json(executionContexJs, *executionContext);
    circuitSimulator->setExecutionContext(executionContext.get());
    fn();
    circuitSimulator->resetExecutionContext();
    executionContext->result.dump();
    dlclose(simLibHandle);
    return "{}";
  });
  app.port(port).run();
}
