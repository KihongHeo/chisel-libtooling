#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "TransformationManager.h"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include <iostream>
#include <sstream>

#include "Transformation.h"

using namespace clang;


int TransformationManager::ErrorInvalidCounter = 1;

TransformationManager *TransformationManager::Instance;

std::map<std::string, Transformation *>
    *TransformationManager::TransformationsMapPtr;

TransformationManager *TransformationManager::GetInstance() {
  if (TransformationManager::Instance) {
    return TransformationManager::Instance;
  }
  TransformationManager::Instance = new TransformationManager();
  assert(TransformationManager::Instance);
  TransformationManager::Instance->TransformationsMap =
      *TransformationManager::TransformationsMapPtr;
  return TransformationManager::Instance;
}

Preprocessor &TransformationManager::getPreprocessor() {
  return GetInstance()->ClangInstance->getPreprocessor();
}

/*bool TransformationManager::isCXXLangOpt()
{
  TransAssert(TransformationManager::Instance && "Invalid Instance!");
  TransAssert(TransformationManager::Instance->ClangInstance &&
              "Invalid ClangInstance!");
  return (TransformationManager::Instance->ClangInstance->getLangOpts()
          .CPlusPlus);
}*/

bool TransformationManager::isCLangOpt() {
  TransAssert(TransformationManager::Instance && "Invalid Instance!");
  TransAssert(TransformationManager::Instance->ClangInstance &&
              "Invalid ClangInstance!");
  return (TransformationManager::Instance->ClangInstance->getLangOpts().C99);
}

/*bool TransformationManager::isOpenCLLangOpt()
{
  TransAssert(TransformationManager::Instance && "Invalid Instance!");
  TransAssert(TransformationManager::Instance->ClangInstance &&
              "Invalid ClangInstance!");
  return (TransformationManager::Instance->ClangInstance->getLangOpts()
          .OpenCL);
}*/

bool TransformationManager::initializeCompilerInstance(std::string &ErrorMsg) {
  /*if (ClangInstance) {
    ErrorMsg = "CompilerInstance has been initialized!";
    return false;
  }*/

  ClangInstance = new CompilerInstance();
  assert(ClangInstance);

  ClangInstance->createDiagnostics();

  TargetOptions &TargetOpts = ClangInstance->getTargetOpts();
  PreprocessorOptions &PPOpts = ClangInstance->getPreprocessorOpts();
  TargetOpts.Triple = LLVM_DEFAULT_TARGET_TRIPLE;
  llvm::Triple T(TargetOpts.Triple);
  CompilerInvocation &Invocation = ClangInstance->getInvocation();
  InputKind IK = FrontendOptions::getInputKindForExtension(
      StringRef(SrcFileName).rsplit('.').second);
  Invocation.setLangDefaults(ClangInstance->getLangOpts(), InputKind::C, T,
                             PPOpts);
  TargetInfo *Target =
      TargetInfo::CreateTargetInfo(ClangInstance->getDiagnostics(),
                                   ClangInstance->getInvocation().TargetOpts);
  ClangInstance->setTarget(Target);

  ClangInstance->createFileManager();
  ClangInstance->createSourceManager(ClangInstance->getFileManager());
  ClangInstance->createPreprocessor(TU_Complete);

  DiagnosticConsumer &DgClient = ClangInstance->getDiagnosticClient();
  DgClient.BeginSourceFile(ClangInstance->getLangOpts(),
                           &ClangInstance->getPreprocessor());
  ClangInstance->createASTContext();

  assert(CurrentTransformationImpl && "Bad transformation instance!");
  ClangInstance->setASTConsumer(
      std::unique_ptr<ASTConsumer>(CurrentTransformationImpl));
  Preprocessor &PP = ClangInstance->getPreprocessor();
  PP.getBuiltinInfo().initializeBuiltins(PP.getIdentifierTable(),
                                         PP.getLangOpts());

  if (!ClangInstance->InitializeSourceManager(
          FrontendInputFile(SrcFileName, IK))) {
    ErrorMsg = "Cannot open source file!";
    return false;
  }

  return true;
}

void TransformationManager::Finalize() {
  assert(TransformationManager::Instance);

  std::map<std::string, Transformation *>::iterator I, E;
  for (I = Instance->TransformationsMap.begin(),
      E = Instance->TransformationsMap.end();
       I != E; ++I) {
    // CurrentTransformationImpl will be freed by ClangInstance
    if ((*I).second != Instance->CurrentTransformationImpl)
      delete (*I).second;
  }
  if (Instance->TransformationsMapPtr)
    delete Instance->TransformationsMapPtr;

  delete Instance->ClangInstance;

  delete Instance;
  Instance = NULL;
}

llvm::raw_ostream *TransformationManager::getOutStream() {
  if (OutputFileName.empty())
    return &(llvm::outs());

  std::error_code EC;
  llvm::raw_fd_ostream *Out =
      new llvm::raw_fd_ostream(OutputFileName, EC, llvm::sys::fs::F_RW);
  assert(!EC && "Cannot open output file!");
  return Out;
}

void TransformationManager::closeOutStream(llvm::raw_ostream *OutStream) {
  if (!OutputFileName.empty())
    delete OutStream;
}

bool TransformationManager::doTransformation(std::string &ErrorMsg,
                                             int &ErrorCode) {
  ErrorMsg = "";

  ClangInstance->createSema(TU_Complete, 0);
  DiagnosticsEngine &Diag = ClangInstance->getDiagnostics();
  Diag.setSuppressAllDiagnostics(true);
  Diag.setIgnoreAllWarnings(true);

  CurrentTransformationImpl->setQueryInstanceFlag(QueryInstanceOnly);
  CurrentTransformationImpl->setTransformationCounter(TransformationCounter);
  if (ToCounter > 0) {
    if (CurrentTransformationImpl->isMultipleRewritesEnabled()) {
      CurrentTransformationImpl->setToCounter(ToCounter);
    } else {
      ErrorMsg = "current transformation[";
      ErrorMsg += CurrentTransName;
      ErrorMsg += "] does not support multiple rewrites!";
      return false;
    }
  }

  ParseAST(ClangInstance->getSema());

  ClangInstance->getDiagnosticClient().EndSourceFile();

  if (QueryInstanceOnly) {
    return true;
  }

  llvm::raw_ostream *OutStream = getOutStream();
  bool RV;
  if (CurrentTransformationImpl->transSuccess()) {
    CurrentTransformationImpl->outputTransformedSource(*OutStream);
    RV = true;
  } else if (CurrentTransformationImpl->transInternalError()) {
    CurrentTransformationImpl->outputOriginalSource(*OutStream);
    RV = true;
  } else {
    CurrentTransformationImpl->getTransErrorMsg(ErrorMsg);
    if (CurrentTransformationImpl->isInvalidCounterError())
      ErrorCode = ErrorInvalidCounter;
    RV = false;
  }
  closeOutStream(OutStream);
  return RV;
}

bool TransformationManager::verify(std::string &ErrorMsg, int &ErrorCode) {
  if (!CurrentTransformationImpl) {
    ErrorMsg = "Empty transformation instance!";
    return false;
  }

  if (CurrentTransformationImpl->skipCounter())
    return true;

  /*if (TransformationCounter <= 0) {
    ErrorMsg = "Invalid transformation counter!";
    ErrorCode = ErrorInvalidCounter;
    return false;
  }*/

  if ((ToCounter > 0) && (ToCounter < TransformationCounter)) {
    ErrorMsg = "to-counter value cannot be smaller than counter value!";
    ErrorCode = ErrorInvalidCounter;
    return false;
  }

  return true;
}

void TransformationManager::registerTransformation(const char *TransName,
                                                   Transformation *TransImpl) {
  if (!TransformationManager::TransformationsMapPtr) {
    TransformationManager::TransformationsMapPtr =
        new std::map<std::string, Transformation *>();
  }

  assert((TransImpl != NULL) && "NULL Transformation!");
  assert((TransformationManager::TransformationsMapPtr->find(TransName) ==
          TransformationManager::TransformationsMapPtr->end()) &&
         "Duplicated transformation!");
  (*TransformationManager::TransformationsMapPtr)[TransName] = TransImpl;
}

void TransformationManager::printTransformations() {
  llvm::outs() << "Registered Transformations:\n";

  std::map<std::string, Transformation *>::iterator I, E;
  for (I = TransformationsMap.begin(), E = TransformationsMap.end(); I != E;
       ++I) {
    llvm::outs() << "  [" << (*I).first << "]: ";
    llvm::outs() << (*I).second->getDescription() << "\n";
  }
}

void TransformationManager::printTransformationNames() {
  std::map<std::string, Transformation *>::iterator I, E;
  for (I = TransformationsMap.begin(), E = TransformationsMap.end(); I != E;
       ++I) {
    llvm::outs() << (*I).first << "\n";
  }
}

void TransformationManager::outputNumTransformationInstances() {
  int NumInstances = CurrentTransformationImpl->getNumTransformationInstances();
  llvm::outs() << "Available transformation instances: " << NumInstances
               << "\n";
}

TransformationManager::TransformationManager()
    : CurrentTransformationImpl(NULL), TransformationCounter(-1), ToCounter(-1),
      SrcFileName(""), OutputFileName(""), CurrentTransName(""),
      ClangInstance(NULL), QueryInstanceOnly(false)/*, DoReplacement(false),*/
      /*Replacement(""), CheckReference(false), ReferenceValue("")*/ {
  // Nothing to do
}

TransformationManager::~TransformationManager() {
  // Nothing to do
}
