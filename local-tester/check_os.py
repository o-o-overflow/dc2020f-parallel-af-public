#!/usr/bin/env python3
import os
import shutil
import socket
import sys
import time

from nclib import netcat

MANCHESTER_DIFF_LIMIT = 100

original_files = [b"os", b"flag", b"cat", b"sh", b"ls", b"rm", b"self", b"manchester", b"hsh"]

def bin_diff(byte_arr1, byte_arr2):
    byte_diff = 0
    for (x,y) in zip(byte_arr1, byte_arr2):
        if x != y:
            byte_diff += 1

    return byte_diff + abs(len(byte_arr1) - len(byte_arr2))

def our_bin_diff(f1, f2):
    with open(f1, 'rb') as file_1:
        with open(f2, 'rb') as file_2:
            diff = bin_diff(file_1.read(), file_2.read())
            print(f"Diff between {f1} and {f2}: {diff}")
            return diff

def main():
    os.system("/boot.sh; xinetd -f /service.conf")

    # check that the diff for manchester is less than 100 bytes
    diff = our_bin_diff("/test/manchester", "/service/manchester")
    print(f"manchester diff: {diff}")
    if diff > MANCHESTER_DIFF_LIMIT:
        print(f"PUBLIC: Error, diff in manchester was {diff} but limit is {MANCHESTER_DIFF_LIMIT}")
        sys.exit(-1)

    time.sleep(2)

    # Check that they didn't nerf the OS
    shutil.copyfile("/test/cat", "/service/y")

    conn = netcat.Netcat(("localhost", 5000))
    result = conn.recvuntil(b'debug@R3-S1$ ')

    # This test is for phase 2
    conn.sendline(b"y y>z");
    result = conn.recvuntil(b'debug@R3-S1$ ')
    assert os.path.exists("/service/z"), result
    assert(our_bin_diff("/test/cat", "/service/z") == 0)

    # Check that we can cat self to get the exact output
    conn.sendline(b"y self")
    result = conn.recvuntil(b'debug@R3-S1$ ')
    # print(f"result={result}\n")
    with open('/service/manchester', 'rb') as manchester:
        diff = bin_diff(result[:-13], manchester.read())
        print(f"difference between manchesters {diff}")
        assert(diff == 0)

    # Check that we can cat os to get the exact output
    conn.sendline(b"cat os")
    result = conn.recvuntil(b'debug@R3-S1$ ')
    # print(f"result={result}\n")
    with open('/service/os', 'rb') as os_file:
        diff = bin_diff(result[:-13], os_file.read())
        print(f"difference between os {diff}")
        assert(diff == 0)

    # Phase 2
    os.remove("/service/z")
    os.remove("/service/y")

    shutil.copyfile("/test/ls", "/service/y")
    conn.sendline(b"y")
    result = conn.recvuntil(b'debug@R3-S1$ ')

    required_bytes = original_files.copy()
    required_bytes.append(b'y')
    for b in required_bytes:
        assert b in result, f"expected {b} in {result}"

    conn.close()

    # now, copy over an OS that just prints the flag

    shutil.copyfile("/test/output-flag", "/service/os")
    conn = netcat.Netcat(("localhost", 5000))
    result = conn.recv_all()

    with open('/service/flag', 'rb') as flag:
        assert result == flag.read()
    

if __name__ == '__main__':
    main()

