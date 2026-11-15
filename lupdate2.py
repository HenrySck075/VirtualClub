#!/usr/bin/env python3
"""
Flexible Qt lupdate-compatible CLI tool for QWidgets C++ projects using libclang.
Supports JSON-based project files with subProjects, include paths, custom function aliases,
robust .ts translation merging, accurate C++ AST context scoping, and flexible -locations modes.
"""

import sys
import os
import re
import json
import argparse
import xml.etree.ElementTree as ET
import xml.dom.minidom
from pathlib import Path

try:
    import clang.cindex
except ImportError:
    print("Error: 'clang' Python package is required. Install it via 'pip install clang'.", file=sys.stderr)
    sys.exit(1)

# Default Qt translation functions and their argument positions (1-indexed)
DEFAULT_ALIASES = {
    'tr': {'text_pos': 1, 'context_pos': None},
    'trUtf8': {'text_pos': 1, 'context_pos': None},
    'QT_TR_NOOP': {'text_pos': 1, 'context_pos': None},
    'QT_TR_NOOP_UTF8': {'text_pos': 1, 'context_pos': None},
    'QT_TRANSLATE_NOOP': {'text_pos': 2, 'context_pos': 1},
    'QT_TRANSLATE_NOOP_UTF8': {'text_pos': 2, 'context_pos': 1},
    'translate': {'text_pos': 2, 'context_pos': 1},
    'qsTr': {'text_pos': 1, 'context_pos': None},
    'qsTranslate': {'text_pos': 2, 'context_pos': 1},
}


def extract_string_from_cursor(cursor):
    """Extracts and unescapes string literals from a clang expression cursor using tokenization."""
    tokens = list(cursor.get_tokens())
    for t in tokens:
        if t.kind == clang.cindex.TokenKind.LITERAL:
            spelling = t.spelling
            matches = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', spelling)
            if matches:
                full_str = "".join(matches)
                try:
                    return full_str.encode().decode('unicode-escape')
                except Exception:
                    return full_str
    return None


def get_enclosing_context(cursor):
    """Walks up semantic parents to find the enclosing C++ class or struct name for context scoping."""
    curr = cursor.semantic_parent
    while curr and curr.kind != clang.cindex.CursorKind.TRANSLATION_UNIT:
        if curr.kind in (clang.cindex.CursorKind.CLASS_DECL, clang.cindex.CursorKind.STRUCT_DECL):
            if curr.spelling:
                return curr.spelling
        curr = curr.semantic_parent
    return "@default"


def parse_cpp_file_clang(filepath, aliases, include_paths=None):
    """Scans a C++ source/header file using libclang AST for translation calls."""
    messages = []
    filepath = Path(filepath)
    
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception as e:
        print(f"Warning: Could not open {filepath}: {e}", file=sys.stderr)
        return messages

    # Fast pre-check: skip full AST parsing if none of the alias identifiers appear in the file text
    alias_names = set(aliases.keys())
    if not any(alias in content for alias in alias_names):
        return messages

    try:
        index = clang.cindex.Index.create()
        args = ['-std=c++20', '-x', 'c++']
        if include_paths:
            for inc in include_paths:
                args.append(f'-I{inc}')
        
        tu = index.parse(str(filepath), args=args)
    except Exception as e:
        print(f"Warning: libclang failed to parse {filepath}: {e}", file=sys.stderr)
        return messages

    for cursor in tu.cursor.walk_preorder():
        if cursor.kind == clang.cindex.CursorKind.CALL_EXPR:
            func_name = cursor.spelling
            if not func_name:
                func_name = cursor.displayname.split('(')[0].strip()

            if func_name in aliases:
                rule = aliases[func_name]
                args_list = list(cursor.get_arguments())

                text_pos = rule['text_pos'] - 1
                context_pos = rule['context_pos'] - 1 if rule['context_pos'] is not None else None

                if len(args_list) > text_pos:
                    source_text = extract_string_from_cursor(args_list[text_pos])
                    if source_text:
                        msg_context = get_enclosing_context(cursor)
                        if context_pos is not None and len(args_list) > context_pos:
                            ctx_text = extract_string_from_cursor(args_list[context_pos])
                            if ctx_text:
                                msg_context = ctx_text

                        location = cursor.location
                        line_no = location.line if location and location.line else 0
                        file_path = location.file.name if location and location.file else str(filepath)

                        messages.append({
                            'context': msg_context,
                            'source': source_text,
                            'comment': '',
                            'file': file_path,
                            'line': line_no
                        })

    return messages


def parse_ui_file(filepath):
    """Parses a Qt Designer .ui XML file for translatable strings."""
    messages = []
    try:
        tree = ET.parse(filepath)
        root = tree.getroot()
        
        class_elem = root.find('.//class')
        context = class_elem.text if class_elem is not None and class_elem.text else Path(filepath).stem
        
        for elem in root.iter():
            if elem.tag == 'string':
                if elem.attrib.get('notr') == 'true':
                    continue
                text = elem.text
                if text and text.strip():
                    comment = elem.attrib.get('comment', '')
                    messages.append({
                        'context': context,
                        'source': text,
                        'comment': comment,
                        'file': str(filepath),
                        'line': 0
                    })
    except Exception as e:
        print(f"Error parsing UI file {filepath}: {e}", file=sys.stderr)
    return messages


def categorize_file(filepath, sources, headers, forms):
    """Categorizes a file into sources, headers, or UI forms based on its extension."""
    path = Path(filepath)
    ext = path.suffix.lower()
    if ext in ('.cpp', '.cc', '.cxx', '.c'):
        sources.append(str(path.resolve()))
    elif ext in ('.h', '.hpp', '.hh'):
        headers.append(str(path.resolve()))
    elif ext == '.ui':
        forms.append(str(path.resolve()))


def parse_json_project_file(json_path):
    """Parses the JSON project file, extracting sources, translations, subProjects, and include paths."""
    json_path = Path(json_path)
    json_dir = json_path.parent
    sources, headers, forms, translations, include_paths = [], [], [], [], []
    
    if not json_path.exists():
        print(f"Error: JSON project file {json_path} not found.", file=sys.stderr)
        return sources, headers, forms, translations, include_paths

    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except Exception as e:
        print(f"Error reading/parsing JSON project file {json_path}: {e}", file=sys.stderr)
        return sources, headers, forms, translations, include_paths

    for t in data.get('translations', []):
        translations.append(t)

    def resolve_path(p_str):
        p = Path(p_str)
        if not p.is_absolute():
            p = json_dir / p
        return str(p)

    for inc in data.get('includePaths', []):
        include_paths.append(resolve_path(inc))

    for s in data.get('sources', []):
        categorize_file(resolve_path(s), sources, headers, forms)

    for sub in data.get('subProjects', []):
        excluded_set = set(sub.get('excluded', []))
        for s in sub.get('sources', []):
            if s in excluded_set or Path(s).name in excluded_set:
                continue
            
            resolved_s = resolve_path(s)
            if any(Path(resolved_s) == Path(resolve_path(ex)) for ex in excluded_set):
                continue
                
            categorize_file(resolved_s, sources, headers, forms)

    # Automatically add project root and parent directories to include paths for libclang header resolution
    include_paths.append(str(json_dir.resolve()))
    return sources, headers, forms, translations, include_paths


def update_ts_file(ts_path, new_messages, no_obsolete=False, locations_mode='absolute'):
    """Updates or creates a Qt XML TS translation file, handling merging, obsolete items, and location formatting."""
    ts_path = Path(ts_path)
    existing_messages = {}

    if ts_path.exists():
        try:
            tree = ET.parse(ts_path)
            root = tree.getroot()
            for context_elem in root.findall('context'):
                name_elem = context_elem.find('name')
                if name_elem is not None:
                    ctx_name = name_elem.text
                    for msg_elem in context_elem.findall('message'):
                        src_elem = msg_elem.find('source')
                        if src_elem is not None and src_elem.text is not None:
                            src_text = src_elem.text
                            trans_elem = msg_elem.find('translation')
                            comm_elem = msg_elem.find('comment')
                            
                            locations = []
                            for loc_elem in msg_elem.findall('location'):
                                locations.append({
                                    'file': loc_elem.get('filename'),
                                    'line': loc_elem.get('line')
                                })

                            trans_attrib = dict(trans_elem.attrib) if trans_elem is not None else {'type': 'unfinished'}
                            trans_text = trans_elem.text if trans_elem is not None else None
                            is_obsolete = trans_attrib.get('type') == 'obsolete'

                            existing_messages[(ctx_name, src_text)] = {
                                'context': ctx_name,
                                'source': src_text,
                                'translation': trans_text,
                                'attrib': trans_attrib,
                                'comment': comm_elem.text if comm_elem is not None else '',
                                'locations': locations,
                                'obsolete': is_obsolete
                            }
        except Exception as e:
            print(f"Warning: Could not parse existing TS file {ts_path}: {e}", file=sys.stderr)

    new_msgs_map = {}
    for msg in new_messages:
        key = (msg['context'], msg['source'])
        if key not in new_msgs_map:
            new_msgs_map[key] = {
                'context': msg['context'],
                'source': msg['source'],
                'comment': msg['comment'],
                'locations': []
            }
        loc_entry = {'file': msg['file'], 'line': str(msg['line']) if msg['line'] > 0 else None}
        if loc_entry not in new_msgs_map[key]['locations']:
            new_msgs_map[key]['locations'].append(loc_entry)
        if msg['comment'] and not new_msgs_map[key]['comment']:
            new_msgs_map[key]['comment'] = msg['comment']

    final_messages_by_context = {}

    for key, new_data in new_msgs_map.items():
        ctx = new_data['context']
        src = new_data['source']
        final_messages_by_context.setdefault(ctx, {})

        if key in existing_messages:
            old_data = existing_messages[key]
            final_messages_by_context[ctx][src] = {
                'source': src,
                'translation': old_data['translation'],
                'attrib': old_data['attrib'],
                'comment': new_data['comment'] or old_data['comment'],
                'locations': new_data['locations'],
                'obsolete': False
            }
        else:
            final_messages_by_context[ctx][src] = {
                'source': src,
                'translation': None,
                'attrib': {'type': 'unfinished'},
                'comment': new_data['comment'],
                'locations': new_data['locations'],
                'obsolete': False
            }

    if not no_obsolete:
        for key, old_data in existing_messages.items():
            ctx = old_data['context']
            src = old_data['source']
            if ctx not in final_messages_by_context or src not in final_messages_by_context[ctx]:
                final_messages_by_context.setdefault(ctx, {})
                attrib = dict(old_data['attrib'])
                attrib['type'] = 'obsolete'
                final_messages_by_context[ctx][src] = {
                    'source': src,
                    'translation': old_data['translation'],
                    'attrib': attrib,
                    'comment': old_data['comment'],
                    'locations': old_data['locations'],
                    'obsolete': True
                }

    ts_root = ET.Element('TS', {'version': '2.1'})

    for ctx_name in sorted(final_messages_by_context.keys()):
        ctx_elem = ET.SubElement(ts_root, 'context')
        name_elem = ET.SubElement(ctx_elem, 'name')
        name_elem.text = ctx_name

        msgs_dict = final_messages_by_context[ctx_name]
        for src_text in sorted(msgs_dict.keys()):
            msg_data = msgs_dict[src_text]
            msg_elem = ET.SubElement(ctx_elem, 'message')

            if locations_mode != 'none':
                for loc in msg_data['locations']:
                    loc_file = loc['file']
                    if locations_mode == 'relative' and loc_file:
                        try:
                            loc_file = os.path.relpath(loc_file, ts_path.parent)
                        except Exception:
                            pass
                    loc_elem = ET.SubElement(msg_elem, 'location')
                    loc_elem.set('filename', loc_file)
                    if loc['line']:
                        loc_elem.set('line', loc['line'])

            src_elem = ET.SubElement(msg_elem, 'source')
            src_elem.text = src_text

            if msg_data['comment']:
                comm_elem = ET.SubElement(msg_elem, 'comment')
                comm_elem.text = msg_data['comment']

            trans_elem = ET.SubElement(msg_elem, 'translation')
            if msg_data['translation'] is not None:
                trans_elem.text = msg_data['translation']
            
            for k, v in msg_data['attrib'].items():
                trans_elem.set(k, v)

    xml_str = ET.tostring(ts_root, encoding='utf-8')
    header = b'<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n'
    
    dom = xml.dom.minidom.parseString(xml_str)
    pretty_xml = dom.toprettyxml(indent='    ', encoding='utf-8')
    lines_out = [line for line in pretty_xml.splitlines() if line.strip()]
    final_output = header + b'\n'.join(lines_out[1:])

    with open(ts_path, 'wb') as f:
        f.write(final_output)
    print(f"Updated translation file: {ts_path}")


def main():
    parser = argparse.ArgumentParser(description="Flexible Qt lupdate CLI replacement with libclang AST parsing & JSON project support.")
    parser.add_argument('-project', help="Path to JSON project file.")
    parser.add_argument('-ts', nargs='+', help="Output translation (.ts) files override.")
    parser.add_argument('-no-obsolete', action='store_true', help="Drop obsolete messages.")
    parser.add_argument('-locations', choices=['absolute', 'relative', 'none'], default='absolute', help="Specify or override how source code references are saved in TS files.")
    parser.add_argument('--alias', action='append', help="Define custom alias rule in format FunctionName=TextArgPosition (e.g., myTr=1)")
    parser.add_argument('sources', nargs='*', help="Source, header, or UI files to process directly.")

    args = parser.parse_args()

    aliases = DEFAULT_ALIASES.copy()

    if args.alias:
        for alias_spec in args.alias:
            if '=' in alias_spec:
                name, pos_str = alias_spec.split('=', 1)
                try:
                    pos = int(pos_str)
                    ctx_pos = 1 if pos > 1 else None
                    aliases[name.strip()] = {'text_pos': pos, 'context_pos': ctx_pos}
                except ValueError:
                    print(f"Error: Invalid position in alias specification '{alias_spec}'. Expected integer.", file=sys.stderr)
                    sys.exit(1)

    sources = []
    headers = []
    forms = []
    translations = []
    include_paths = []

    if args.project:
        p_sources, p_headers, p_forms, p_translations, p_include_paths = parse_json_project_file(args.project)
        sources.extend(p_sources)
        headers.extend(p_headers)
        forms.extend(p_forms)
        translations.extend(p_translations)
        include_paths.extend(p_include_paths)

    for src in args.sources:
        categorize_file(src, sources, headers, forms)

    if args.ts:
        translations = args.ts

    if not translations:
        print("Error: No target translation (.ts) files specified (use -ts or define translations in JSON project file).", file=sys.stderr)
        sys.exit(1)

    all_files_to_parse = list(set(sources + headers))
    all_ui_files = list(set(forms))

    print(f"Parsing {len(all_files_to_parse)} C++ source/header file(s) via libclang...")
    print(f"Parsing {len(all_ui_files)} UI file(s)...")

    extracted_messages = []

    for filepath in all_files_to_parse:
        extracted_messages.extend(parse_cpp_file_clang(filepath, aliases, include_paths))

    for filepath in all_ui_files:
        extracted_messages.extend(parse_ui_file(filepath))

    print(f"Total extracted translatable strings: {len(extracted_messages)}")

    for ts_file in translations:
        update_ts_file(ts_file, extracted_messages, no_obsolete=args.no_obsolete, locations_mode=args.locations)


if __name__ == '__main__':
    main()
