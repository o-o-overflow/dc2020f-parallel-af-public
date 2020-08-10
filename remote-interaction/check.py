#!/usr/bin/env python3

from nclib import netcat
import sys
import socket

original_files = [b"os", b"flag", b"cat", b"sh", b"ls", b"rm", b"self", b"manchester", b"hsh"]

def main():

    host = sys.argv[1]
    port = int(sys.argv[2])

    conn = netcat.Netcat((host, port))

    #result = conn.recvuntil(b'awesome chall\n')
    result = conn.recvuntil(b'debug@R3-S1$ ')

    required_bytes = [b"Booting up IndustrialAutomation OS v. 0.0.2", b"Loading memory", b"Loading instructions", b"Loading drivers", b"Configuring peripherals", b"Boot process complete", b"Running tests", b"Tests complete"]
    for b in required_bytes:
        assert b in result, f"expected {b} in {result}"

    conn.sendline(b"ls")
    result = conn.recvuntil(b'debug@R3-S1$ ')
    for b in original_files:
        assert b in result, f"expected {b} in {result}"

    # Phase 2 tests
    conn.sendline(b"cat ls>z")
    result = conn.recvuntil(b'debug@R3-S1$ ')
    conn.sendline(b"ls")
    result = conn.recvuntil(b'debug@R3-S1$ ')

    required_bytes = original_files.copy()
    required_bytes.append(b'z')
    for b in required_bytes:
        assert b in result, f"expected {b} in {result}"

    conn.sendline(b"rm z")
    result = conn.recvuntil(b'debug@R3-S1$ ')

    conn.sendline(b"ls")
    result = conn.recvuntil(b'debug@R3-S1$ ')
    assert b"z" not in result

    for b in original_files:
        assert b in result, f"expected {b} in {result}"

    conn.sendline(b"rm cat")
    result = conn.recvuntil(b'debug@R3-S1$ ')

    conn.sendline(b"ls")
    result = conn.recvuntil(b'debug@R3-S1$ ')

    for b in original_files:
        assert b in result, f"expected {b} in {result}"

    # Phase 2 tests
    # Now test our writing to a file
    conn.sendline(b"cat ->z")

    our_test_input = b"sephiALD\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x05\x00\x00\x00\x11A\x00\x00\xb0\xff\xff\xff\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x19A\x00\x00\xb0\xff\xff\xff\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00Adam_Doupe123421\n\n\n\x00\x00\x01\x02\x03\x04\x05adam\ntesting\x00testAdamdTestTesttesttesttest"
    
    conn.send(our_test_input)
    conn.shutdown_wr()
    result = conn.recvuntil(b'debug@R3-S1$ ')
    result = conn.recvuntil(b'Goodbye')
    conn.close()

    # make a new connection, check that the file is still there
    conn = netcat.Netcat((host, port))

    result = conn.recvuntil(b'debug@R3-S1$ ')

    required_bytes = [b"Booting up IndustrialAutomation OS v. 0.0.2", b"Loading memory", b"Loading instructions", b"Loading drivers", b"Configuring peripherals", b"Boot process complete", b"Running tests", b"Tests complete"]
    for b in required_bytes:
        assert b in result, f"expected {b} in {result}"

    conn.sendline(b"ls")
    result = conn.recvuntil(b'debug@R3-S1$ ')
    required_bytes = original_files.copy()
    required_bytes.append(b'z')
    for b in required_bytes:
        assert b in result, f"expected {b} in {result}"

    conn.sendline(b"cat z")
    result = conn.recvuntil(b'debug@R3-S1$ ')
    assert our_test_input in result

    conn.sendline(b"rm z")
    result = conn.recvuntil(b'debug@R3-S1$ ')

    conn.sendline(b"ls")
    result = conn.recvuntil(b'debug@R3-S1$ ')
    assert b"z" not in result

    # Test that we can't just cat the flag
    conn.sendline(b"cat flag")
    result = conn.recvuntil(b'debug@R3-S1$ ')
    assert b"OOO" not in result
    
    sys.exit(0)


if __name__ == '__main__':
    main()
    

