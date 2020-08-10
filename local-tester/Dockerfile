# This container is built on top of the patched-service ones
ARG SERVICE=build--this--with--a--build-arg
FROM $SERVICE


# Extra dependencies of these check scripts, if any
run apt-get -qq --no-install-recommends install file netcat python3 python3-pip python3-setuptools
RUN pip3 install wheel
COPY requirements.txt /requirements.txt
RUN pip3 install -r requirements.txt


RUN mkdir /test
copy check_machine.sh /test
copy check_os.py /test
copy manchester /test
copy os /test
copy sh /test
copy rm /test
copy cat /test
copy ls /test
copy output-flag /test
COPY programs /test/programs
