# This script generates two headers:
# - the list of the resources to be included in resources.rc
# - the mapping of the resource names and identifiers

import sys
import re
import argparse
import io

parser = argparse.ArgumentParser(description="Process some windows resources.")
parser.add_argument('--files', nargs='+', help='a list of file names')
parser.add_argument('--header-resource-rc', help='the .h file to generate to be included in .rc')
parser.add_argument('--header-resource-mapping', help='the .h file to generate mapping resource names and identifiers')
args = parser.parse_args()

def process_line(f, line):
    rc_re = re.compile('^(\d{1,4}) RCDATA "\.\./assets/(.*)"')
    rc_match = rc_re.match(line)
    if rc_match:
        f.write('{"' + rc_match.groups()[1] + '", ' + rc_match.groups()[0]  + '},\n')
        return True
    return False

identifier = 300

def print_declaration(f, asset, identifier):
    f.write(str(identifier) + " RCDATA " + "../assets/" + asset + "\n")

def print_mapping(f, asset, identifier):
    f.write('{"' + asset + '", ' + str(identifier) + '},\n')

def print_mapping_header(f):
    f.write("#ifndef ION_SIMULATOR_WINDOWS_RESOURCES_H\n")
    f.write("#define ION_SIMULATOR_WINDOWS_RESOURCES_H\n\n")
    f.write("// This file is auto-generated by assets.py\n\n")
    f.write("constexpr struct {const char * identifier; int id; } resourcesIdentifiers[] = {\n")

def print_mapping_footer(f):
    f.write("};\n\n")
    f.write("#endif\n")

def print(files, path, print_header, print_footer, process_asset):
    f = open(path, "w")
    print_header(f)
    identifier = 300
    for asset in files:
        process_asset(f, asset, identifier)
        identifier += 1
    print_footer(f)
    f.close()


if (args.header_resource_rc):
    print(args.files, args.header_resource_rc, lambda f: None, lambda f: None, print_declaration)

if (args.header_resource_mapping):
    print(args.files, args.header_resource_mapping, print_mapping_header, print_mapping_footer, print_mapping)

