#!/bin/sh -e

cd /service
tar -xvf /parallel-af.tar.gz manchester --no-same-owner -C ./ || true
tar -xvf /parallel-af.tar.gz os --no-same-owner -C ./ || true
cd -

chmod 755 /service/manchester
chmod 644 /service/os
