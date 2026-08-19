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

# stands in for a type name that is claimed but has no shape of its own, see Builder.build_root
RESERVED = object()

# a YAML key may be anything, a member name may not
CPP_KEYWORDS = frozenset('''
alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t
class compl concept const consteval constexpr constinit const_cast continue co_await co_return
co_yield decltype default delete do double dynamic_cast else enum explicit export extern false float
for friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or
or_eq private protected public register reinterpret_cast requires return short signed sizeof static
static_assert static_cast struct switch template this thread_local throw true try typedef typeid
typename union unsigned using virtual void volatile wchar_t while xor xor_eq
'''.split())

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
    parts = re.split(r'[^0-9A-Za-z]+', name)
    out = ''.join(p[:1].upper() + p[1:] for p in parts if p)
    if not out:
        return 'Unnamed'
    return out if out[0].isalpha() else '_' + out


def identifier(name):
    """a usable C++ member name for a YAML key; the quoted key itself stays untouched"""
    out = re.sub(r'[^0-9A-Za-z_]+', '_', name)
    if not out or out[0].isdigit():
        out = '_' + out
    return out + '_' if out in CPP_KEYWORDS else out


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
    """a ConfigValue<T> member. key is the YAML key, member the C++ name standing for it"""

    def __init__(self, key, cpp_type, required=True, default=None, tagged=False):
        self.key, self.member, self.cpp_type = key, key, cpp_type
        self.required, self.default, self.tagged = required, default, tagged


class Group:
    """a ConfigGroup: a named section, or a nameless map entry / root"""

    def __init__(self, key, type_name, children, required=True, named=True):
        self.key, self.member, self.type_name, self.children = key, key, type_name, children
        self.required, self.named = required, named


class Map:
    """a ConfigMap<Entry, Key>"""

    def __init__(self, key, entry, key_type, mode, required=True):
        self.key, self.member, self.entry = key, key, entry
        self.key_type, self.mode, self.required = key_type, mode, required


class Struct:
    """a plain struct emitted into the extensions header, with convert + operator<<

    members are (C++ member name, YAML key, C++ type) triples
    """

    def __init__(self, name, members, kind, item_type=None):
        self.name, self.members, self.kind = name, members, kind
        self.item_type = item_type


def shape_signature(node):
    """what makes two generated types interchangeable: the shape, never the values behind it"""
    if isinstance(node, Field):
        return ('field', node.key, node.member, node.cpp_type, node.required, node.default)
    if isinstance(node, Map):
        return ('map', node.key, node.member, node.key_type, node.mode, node.required,
                shape_signature(node.entry))
    return ('group', node.key, node.member, node.named, node.required,
            tuple(shape_signature(c) for c in node.children))


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
    """union of keys across sibling mappings, first-seen order, keeping every value node seen for a key

    Every value matters: the C++ type of a key has to cover all of them, not just the first one.
    """
    order, values, in_how_many = [], {}, {}
    for n in nodes:
        seen_here = set()
        for k, v in n.value:
            if k.value not in values:
                order.append(k)
                values[k.value], in_how_many[k.value] = [], 0
            values[k.value].append(v)
            if k.value not in seen_here:
                seen_here.add(k.value)
                in_how_many[k.value] += 1
    universal = {k: c == len(nodes) for k, c in in_how_many.items()}
    return [(k, values[k.value]) for k in order], universal


# --------------------------------------------------------------------------- builder

class Builder:
    def __init__(self, directives, root_name):
        self.directives = directives
        self.root_name = root_name
        self.structs = []          # generated plain structs, in dependency order
        self.tagged_types = []     # types named by !tags, which the user must provide
        self.type_shapes = {}      # generated type name -> the shape it stands for
        self.notes = []

    # -- entry points ------------------------------------------------------

    def build_root(self, node):
        # the root type is named on the command line and keeps that name, so no child may claim it
        self.type_shapes[self.root_name] = RESERVED
        children = self.merged_children(self.root_name, [node], self.root_name)
        return Group('', self.root_name, children, named=False)

    def child(self, key_node, value_nodes, optional=False):
        """one member, from every value node its key was seen with across sibling mappings"""
        key      = key_node.value
        flags    = directives_for(key_node, self.directives)
        required = not optional and 'optional' not in flags and 'default' not in flags
        primary  = value_nodes[0]
        tag      = explicit_tag(primary)

        if tag:
            if tag in BUILTIN_TYPES:
                # plain type override, e.g. radius: !double 2
                default = None
                if 'default' in flags and is_scalar(primary):
                    default = cpp_literal(tag, primary.value)
                return Field(key, tag, required, default)

            self.tagged_types.append(tag)
            if 'default' in flags:
                self.notes.append("%s: !default ignored, %s is a user supplied type" % (key, tag))
            return Field(key, tag, required, None, tagged=True)

        if is_scalar(primary):
            scalars = [v.value for v in value_nodes if is_scalar(v)]
            if len(scalars) != len(value_nodes):
                self.notes.append('%s: not a scalar everywhere, type inferred from the scalar ones' % key)
            cpp     = scalar_cpp_type(scalars)
            default = cpp_literal(cpp, primary.value) if 'default' in flags else None
            return Field(key, cpp, required, default)

        if is_map(primary):
            maps = [v for v in value_nodes if is_map(v)]
            if 'group' not in flags and ('map' in flags or any(looks_like_keyed_map(v) for v in maps)):
                return self.keyed_map(key, maps, required)
            return self.group(key, maps, required)

        if is_seq(primary):
            return self.sequence(key, value_nodes, required, flags)

        raise SystemExit('unsupported node for key %r' % key)

    # -- shapes ------------------------------------------------------------

    def group(self, key, nodes, required, named=True, type_base=None):
        base     = type_base or pascal(key) + 'Config'
        children = self.merged_children(key or self.root_name, nodes, base)
        shape    = ('group', key, named, required, tuple(shape_signature(c) for c in children))
        return Group(key, self.type_name(base, shape), children, required, named)

    def keyed_map(self, key, nodes, required):
        entry_nodes = [v for n in nodes for _, v in n.value]
        keys        = [k for n in nodes for k in map_keys(n)]
        key_type    = 'int' if all(INT_RE.match(k) for k in keys) else 'std::string'
        entry       = self.entry_group(key, [v for v in entry_nodes if is_map(v)])
        return Map(key, entry, key_type, 'STANDARD', required)

    def sequence(self, key, nodes, required, flags):
        items = [i for n in nodes if is_seq(n) for i in n.value]
        if items and all(is_map(i) for i in items):
            if 'list' not in flags and all('id' in map_keys(i) for i in items):
                entry    = self.entry_group(key, items)
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
        return self.group(key, entry_nodes, True, named=False,
                          type_base=pascal(singular(key)) + 'Entry')

    # -- names -------------------------------------------------------------

    def type_name(self, base, shape):
        """one generated type per distinct shape: identical shapes share a name, different ones never do"""
        candidate, suffix = base, 1
        while True:
            known = self.type_shapes.get(candidate)
            if known is None:
                self.type_shapes[candidate] = shape
                if candidate != base:
                    self.notes.append('%s: name already stands for another shape, generated %s'
                                      % (base, candidate))
                return candidate
            if known == shape:
                return candidate
            suffix += 1
            candidate = '%s%d' % (base, suffix)

    def member_names(self, keys, where, reserved):
        """C++ member names for YAML keys: usable as identifiers, and unique within one type"""
        names, used = [], {reserved}
        for key in keys:
            name = identifier(key)
            if name in used:
                suffix = 2
                while '%s%d' % (name, suffix) in used:
                    suffix += 1
                name = '%s%d' % (name, suffix)
            if name != key:
                self.notes.append('%s.%s: not usable as a C++ member name, named %s' % (where, key, name))
            used.add(name)
            names.append(name)
        return names

    def merged_children(self, where, nodes, reserved):
        """the members of a group, merged across sibling mappings, with their C++ names assigned"""
        order, universal = merged_entry_keys(nodes)
        children = []
        for k, values in order:
            optional = not universal[k.value]
            if optional:
                self.notes.append('%s.%s: absent from some entries, marked optional' % (where, k.value))
            children.append(self.child(k, values, optional))

        for child, member in zip(children, self.member_names([c.key for c in children], where, reserved)):
            child.member = member
        return children

    # -- generated structs -------------------------------------------------

    def struct_from_maps(self, key, items):
        base             = pascal(singular(key))
        order, universal = merged_entry_keys(items)
        members          = []
        for k, values in order:
            primary = values[0]
            tag     = explicit_tag(primary)
            if tag:
                if tag not in BUILTIN_TYPES:
                    self.tagged_types.append(tag)
                members.append((k.value, tag))
            elif is_scalar(primary):
                members.append((k.value, scalar_cpp_type([v.value for v in values if is_scalar(v)])))
            elif is_seq(primary):
                inner = [i for v in values if is_seq(v) for i in v.value]
                if inner and all(is_map(i) for i in inner):
                    members.append((k.value, 'std::vector<%s>' % self.struct_from_maps(k.value, inner)))
                else:
                    members.append((k.value, self.vector_type(inner)))
            elif is_map(primary):
                members.append((k.value, self.struct_from_maps(k.value, [v for v in values if is_map(v)])))
            if not universal[k.value]:
                self.notes.append('%s.%s: absent from some items of the sequence' % (key, k.value))

        named = list(zip(self.member_names([m for m, _ in members], key, base),
                         [m for m, _ in members],
                         [t for _, t in members]))
        name = self.type_name(base, ('struct', 'map', tuple(named)))
        self.structs.append(Struct(name, named, 'map'))
        return name

    def struct_from_scalar_seq(self, key, items):
        members = [('items', 'items', self.vector_type(items))]
        name    = self.type_name(pascal(key), ('struct', 'seq', tuple(members)))
        self.structs.append(Struct(name, members, 'seq'))
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
        """post-order, so every type is defined before it is used, and once per type name"""
        for child in group.children:
            if isinstance(child, Group):
                self.collect(child, out)
            elif isinstance(child, Map):
                self.collect(child.entry, out)
        if all(seen.type_name != group.type_name for seen in out):
            out.append(group)

    def group_struct(self, group):
        lines = ['struct %s : ConfigGroup' % group.type_name, '{']

        rows = []
        for child in group.children:
            if isinstance(child, Field):
                rows.append(('ConfigValue<%s>' % child.cpp_type, self.field_init(child)))
            elif isinstance(child, Group):
                rows.append((child.type_name, '%s;' % child.member))
            elif isinstance(child, Map):
                rows.append((self.map_type(child), self.map_init(child)))
        lines += ['    ' + r for r in align(rows)]
        if rows:
            lines += ['']

        ctor = '    %s()' % group.type_name
        if group.named:
            lines += [ctor, '        : ConfigGroup("%s"%s)' % (
                group.key, '' if group.required else ', Required::NO')]
        else:
            lines += [ctor]

        names = [c.member for c in group.children]
        if names:
            lines += ['    {', '        registerEntries(']
            lines += ['            %s,' % n for n in names[:-1]]
            lines += ['            %s' % names[-1]]
            lines += ['        );', '    }']
        else:
            # an empty mapping in the source YAML: a group with nothing to register
            lines += ['    {}']
        lines += ['};']
        return lines

    def field_init(self, field):
        extra = ''
        if field.default is not None:
            extra = ', Default<%s>{%s}' % (field.cpp_type, field.default)
        elif not field.required:
            extra = ', Required::NO'
        return '%s {"%s"%s};' % (field.member, field.key, extra)

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
        return '%s {%s};' % (node.member, ', '.join(args))

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
                rows = [(m_type, '%s {};' % member) for member, _, m_type in struct.members]
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
        for i, (member, key, m_type) in enumerate(struct.members):
            lead = ('' if i == 0 else ', ') + '%s: ' % key
            if m_type.startswith('std::vector'):
                parts.append('    os << "%s[" << value.%s << "]";' % (lead, member))
            else:
                parts.append('    os << "%s" << value.%s;' % (lead, member))
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
            for member, key, _ in struct.members:
                lines += ['        if (auto field = node["%s"]) {' % key,
                          '            out.%s = field.as<decltype(out.%s)>();' % (member, member),
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
