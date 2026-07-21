import struct
import zlib
import pickle
import io
import sys
class PyExpr(str):
    """Mocks renpy.ast.PyExpr, which inherits from str/unicode."""
    def __new__(cls, value, *args, **kwargs):
        # pickle passes the raw string value to the constructor first
        return super().__new__(cls, value)

    def __setstate__(self, state):
        # Store metadata dictionary if present, without losing the string value
        if isinstance(state, dict):
            self.__dict__.update(state)
# balls
def hash32(s):
    """
    Computes a deterministic 32-bit integer hash of a string,
    matching the native Cython implementation used by Ren'Py.
    """
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
        
        """
        if hashcode is None:
            if isinstance(source, PyExpr):
                hashcode = source.hashcode # type: ignore
            else: 
                hashcode = hash32(source)

        self.hashcode = hashcode
        """
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
    """Recursively drills down into Init blocks to pull out variables."""
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

if __name__ == "__main__":
    rpyc_file = sys.argv[1]
    
    try:
        unpickled_data = peek_rpyc_file(rpyc_file)
        
        if isinstance(unpickled_data, tuple) and len(unpickled_data) == 2:
            _, statements = unpickled_data
        else:
            statements = unpickled_data
            
        walk_and_inspect(statements)
                
    except FileNotFoundError:
        print(f"Error: '{rpyc_file}' not found.")
