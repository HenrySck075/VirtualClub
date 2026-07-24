import pickle
import sys
import zlib

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
    if not index:
        index = read_rpa_index(rpa_path)
        
    if target_filename not in index:
        raise FileNotFoundError(f"'{target_filename}' not found in archive.")
        
    # Grab the first match entry
    offset, length, _ = index[target_filename][0]
    
    with open(rpa_path, "rb") as src:
        src.seek(offset)
        return src.read(length)

# --- Example Usage ---
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python rpa_reader.py <path_to_archive.rpa> [<files_to_extract>...]")
        sys.exit(1)
        
    rpa_file = sys.argv[1]

    extracts = sys.argv[2:]

    archive_index = read_rpa_index(rpa_file)

    if len(extracts) != 0:
        for filename in extracts:
            try:
                data = extract_single_file(rpa_file, filename, archive_index)
                with open(filename, "wb") as out_file:
                    out_file.write(data)
                print(f"Extracted: {filename}")
            except FileNotFoundError:
                print(f"File not found in archive: {filename}")
    else:
        print("Files available in archive:")
        for path in list(archive_index.keys()): # print first 5 files safely
            print(f" - {path}")
