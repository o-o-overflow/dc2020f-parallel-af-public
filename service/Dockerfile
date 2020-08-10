from ubuntu:18.04

# example:
run apt-get -qq update && apt-get install -qq --no-install-recommends xinetd
# python3-pip, etc.

# Note: anything that goes in public_files must be pre-built and checked in git

RUN mkdir /service

copy parallel-af.tar.gz /
copy sh /service
copy cat /service
copy ls /service
copy rm /service
copy hsh /service
copy gme /service
# /service directory must have the sticky bit set
# Also, create a symlink for them back to the machine, so that they can cat self
RUN chmod 644 /service/* && chmod 1777 /service && cd /service && ln -s manchester self

# The flagbot will periodically echo the flag > this file (flag_path in info.yaml)
run touch /service/flag && chmod 644 /service/flag && echo "OOO{this is a test flag}" > /service/flag

copy service.conf /service.conf
copy banner_fail /banner_fail
copy wrapper /wrapper
copy boot.sh /
RUN chmod 755 /wrapper /boot.sh


# TODO: If possible, change this to be unique (here and in xinetd)
#       Otherwise, specify a unique game_port in info.yml
expose 5000


cmd chmod go-rwx /proc && /boot.sh && xinetd -dontfork -f /service.conf
# ^^ Assuming that ps would be too revealing.
#    AFAIK, this also disables the built-in printf(%n) protection. Alternatively:
#cmd ["/usr/sbin/xinetd", "-dontfork", "-f", "/service.conf"]

# TODO: -filelog /dev/stderr
