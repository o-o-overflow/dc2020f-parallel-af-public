#!/usr/bin/env python3
import argparse
import itertools
import logging
import struct

l = logging.getLogger("compiler")

HEADER_SIZE = 16
DESTINATION_TO_UPDATE_SIZE = 8
EXTERNAL_SIZE = 264
EXPORT_SIZE = 260

def main(files_to_strip):
    l.debug(f"files to strip: {' '.join(files_to_strip)}")

    i = itertools.count(0)

    mapping = dict()
    for file_name in files_to_strip:
        with open(file_name, 'r+b') as f:
            content = f.read()
            num_constant, num_to_fix, num_external_ref, num_exported = struct.unpack_from('<xxxxxxxxHHHH', content)

            l.debug(f"num_constant={num_constant} num_to_fix={num_to_fix} num_external_ref={num_external_ref} num_exported={num_exported}")

            start_external = HEADER_SIZE + (num_constant * DESTINATION_TO_UPDATE_SIZE) + (num_to_fix * DESTINATION_TO_UPDATE_SIZE)

            start_exported = start_external + (num_external_ref * EXTERNAL_SIZE)
            end_exported = start_exported + (num_exported * EXPORT_SIZE)

            external_format = '<IBxxx256s'
            new_external = b""
            for inst_num, flags, name in struct.iter_unpack(external_format, content[start_external:start_exported]):
                new_name = None
                if name in mapping:
                    new_name = mapping[name]
                else:
                    new_name = f"{next(i)}".encode()
                    mapping[name] = new_name
                new_external += struct.pack(external_format, inst_num, flags, new_name)
                l.debug(f"inst_num={inst_num} flags={flags} name={name} new_name={new_name}")

            assert(len(new_external) == len(content[start_external:start_exported]))

            export_format = '<I256s'
            new_export = b""
            for inst_num, name in struct.iter_unpack(export_format, content[start_exported:end_exported]):
                new_name = None
                if name.startswith(b'_main_arg_0_export') or name.startswith(b'_main_return_location_export'):
                    new_name = name
                elif name in mapping:
                    new_name = mapping[name]
                else:
                    new_name = f"{next(i)}".encode()
                    mapping[name] = new_name
                new_export += struct.pack(export_format, inst_num, new_name)
                l.debug(f"inst_num={inst_num} name={name} new_name={new_name}")

            assert(len(new_export) == len(content[start_exported:end_exported]))
            new_content = content[:start_external] + new_external + new_export + content[end_exported:]
            assert(len(new_content) == len(content))

            f.seek(0)
            f.write(new_content)

        pass

    pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog="strip")
    parser.add_argument("--debug", action="store_true", help="Enable debugging")
    parser.add_argument("--file", nargs="+", help="files to strip", required=True)

    args = parser.parse_args()

    if args.debug:
        logging.basicConfig(level=logging.DEBUG)

    main(args.file)
    
