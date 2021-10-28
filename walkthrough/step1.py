from wasmer import engine, Store, ImportObject, Function, Module, Instance, Memory, MemoryType
from wasmer_compiler_cranelift import Compiler

# The store represents all global state that can be
# minipulated by WebAssembly programs. The store
# holds the engine, which is responsible for compiling
# the module into something that can be executed.
#
# https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.Store
store = Store(engine.JIT(Compiler))

# Define the memory to be imported into the module.
# minimum=1 starts the memory at one page (64Kb).
# You can optionally provide a maximum to limit memory use.
# shared=False does not allow the memory to be accessed between threads.
#
# https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.MemoryType
memory_type = MemoryType(minimum=1, shared=False)

# Define the memory instance, which consists of a vector of bytes.
#
# https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.Memory
memory = Memory(store, memory_type)

# Compile the module to be able to execute it.
#
# https://wasmerio.github.io/wasmer-python/api/wasmer/#wasmer.Module
module = Module(store, open("./hello-world-memory-import.wasm", "rb").read())

print("Successfully loaded WASM module!")