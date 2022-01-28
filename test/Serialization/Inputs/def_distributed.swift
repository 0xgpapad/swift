import _Distributed

/// Use the existential wrapper as the default actor system.
typealias DefaultDistributedActorSystem = FakeActorSystem

@available(SwiftStdlib 5.6, *)
public distributed actor DA {
  public distributed func doSomethingDistributed(param: String) async -> Int {
    return 0
  }
}

// ==== Fake Address -----------------------------------------------------------

public struct ActorAddress: Hashable, Sendable, Codable {
  public let address: String
  public init(parse address : String) {
    self.address = address
  }

  public init(from decoder: Decoder) throws {
    let container = try decoder.singleValueContainer()
    self.address = try container.decode(String.self)
  }

  public func encode(to encoder: Encoder) throws {
    var container = encoder.singleValueContainer()
    try container.encode(self.address)
  }
}

// ==== Fake Transport ---------------------------------------------------------

public struct FakeActorSystem: DistributedActorSystem {
  public typealias ActorID = ActorAddress
  public typealias InvocationDecoder = FakeInvocation
  public typealias InvocationEncoder = FakeInvocation
  public typealias SerializationRequirement = Codable

  init() {
    print("Initialized new FakeActorSystem")
  }

  public func resolve<Act>(id: ActorID, as actorType: Act.Type) throws -> Act?
      where Act: DistributedActor,
      Act.ID == ActorID  {
    nil
  }

  public func assignID<Act>(_ actorType: Act.Type) -> ActorID
      where Act: DistributedActor,
      Act.ID == ActorID {
    ActorAddress(parse: "xxx")
  }

  public func actorReady<Act>(_ actor: Act)
      where Act: DistributedActor,
      Act.ID == ActorID {
  }

  public func resignID(_ id: ActorID) {
  }

  public func makeInvocationEncoder() -> InvocationEncoder {
    .init()
  }

  func remoteCall<Act, Err, Res>(
    on actor: Act,
    target: RemoteCallTarget,
    invocationDecoder: inout InvocationDecoder,
    throwing: Err.Type,
    returning: Res.Type
  ) async throws -> Res
    where Act: DistributedActor,
    Err: Error,
//          Act.ID == ActorID,
    Res: SerializationRequirement {
    print("remoteCall: on:\(actor), target:\(target), invocation:\(invocationDecoder), throwing:\(throwing), returning:\(returning)")
    return "<REMOTE CALL>" as! Res
  }

  func remoteCallVoid<Act, Err>(
    on actor: Act,
    target: RemoteCallTarget,
    invocationDecoder: inout InvocationDecoder,
    throwing: Err.Type
  ) async throws
    where Act: DistributedActor,
    Err: Error
//          Act.ID == ActorID
  {
    print("remoteCallVoid: on:\(actor), target:\(target), invocation:\(invocationDecoder), throwing:\(throwing)")
    return ()
  }
}

public struct FakeInvocation: DistributedTargetInvocationEncoder, DistributedTargetInvocationDecoder {
  public typealias SerializationRequirement = Codable

  public mutating func recordGenericSubstitution<T>(_ type: T.Type) throws {}
  public mutating func recordArgument<Argument: SerializationRequirement>(_ argument: Argument) throws {}
  public mutating func recordReturnType<R: SerializationRequirement>(_ type: R.Type) throws {}
  public mutating func recordErrorType<E: Error>(_ type: E.Type) throws {}
  public mutating func doneRecording() throws {}

  // === Receiving / decoding -------------------------------------------------

  public func decodeGenericSubstitutions() throws -> [Any.Type] { [] }
  public mutating func decodeNextArgument<Argument>(
    _ argumentType: Argument.Type,
    into pointer: UnsafeMutablePointer<Argument> // pointer to our hbuffer
  ) throws { /* ... */ }
  public func decodeReturnType() throws -> Any.Type? { nil }
  public func decodeErrorType() throws -> Any.Type? { nil }

  public struct FakeArgumentDecoder: DistributedTargetInvocationArgumentDecoder {
    public typealias SerializationRequirement = Codable
  }
}

