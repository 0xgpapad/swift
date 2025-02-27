///===--- DistributedActor.cpp - Distributed actor implementation ----------===///
///
/// This source file is part of the Swift.org open source project
///
/// Copyright (c) 2014 - 2021 Apple Inc. and the Swift project authors
/// Licensed under Apache License v2.0 with Runtime Library Exception
///
/// See https:///swift.org/LICENSE.txt for license information
/// See https:///swift.org/CONTRIBUTORS.txt for the list of Swift project authors
///
///===----------------------------------------------------------------------===///
///
/// The implementation of Swift distributed actors.
///
///===----------------------------------------------------------------------===///

#include "swift/ABI/Task.h"
#include "swift/ABI/Actor.h"
#include "swift/ABI/Metadata.h"
#include "swift/Runtime/AccessibleFunction.h"
#include "swift/Runtime/Concurrency.h"

using namespace swift;

static const AccessibleFunctionRecord *
findDistributedAccessor(const char *targetNameStart, size_t targetNameLength) {
  if (auto *func = runtime::swift_findAccessibleFunction(targetNameStart,
                                                         targetNameLength)) {
    assert(func->Flags.isDistributed());
    return func;
  }
  return nullptr;
}

SWIFT_CC(swift)
SWIFT_EXPORT_FROM(swift_Distributed)
void *swift_distributed_getGenericEnvironment(const char *targetNameStart,
                                              size_t targetNameLength) {
  auto *accessor = findDistributedAccessor(targetNameStart, targetNameLength);
  return accessor ? accessor->GenericEnvironment.get() : nullptr;
}

/// func _executeDistributedTarget(
///    on: AnyObject,
///    _ targetName: UnsafePointer<UInt8>,
///    _ targetNameLength: UInt,
///    argumentDecoder: AnyObject,
///    argumentTypes: UnsafeBufferPointer<Any.Type>,
///    resultBuffer: Builtin.RawPointer,
///    substitutions: UnsafeRawPointer?,
///    witnessTables: UnsafeRawPointer?,
///    numWitnessTables: UInt
/// ) async throws
using TargetExecutorSignature =
    AsyncSignature<void(/*on=*/DefaultActor *,
                        /*targetName=*/const char *, /*targetNameSize=*/size_t,
                        /*argumentDecoder=*/HeapObject *,
                        /*argumentTypes=*/const Metadata *const *,
                        /*resultBuffer=*/void *,
                        /*substitutions=*/void *,
                        /*witnessTables=*/void **,
                        /*numWitnessTables=*/size_t,
                        /*resumeFunc=*/TaskContinuationFunction *,
                        /*callContext=*/AsyncContext *),
                   /*throws=*/true>;

SWIFT_CC(swiftasync)
SWIFT_EXPORT_FROM(swift_Distributed)
TargetExecutorSignature::FunctionType swift_distributed_execute_target;

/// Accessor takes:
///   - an async context
///   - an argument decoder as an instance of type conforming to `InvocationDecoder`
///   - a list of all argument types (with substitutions applied)
///   - a result buffer as a raw pointer
///   - a list of substitutions
///   - a list of witness tables
///   - a number of witness tables in the buffer
///   - a reference to an actor to execute method on.
using DistributedAccessorSignature =
    AsyncSignature<void(/*argumentDecoder=*/HeapObject *,
                        /*argumentTypes=*/const Metadata *const *,
                        /*resultBuffer=*/void *,
                        /*substitutions=*/void *,
                        /*witnessTables=*/void **,
                        /*numWitnessTables=*/size_t,
                        /*actor=*/HeapObject *),
                   /*throws=*/true>;

SWIFT_CC(swiftasync)
static DistributedAccessorSignature::ContinuationType
    swift_distributed_execute_target_resume;

SWIFT_CC(swiftasync)
static void ::swift_distributed_execute_target_resume(
    SWIFT_ASYNC_CONTEXT AsyncContext *context,
    SWIFT_CONTEXT SwiftError *error) {
  auto parentCtx = context->Parent;
  auto resumeInParent =
      reinterpret_cast<TargetExecutorSignature::ContinuationType *>(
          parentCtx->ResumeParent);
  swift_task_dealloc(context);
  // See `swift_distributed_execute_target` - `parentCtx` in this case
  // is `callContext` which should be completely transparent on resume.
  return resumeInParent(parentCtx->Parent, error);
}

SWIFT_CC(swiftasync)
void ::swift_distributed_execute_target(
    SWIFT_ASYNC_CONTEXT AsyncContext *callerContext,
    DefaultActor *actor,
    const char *targetNameStart, size_t targetNameLength,
    HeapObject *argumentDecoder,
    const Metadata *const *argumentTypes,
    void *resultBuffer,
    void *substitutions,
    void **witnessTables,
    size_t numWitnessTables,
    TaskContinuationFunction *resumeFunc,
    AsyncContext *callContext) {
  auto *accessor = findDistributedAccessor(targetNameStart, targetNameLength);
  if (!accessor) {
    assert(false && "no distributed accessor");
    return; // FIXME(distributed): return -1 here so the lib can fail the call
  }

  auto *asyncFnPtr = reinterpret_cast<
      const AsyncFunctionPointer<DistributedAccessorSignature> *>(
      accessor->Function.get());
  assert(asyncFnPtr && "no function pointer for distributed_execute_target");

  DistributedAccessorSignature::FunctionType *accessorEntry =
      asyncFnPtr->Function.get();

  AsyncContext *calleeContext = reinterpret_cast<AsyncContext *>(
      swift_task_alloc(asyncFnPtr->ExpectedContextSize));

  // TODO(concurrency): Special functions like this one are currently set-up
  // to pass "caller" context and resume function as extra parameters due to
  // how they are declared in C. But this particular function behaves exactly
  // like a regular `async throws`, which means that we need to initialize
  // intermediate `callContext` using parent `callerContext`. A better fix for
  // this situation would be to adjust IRGen and handle function like this
  // like regular `async` functions even though they are classified as special.
  callContext->Parent = callerContext;
  callContext->ResumeParent = resumeFunc;

  calleeContext->Parent = callContext;
  calleeContext->ResumeParent = reinterpret_cast<TaskContinuationFunction *>(
      swift_distributed_execute_target_resume);

  accessorEntry(calleeContext,
                argumentDecoder, argumentTypes,
                resultBuffer,
                substitutions,
                witnessTables,
                numWitnessTables,
                actor);
}
