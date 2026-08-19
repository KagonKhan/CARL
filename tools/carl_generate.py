#!/usr/bin/env python3
"""Generate a CARL config model from an annotated YAML file.

The input is one complete config. Structure comes from the YAML itself; anything the shape cannot
express is declared inline:

    port: 52000                        # required int
    maxEncoders: 8         # !optional  -> Required::NO
    maxDuration: 10        # !default   -> Default<int>{10}
    transformationMatrix: !Matrix3 []  -> ConfigValue<Matrix3>, you supply YAML::convert<Matrix3>
    recognizers:           # !group    -> force a group of nested groups
    cameras:               # !map      -> force a keyed ConfigMap

Directives are written !word so prose is never mistaken for one, and are read from a trailing
comment on the key's line or from a standalone comment line directly above it.
"""

import argparse
import re
import sys
from pathlib import Path

import yaml

INT_RE   = re.compile(r'^[+-]?\d+$')
FLOAT_RE = re.compile(r'^[+-]?(\d+\.\d*|\.\d+|\d+)([eE][+-]?\d+)?$')
BOOL_STR = {'true', 'false'}

DIRECTIVES = {'optional', 'default', 'group', 'map', 'list'}

# yaml-cpp and the standard library already satisfy CARL for these, so a !tag naming one is a plain
# type override and needs no static_assert or user-supplied convert
BUILTIN_TYPES = {
    'bool', 'char', 'short', 'int', 'long', 'long long', 'unsigned', 'unsigned char',
    'unsigned short', 'unsigned int', 'unsigned long', 'unsigned long long',
    'float', 'double', 'long double', 'std::string', 'std::size_t',
}


# --------------------------------------------------------------------------- annotations

def read_directives(text):
    """line index -> (directives on that line, whether the line is only a comment)

    A directive must be written !word, so ordinary prose in a comment is never mistaken for one.
    """
    found = {}
    for i, line in enumerate(text.splitlines()):
        hash_at = find_comment(line)
        if hash_at is None:
            continue
        words = re.split(r'[\s,]+', line[hash_at + 1:].strip())
        hits = {w[1:].lower() for w in words if w.startswith('!') and w[1:].lower() in DIRECTIVES}
        if hits:
            found[i] = (hits, line[:hash_at].strip() == '')
    return found


def find_comment(line):
    """index of the comment '#', ignoring '#' inside quotes"""
    quote = None
    for i, ch in enumerate(line):
        if quote:
            if ch == quote:
                quote = None
        elif ch in '"\'':
            quote = ch
        elif ch == '#':
            if i == 0 or line[i - 1] in ' \t':
                return i
    return None


def directives_for(key_node, table):
    """this key's own trailing comment, plus a standalone comment line directly above it"""
    line = key_node.start_mark.line

    own, _ = table.get(line, (set(), False))
    above, above_is_standalone = table.get(line - 1, (set(), False))
    return own | (above if above_is_standalone else set())


# --------------------------------------------------------------------------- naming

def pascal(name):
    parts = re.split(r'[_\-]', name)
    return ''.join(p[:1].upper() + p[1:] for p in parts if p)


def singular(name):
    for suffix, repl in (('ies', 'y'), ('sses', 'ss'), ('s', '')):
        if name.endswith(suffix) and len(name) > len(suffix):
            return name[: -len(suffix)] + repl
    return name


def scalar_cpp_type(values):
    """widen numerics, fall back to string"""
    if not values:
        return 'std::string'
    if all(v.lower() in BOOL_STR for v in values):
        return 'bool'
    if all(INT_RE.match(v) for v in values):
        return 'int'
    if all(FLOAT_RE.match(v) for v in values):
        return 'double'
    return 'std::string'


def cpp_literal(cpp_type, raw):
    if cpp_type == 'bool':
        return 'true' if raw.lower() == 'true' else 'false'
    if cpp_type == 'std::string':
        return 'std::string{"%s"}' % raw.replace('\\', '\\\\').replace('"', '\\"')
    if cpp_type == 'double' and INT_RE.match(raw):
        return raw + '.0'
    return raw


# --------------------------------------------------------------------------- IR

class Field:
    """a ConfigValue<T> member"""

    def __init__(self, key, cpp_type, required=True, default=None, tagged=False):
        self.key, self.cpp_type = key, cpp_type
        self.required, self.default, self.tagged = required, default, tagged


class Group:
    """a ConfigGroup: a named section, or a nameless map entry / root"""

    def __init__(self, key, type_name, children, required=True, named=True):
        self.key, self.type_name, self.children = key, type_name, children
        self.required, self.named = required, named


class Map:
    """a ConfigMap<Entry, Key>"""

    def __init__(self, key, entry, key_type, mode, required=True):
        self.key, self.entry, self.key_type, self.mode = key, entry, key_type, mode
        self.required = required


class Struct:
    """a plain struct emitted into the extensions header, with convert + operator<<"""

    def __init__(self, name, members, kind, item_type=None):
        self.name, self.members, self.kind = name, members, kind
        self.item_type = item_type


# --------------------------------------------------------------------------- YAML node helpers

def is_map(node):
    return isinstance(node, yaml.MappingNode)


def is_seq(node):
    return isinstance(node, yaml.SequenceNode)


def is_scalar(node):
    return isinstance(node, yaml.ScalarNode)


def explicit_tag(node):
    """the C++ type named by an explicit !tag, or None"""
    tag = node.tag
    if tag.startswith('tag:yaml.org,2002:') or tag == '!':
        return None
    return tag.lstrip('!')


def map_keys(node):
    return [k.value for k, _ in node.value]


def looks_like_keyed_map(node):
    """every value a mapping, at least two of them, and their key sets broadly agree"""
    values = [v for _, v in node.value]
    if len(values) < 2 or not all(is_map(v) for v in values):
        return False
    sets = [set(map_keys(v)) for v in values]
    union = set().union(*sets)
    inter = set(sets[0]).intersection(*sets[1:])
    return bool(union) and len(inter) / len(union) >= 0.5


def merged_entry_keys(nodes):
    """union of keys across sibling mappings, first-seen order, remembering which are universal"""
    order, seen_in = [], {}
    for n in nodes:
        for k, v in n.value:
            if k.value not in seen_in:
                order.append((k, v))
                seen_in[k.value] = 0
            seen_in[k.value] += 1
    universal = {k: c == len(nodes) for k, c in seen_in.items()}
    return order, universal


# --------------------------------------------------------------------------- builder

class Builder:
    def __init__(self, directives, root_name):
        self.directives = directives
        self.root_name = root_name
        self.structs = []          # generated plain structs, in dependency order
        self.tagged_types = []     # types named by !tags, which the user must provide
        self.notes = []

    # -- entry points ------------------------------------------------------

    def build_root(self, node):
        children = [self.child(k, v) for k, v in node.value]
        return Group('', self.root_name, children, named=False)

    def child(self, key_node, value_node):
        key = key_node.value
        flags = directives_for(key_node, self.directives)
        required = 'optional' not in flags and 'default' not in flags
        tag = explicit_tag(value_node)

        if tag:
            if tag in BUILTIN_TYPES:
                # plain type override, e.g. radius: !double 2
                default = None
                if 'default' in flags and is_scalar(value_node):
                    default = cpp_literal(tag, value_node.value)
                return Field(key, tag, required, default)

            self.tagged_types.append(tag)
            if 'default' in flags:
                self.notes.append("%s: !default ignored, %s is a user supplied type" % (key, tag))
            return Field(key, tag, required, None, tagged=True)

        if is_scalar(value_node):
            cpp = scalar_cpp_type([value_node.value])
            default = cpp_literal(cpp, value_node.value) if 'default' in flags else None
            return Field(key, cpp, required, default)

        if is_map(value_node):
            if 'group' not in flags and ('map' in flags or looks_like_keyed_map(value_node)):
                return self.keyed_map(key, value_node, required)
            return self.group(key, value_node, required)

        if is_seq(value_node):
            return self.sequence(key, value_node, required, flags)

        raise SystemExit('unsupported node for key %r' % key)

    # -- shapes ------------------------------------------------------------

    def group(self, key, node, required):
        children = [self.child(k, v) for k, v in node.value]
        return Group(key, pascal(key) + 'Config', children, required)

    def keyed_map(self, key, node, required):
        entries = [v for _, v in node.value]
        keys = map_keys(node)
        key_type = 'int' if all(INT_RE.match(k) for k in keys) else 'std::string'
        entry = self.entry_group(key, entries)
        return Map(key, entry, key_type, 'STANDARD', required)

    def sequence(self, key, node, required, flags):
        items = node.value
        if items and all(is_map(i) for i in items):
            if 'list' not in flags and all('id' in map_keys(i) for i in items):
                entry = self.entry_group(key, items)
                key_type = self.id_key_type(items)
                return Map(key, entry, key_type, 'ID_LIST', required)

            # a bare sequence of mappings: CARL has no group for it, so generate a plain struct
            item_type = self.struct_from_maps(key, items)
            self.notes.append(
                "%s: sequence of mappings with no 'id', generated struct %s "
                "(no per-field validation)" % (key, item_type))
            return Field(key, 'std::vector<%s>' % item_type, required)

        # sequence of scalars or of sequences: wrap so operator<< is reachable by ADL
        wrapper = self.struct_from_scalar_seq(key, items)
        return Field(key, wrapper, required)

    def id_key_type(self, items):
        ids = []
        for i in items:
            for k, v in i.value:
                if k.value == 'id' and is_scalar(v):
                    ids.append(v.value)
        return scalar_cpp_type(ids) if ids else 'int'

    def entry_group(self, key, entry_nodes):
        """the nameless ConfigGroup used as a ConfigMap entry, from the union of sibling keys"""
        order, universal = merged_entry_keys(entry_nodes)
        children = []
        for k, v in order:
            field = self.child(k, v)
            if not universal[k.value]:
                field.required = False
                self.notes.append('%s.%s: absent from some entries, marked optional' % (key, k.value))
            children.append(field)
        return Group(key, pascal(singular(key)) + 'Entry', children, named=False)

    # -- generated structs -------------------------------------------------

    def struct_from_maps(self, key, items):
        name = pascal(singular(key))
        order, universal = merged_entry_keys(items)
        members = []
        for k, v in order:
            tag = explicit_tag(v)
            if tag:
                if tag not in BUILTIN_TYPES:
                    self.tagged_types.append(tag)
                members.append((k.value, tag))
            elif is_scalar(v):
                members.append((k.value, scalar_cpp_type([v.value])))
            elif is_seq(v) and v.value and all(is_map(i) for i in v.value):
                members.append((k.value, 'std::vector<%s>' % self.struct_from_maps(k.value, v.value)))
            elif is_seq(v):
                members.append((k.value, self.vector_type(v.value)))
            elif is_map(v):
                members.append((k.value, self.struct_from_maps(k.value, [v])))
            if not universal[k.value]:
                self.notes.append('%s.%s: absent from some items of the sequence' % (key, k.value))
        self.structs.append(Struct(name, members, 'map'))
        return name

    def struct_from_scalar_seq(self, key, items):
        name = pascal(key)
        self.structs.append(Struct(name, [('items', self.vector_type(items))], 'seq'))
        return name

    def vector_type(self, items):
        if items and all(is_seq(i) for i in items):
            inner = [s.value for i in items for s in i.value if is_scalar(s)]
            return 'std::vector<std::vector<%s>>' % scalar_cpp_type(inner)
        return 'std::vector<%s>' % scalar_cpp_type([i.value for i in items if is_scalar(i)])


# --------------------------------------------------------------------------- emitter

def align(rows):
    """pad the first column of (type, rest) rows"""
    if not rows:
        return []
    width = max(len(t) for t, _ in rows)
    return ['%-*s %s' % (width, t, rest) for t, rest in rows]


class Emitter:
    def __init__(self, namespace, basename, tag_includes=()):
        self.ns, self.basename = namespace, basename
        self.tag_includes = list(tag_includes)

    # -- config header -----------------------------------------------------

    def config_header(self, root, builder):
        guard = '%s_CONFIG_HPP' % self.basename.upper()
        out = ['#ifndef ' + guard, '#define ' + guard, '']
        out += ['#include "%s_extensions.hpp"' % self.basename, '']
        out += ['#include "config_group.hpp"', '#include "config_map.hpp"',
                '#include "config_value.hpp"', '']
        out += ['#include <string>', '']
        out += ['namespace %s' % self.ns, '{', '']
        out += ['using CARL::ConfigGroup;', 'using CARL::ConfigMap;', 'using CARL::ConfigValue;',
                'using CARL::Default;', 'using CARL::MapType;', 'using CARL::Required;', '']

        emitted = []
        self.collect(root, emitted)
        for group in emitted:
            out += self.group_struct(group)
            out += ['']

        out += ['} // namespace %s' % self.ns, '', '#endif // ' + guard, '']
        return '\n'.join(out)

    def collect(self, group, out):
        """post-order, so every type is defined before it is used"""
        for child in group.children:
            if isinstance(child, Group):
                self.collect(child, out)
            elif isinstance(child, Map):
                self.collect(child.entry, out)
        out.append(group)

    def group_struct(self, group):
        lines = ['struct %s : ConfigGroup' % group.type_name, '{']

        rows = []
        for child in group.children:
            if isinstance(child, Field):
                rows.append(('ConfigValue<%s>' % child.cpp_type, self.field_init(child)))
            elif isinstance(child, Group):
                rows.append((child.type_name, '%s;' % child.key))
            elif isinstance(child, Map):
                rows.append((self.map_type(child), self.map_init(child)))
        lines += ['    ' + r for r in align(rows)]

        names = [c.key for c in group.children]
        lines += ['']
        ctor = '    %s()' % group.type_name
        if group.named:
            lines += [ctor, '        : ConfigGroup("%s"%s)' % (
                group.key, '' if group.required else ', Required::NO')]
            lines += ['    {', '        registerEntries(']
        else:
            lines += [ctor, '    {', '        registerEntries(']
        lines += ['            %s,' % n for n in names[:-1]]
        lines += ['            %s' % names[-1]]
        lines += ['        );', '    }']
        lines += ['};']
        return lines

    def field_init(self, field):
        extra = ''
        if field.default is not None:
            extra = ', Default<%s>{%s}' % (field.cpp_type, field.default)
        elif not field.required:
            extra = ', Required::NO'
        return '%s {"%s"%s};' % (field.key, field.key, extra)

    def map_type(self, node):
        if node.key_type == 'int':
            return 'ConfigMap<%s>' % node.entry.type_name
        return 'ConfigMap<%s, %s>' % (node.entry.type_name, node.key_type)

    def map_init(self, node):
        args = ['"%s"' % node.key]
        if node.mode == 'ID_LIST':
            args.append('MapType::ID_LIST')
        if not node.required:
            if node.mode != 'ID_LIST':
                args.append('MapType::STANDARD')
            args.append('Required::NO')
        return '%s {%s};' % (node.key, ', '.join(args))

    # -- extensions header -------------------------------------------------

    def extensions_header(self, builder):
        guard = '%s_EXTENSIONS_HPP' % self.basename.upper()
        tagged = sorted(set(builder.tagged_types))

        out = ['#ifndef ' + guard, '#define ' + guard, '']
        if tagged:
            out += ['// Types named by a !tag in the source YAML: %s.' % ', '.join(tagged),
                    '// Each needs YAML::convert<T>::decode and operator<<(std::ostream&, T const&).',
                    '// The static_asserts at the bottom fail loudly if either is missing.', '']
            if self.tag_includes:
                out += ['#include "%s"' % inc for inc in self.tag_includes] + ['']
            else:
                out += ['// TODO: pass --tag-include to have the declaring headers included here', '']
        out += ['#include "utils/constraints.hpp"', '']
        out += ['#include <yaml-cpp/yaml.h>', '']
        out += ['#include <cstddef>', '#include <ostream>', '#include <string>', '#include <vector>', '']
        out += ['namespace %s' % self.ns, '{', '']

        if builder.structs:
            out += ['// Shapes CARL has no group for: bare sequences of mappings, and sequences of',
                    '// sequences. Generated as plain types with a convert and a stream operator.', '']
            for struct in self.ordered_structs(builder.structs):
                rows = [(m_type, '%s {};' % m_name) for m_name, m_type in struct.members]
                out += ['struct %s' % struct.name, '{']
                out += ['    ' + r for r in align(rows)]
                out += ['};', '']

            out += ['/// reachable by ADL for any vector of a type declared in this namespace',
                    'template <typename T>',
                    'std::ostream& operator <<(std::ostream& os, std::vector<T> const& items)',
                    '{',
                    '    for (std::size_t i = 0; i < items.size(); ++i) {',
                    '        if (i != 0) {',
                    '            os << ", ";',
                    '        }',
                    '',
                    '        os << items[i];',
                    '    }',
                    '',
                    '    return os;',
                    '}', '']

            for struct in self.ordered_structs(builder.structs):
                out += self.struct_printer(struct)
                out += ['']

        out += ['} // namespace %s' % self.ns, '']

        if builder.structs:
            out += ['namespace YAML', '{', '']
            for struct in self.ordered_structs(builder.structs):
                out += self.struct_convert(struct)
                out += ['']
            out += ['} // namespace YAML', '']

        if tagged:
            out += ['namespace %s' % self.ns, '{', '']
            for t in tagged:
                out += ['static_assert(',
                        '    CARL::is_carl_parseable<%s>,' % t,
                        '    "%s: needs YAML::convert<%s> and operator<<(std::ostream&, %s const&)"' % (t, t, t),
                        ');']
            out += ['', '} // namespace %s' % self.ns, '']

        out += ['#endif // ' + guard, '']
        return '\n'.join(out)

    def ordered_structs(self, structs):
        """dependencies first, and drop duplicates by name"""
        by_name, order = {}, []
        for s in structs:
            if s.name not in by_name:
                by_name[s.name] = s
                order.append(s)
        return order

    def struct_printer(self, struct):
        if struct.kind == 'seq':
            member = struct.members[0][0]
            return ['inline std::ostream& operator <<(std::ostream& os, %s const& value)' % struct.name,
                    '{',
                    '    return os << "[" << value.%s << "]";' % member,
                    '}']

        lines = ['inline std::ostream& operator <<(std::ostream& os, %s const& value)' % struct.name,
                 '{']
        parts = []
        for i, (name, m_type) in enumerate(struct.members):
            sep = '' if i == 0 else '", "'
            lead = (', ' if sep else '') + '%s: ' % name
            if m_type.startswith('std::vector'):
                parts.append('    os << "%s[" << value.%s << "]";' % (lead, name))
            else:
                parts.append('    os << "%s" << value.%s;' % (lead, name))
        lines += parts
        lines += ['    return os;', '}']
        return lines

    def struct_convert(self, struct):
        lines = ['template <>',
                 'struct convert<%s::%s>' % (self.ns, struct.name),
                 '{',
                 '    static bool decode(Node const& node, %s::%s& out)' % (self.ns, struct.name),
                 '    {']
        if struct.kind == 'seq':
            member = struct.members[0][0]
            lines += ['        if (!node.IsSequence()) {',
                      '            return false;',
                      '        }',
                      '',
                      '        out.%s = node.as<decltype(out.%s)>();' % (member, member),
                      '        return true;']
        else:
            lines += ['        if (!node.IsMap()) {',
                      '            return false;',
                      '        }',
                      '']
            for name, _ in struct.members:
                lines += ['        if (auto field = node["%s"]) {' % name,
                          '            out.%s = field.as<decltype(out.%s)>();' % (name, name),
                          '        }',
                          '']
            lines += ['        return true;']
        lines += ['    }', '};']
        return lines


# --------------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('yaml', type=Path, help='one complete, annotated config file')
    ap.add_argument('-o', '--out-dir', type=Path, default=Path('.'))
    ap.add_argument('-n', '--namespace', default='config')
    ap.add_argument('-r', '--root', default='Config', help='type name for the root group')
    ap.add_argument('-b', '--basename', default=None, help='output file stem, default = namespace')
    ap.add_argument('--tag-include', action='append', default=[], metavar='HEADER',
                    help='header to include for !tagged types, repeatable')
    args = ap.parse_args()

    text = args.yaml.read_text()
    node = yaml.compose(text)
    if node is None or not is_map(node):
        raise SystemExit('%s: expected a mapping at the document root' % args.yaml)

    builder = Builder(read_directives(text), args.root)
    root = builder.build_root(node)

    basename = args.basename or args.namespace
    emitter = Emitter(args.namespace, basename, args.tag_include)

    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / (basename + '_config.hpp')).write_text(emitter.config_header(root, builder))
    (args.out_dir / (basename + '_extensions.hpp')).write_text(emitter.extensions_header(builder))

    print('wrote %s_config.hpp and %s_extensions.hpp to %s' % (basename, basename, args.out_dir))
    if builder.tagged_types:
        print('tagged types you must provide: %s' % ', '.join(sorted(set(builder.tagged_types))))
    for note in builder.notes:
        print('  note: %s' % note, file=sys.stderr)


if __name__ == '__main__':
    main()
