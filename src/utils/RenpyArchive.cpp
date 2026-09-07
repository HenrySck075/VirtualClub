#include "RenpyArchive.hpp"
#include <optional>
#include <pybind11/embed.h>

namespace py = pybind11;

namespace ModsIndex {
  std::optional<py::module_> rpaReader = std::nullopt;
  std::optional<py::module_> rpycReader = std::nullopt;

  py::module_ rpaReaderModule() {return rpaReader.value();};
  py::module_ rpycReaderModule() {return rpycReader.value();};

  void loadArchiveReaderModules() {
    rpaReader = py::module_::create_extension_module("rpaReader", "very simple .rpa reader", new PyModuleDef{});

    py::exec(R"(
import pickle
import sys
import zlib
import os


def read_rpa_index(rpa_path):
    with open(rpa_path, "rb") as f:
        # Read the header line
        header = f.readline().decode('utf-8').strip()
        
        if not header.startswith("RPA-"):
            raise ValueError("Not a valid Ren'Py archive.")
        
        version = header.split("-")[1].split(" ")[0]
        
        if version == "3.0":
            # Extract the offset of the index table (in hex) and the obfuscation key
            _, offset_hex, key_hex = header.split(" ")
            offset = int(offset_hex, 16)
            key = int(key_hex, 16)
            
            # Jump straight to the index table
            f.seek(offset)
            compressed_index = f.read()
            
            # 1. Decompress and parse the pickle directly (it's not XORed!)
            raw_index = pickle.loads(zlib.decompress(compressed_index))
            
            # 2. De-obfuscate the individual inner values using the key
            index = {}
            for filename, entries in raw_index.items():
                index[filename] = []
                for entry in entries:
                    # RPAv3 entries can be 2 or 3 element tuples: (offset, length, [prefix])
                    #offset ^ key
                    if len(entry) == 3:
                        off, length, prefix = entry
                        index[filename].append((off ^ key, length ^ key, prefix))
                    else:
                        off, length = entry
                        index[filename].append((off ^ key, length ^ key, b""))
            
        elif version == "2.0":
            offset_hex = header.split(" ")[1]
            offset = int(offset_hex, 16)
            
            f.seek(offset)
            index = pickle.loads(f.read())
        else:
            raise NotImplementedError(f"Unsupported RPA version: {header}")
            
        return index

def extract_single_file(rpa_path, target_filename, index=None):
    if index is None:
        index = read_rpa_index(rpa_path)
        
    if target_filename not in index:
        raise FileNotFoundError(f"'{target_filename}' not found in archive.")
        
    # Grab the first match entry
    offset, length, _ = index[target_filename][0]
    
    with open(rpa_path, "rb") as src:
        src.seek(offset)
        return src.read(length)
    )", rpaReader->attr("__dict__"));


    rpycReader = py::module_::create_extension_module("rpycReader", "very simple .rpyc reader, simple to the point you have to analyze the ast yourself lmao", new PyModuleDef{});

    py::exec(R"(
import struct
import zlib
import pickle
import io
import sys
class PyExpr(str):
    "Mocks renpy.ast.PyExpr, which inherits from str/unicode."
    def __new__(cls, value, *args, **kwargs):
        # pickle passes the raw string value to the constructor first
        return super().__new__(cls, value)

    def __setstate__(self, state):
        # Store metadata dictionary if present, without losing the string value
        if isinstance(state, dict):
            self.__dict__.update(state)
# balls
def hash32(s):
    "Computes a deterministic 32-bit integer hash of a string, matching the native Cython implementation used by Ren'Py."
    # Force the input to a UTF-8 byte string if it's a standard Python string
    if isinstance(s, str):
        s = s.encode("utf-8")
        
    # Initialize the hash value (using FNV-1a or Jenkins initial style values)
    h = 0
    
    for byte in s:
        h += byte
        h = (h + (h << 10)) & 0xFFFFFFFF
        h ^= (h >> 6)
        
    # Final mixing steps to distribute the entropy cleanly across 32 bits
    h = (h + (h << 3)) & 0xFFFFFFFF
    h ^= (h >> 11)
    h = (h + (h << 15)) & 0xFFFFFFFF
    
    return h
class PyCode(object):
    def __setstate__(self, state):
        col_offset = 0; py = 2; hashcode = None;

        match state:
            case (_, source, location, mode, py, hashcode, col_offset): pass
            case (_, source, location, mode, py, hashcode): pass
            case (_, source, location, mode, py): pass
            case (_, source, location, mode): pass
            case _: raise Exception("Invalid state:", state)

        self.py = py
        self.col_offset = col_offset
        self.source = source
        self.filename = location[0]
        self.linenumber = location[1]
        self.mode = mode
        
        
        #if hashcode is None:
        #    if isinstance(source, PyExpr):
        #        hashcode = source.hashcode # type: ignore
        #    else: 
        #        hashcode = hash32(source)

        #self.hashcode = hashcode
        self.bytecode = None

    def __repr__(self):
        return f"<PyCode source={repr(self.source)}, filename={repr(self.filename)}, linenumber={self.linenumber}, mode={repr(self.mode)}, py={self.py}, col_offset={self.col_offset}>"

class MockRenpyObject:
    def __init__(self, *args, **kwargs):
        pass
    
    def __setstate__(self, state):
        if isinstance(state, dict):
            self.__dict__.update(state)
        elif isinstance(state, tuple):
            # Standard node fallback
            if len(state) == 2 and isinstance(state[1], dict):
                self.__dict__.update(state[1])
            # PyCode specific serialization tuple: (version, source, location, mode)
            elif len(state) == 4:
                self.source = state[1]
                self.location = state[2]
                self.mode = state[3]

    def __repr__(self):
        attrs = ", ".join(f"{k}={repr(v)}" for k, v in self.__dict__.items() if not k.startswith('_'))
        return f"<{self.__class__.__name__} {attrs}>"

class RenpyUnpickler(pickle.Unpickler):
    def find_class(self, module, name):
        if name == "PyExpr":
            return PyExpr
        if name == "PyCode":
            return PyCode
        if module.startswith("renpy") or name.startswith("renpy"):
            return type(name, (MockRenpyObject,), {})
        try:
            return super().find_class(module, name)
        except Exception:
            return type(name, (MockRenpyObject,), {})

def peek_rpyc_file(filepath):
    with open(filepath, "rb") as f:
        return peek_rpyc(f)

def peek_rpyc(f):
    if not isinstance(f, io.BytesIO):
        f = io.BytesIO(f)

    if f.read(10) != b"RENPY RPC2":
        raise ValueError("Unsupported or invalid .rpyc file header")
    
    slots = {}
    while True:
        slot_bytes = f.read(12)
        if len(slot_bytes) < 12: break
        slot, start, length = struct.unpack("<III", slot_bytes)
        if slot == 0: break
        slots[slot] = (start, length)
        
    if 1 not in slots: raise ValueError("Missing slot 1")
        
    start, length = slots[1]
    f.seek(start)
    decompressed_bytes = zlib.decompress(f.read(length))
    return RenpyUnpickler(io.BytesIO(decompressed_bytes)).load()

def walk_and_inspect(statements):
    "Recursively drills down into Init blocks to pull out variables."
    if not isinstance(statements, list):
        statements = [statements]
        
    one = True
    for stmt in statements:
        node_type = type(stmt).__name__

        print(stmt)
        
        # If it's an Init wrapper, dive into its internal statement block
        #if node_type == "Init" and hasattr(stmt, 'block'):
        #    walk_and_inspect(stmt.block)
            
        # Target your define / default values
        if node_type in ("Define", "Default"):
            varname = getattr(stmt, 'varname', 'Unknown')
            
            # Extract code source from PyCode object cleanly now
            code_obj = getattr(stmt, 'code', None)
            code_str = getattr(code_obj, 'source', str(code_obj))
            if one:
                print(code_str.__dict__)
                one = False
            
            print(f"[{node_type}] {varname} = {code_str}")
            
        elif node_type == "Python":
            code_obj = getattr(stmt, 'code', None)
            code_str = getattr(code_obj, 'source', str(code_obj))
            print(f"[Python] {code_str.strip()}")

def get_rpyc_statements(unpickled_data):
    if isinstance(unpickled_data, tuple) and len(unpickled_data) == 2:
        _, statements = unpickled_data
    else:
        statements = unpickled_data

    return statements if isinstance(statements, list) else [statements]

def lookup_defines(stmts, requested_defines):
    defines = {}

    for statement in stmts:
        node_type = type(statement).__name__
        if node_type == "Init" and hasattr(statement, 'block'):
            defines.update(lookup_defines(statement.block, requested_defines))
        elif node_type in ("Define", "Default"):
            store = getattr(statement, "store", "store")
            store = store.removeprefix("store.")
            if store == "store": store = ""

            if store in requested_defines:
                varname = getattr(statement, "varname", "")
                if varname in requested_defines[store]:
                    code_obj = getattr(statement, "code", None)
                    code_str = getattr(code_obj, "source", str(code_obj))

                    print(code_str)

                    defines[(store+"." if store != "" else "")+varname] = eval(code_str) # bit scary but what am i supposed to do
    return defines
    )", rpycReader->attr("__dict__"));
  }
}
